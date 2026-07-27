#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <atomic>

using namespace desktop_app::v2;

namespace {

struct CapturedFrame
{
    quint64 deliveryId = 0;
    qint64 timestampNs = 0;
};

struct PreviewSample
{
    qint64 publishedAtMs = 0;
    qint64 acceptedAtMs = -1;
    quint64 deliveryId = 0;
    quint64 minimumExpectedDeliveryId = 0;
    uchar firstPixel = 0;
};

class FakeCameraDevice final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("fake-camera"); }
    bool open(QString *) override { ++openCalls; return true; }
    bool start(QString *error) override
    {
        ++startCalls;
        if (failStart.load()) {
            *error = QStringLiteral("Camera stream could not be armed.");
            return false;
        }
        return true;
    }
    bool stop(QString *) override { ++stopCalls; return true; }
    bool close(QString *) override { ++closeCalls; return true; }
    CameraConfigurationSupport configurationSupport(QString *error) const override
    {
        error->clear();
        return CameraConfigurationSupport::Supported;
    }
    bool readConfiguration(CameraAppliedSettings &settings, QString *error) override
    {
        settings = appliedSettings;
        error->clear();
        return true;
    }
    CameraConfigurationResult applyConfiguration(
        const CameraAppliedSettings &requested,
        CameraAppliedSettings &applied,
        QString *error) override
    {
        ++applyConfigurationCalls;
        if (configurationDelayMs.load() > 0)
            QThread::msleep(static_cast<unsigned long>(configurationDelayMs.load()));
        if (rejectConfiguration.load()) {
            *error = QStringLiteral("Camera configuration was rejected.");
            return CameraConfigurationResult::Rejected;
        }
        appliedSettings = requested;
        applied = appliedSettings;
        error->clear();
        return CameraConfigurationResult::Applied;
    }
    CameraFrameResult latestFrame(CameraFrame &frame, QString *error) override
    {
        if (failFrames.load()) {
            *error = QStringLiteral("Camera transport failed.");
            return CameraFrameResult::Error;
        }
        if (manualFramesOnly.load())
            return CameraFrameResult::NoFrame;
        const quint64 id = delivery.fetch_add(1);
        frame.pixelFormat = CameraPixelFormat::Mono8;
        frame.width = 2;
        frame.height = 1;
        frame.rowBytes = 2;
        frame.bitDepth = 8;
        frame.deliveryId = id;
        frame.monotonicTimestampNs = static_cast<qint64>(id * 100);
        frame.bytes = movingFrames.load()
            ? QByteArray(2, static_cast<char>(id & 1 ? 0xff : 0x00))
            : QByteArray::fromHex("1122");
        return CameraFrameResult::Frame;
    }

    std::atomic_bool failStart = false;
    std::atomic_bool failFrames = false;
    std::atomic_bool rejectConfiguration = false;
    std::atomic_bool movingFrames = false;
    std::atomic_bool manualFramesOnly = false;
    std::atomic_int configurationDelayMs = 0;
    std::atomic<quint64> delivery = 1;
    CameraAppliedSettings appliedSettings = [] {
        CameraAppliedSettings settings;
        settings.width = 2304;
        settings.height = 2304;
        settings.bitDepth = 12;
        settings.pixelType = CameraPixelType::Mono16;
        settings.exposureMs = 10.0;
        settings.readoutMode = CameraReadoutMode::Fast;
        return settings;
    }();
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
    int applyConfigurationCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool waitForIdle(CameraController &controller, QSignalSpy &busySpy)
{
    while (controller.busy()) {
        if (!busySpy.wait(1000))
            return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ApplicationStateStore store;
    auto device = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *fake = device.get();
    auto *service = new CameraService(std::move(device), store);
    CameraPreviewImageProvider provider;
    CameraController controller(*service, provider);
    QThread worker;
    service->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished, service, &QObject::deleteLater);
    worker.start();

    QSignalSpy busySpy(&controller, &CameraController::busyChanged);
    QSignalSpy previewSpy(&controller, &CameraController::previewSourceChanged);
    QSignalSpy frameSpy(&controller, &CameraController::frameReady);

    ok &= check(controller.open() && !controller.open(),
                "Lifecycle bools must report request acceptance, not completion.");
    ok &= check(waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.deviceId() == QStringLiteral("fake-camera")
                    && controller.configurationAvailable()
                    && controller.resolution() == QStringLiteral("2304 x 2304")
                    && controller.resolutionPresets().size() == 25
                    && controller.resolutionPresets().at(20)
                           == QStringLiteral("Custom")
                    && controller.resolutionPresets().at(21)
                           == QStringLiteral("512 x 128")
                    && controller.resolutionPresetIndex() == 0
                    && controller.bitDepth() == QStringLiteral("8-bit")
                    && controller.exposureMs() == QStringLiteral("10")
                    && controller.readoutMode() == QStringLiteral("Fast")
                    && fake->applyConfigurationCalls == 1,
                "A new live Camera must apply and publish the 8-bit default.");

    ok &= check(controller.applyExposureMs(4.5)
                    && !controller.applyBitDepth(8)
                    && waitForIdle(controller, busySpy)
                    && controller.exposureMs() == QStringLiteral("4.5")
                    && fake->applyConfigurationCalls == 2,
                "Controller must serialize configuration and publish factual readback.");

    ok &= check(controller.selectCustomResolution()
                    && controller.resolution() == QStringLiteral("Custom"),
                "Custom selection may reveal the existing draft fields.");
    fake->rejectConfiguration = true;
    ok &= check(controller.applyResolution(1536, 1024)
                    && waitForIdle(controller, busySpy)
                    && controller.error()
                           == QStringLiteral("Camera configuration was rejected.")
                    && controller.customWidth() == QStringLiteral("2304")
                    && controller.customHeight() == QStringLiteral("2304")
                    && controller.resolution() == QStringLiteral("Custom"),
                "Rejected custom edits must retain factual applied dimensions and draft mode.");
    fake->rejectConfiguration = false;
    ok &= check(controller.selectResolutionPreset(10)
                    && waitForIdle(controller, busySpy)
                    && controller.resolution() == QStringLiteral("1152 x 1152")
                    && controller.resolutionPresetIndex() == 10,
                "A successful preset apply must leave Custom draft mode.");

    CameraAppliedSettings profileSettings = fake->appliedSettings;
    profileSettings.width = 512;
    profileSettings.height = 128;
    profileSettings.exposureMs = 3.25;
    ok &= check(controller.applyProfileSettings(profileSettings, 12, 220)
                    && !controller.busy()
                    && controller.resolution() == QStringLiteral("512 x 128")
                    && controller.exposureMs() == QStringLiteral("3.25")
                    && controller.previewLutMinimum() == 12
                    && controller.previewLutMaximum() == 220,
                "Setup Profile camera apply must return only after accepted hardware readback.");
    fake->rejectConfiguration = true;
    ok &= check(!controller.applyProfileSettings(profileSettings, 30, 200)
                    && !controller.busy()
                    && controller.error()
                           == QStringLiteral("Camera configuration was rejected.")
                    && controller.previewLutMinimum() == 12
                    && controller.previewLutMaximum() == 220,
                "Rejected profile camera settings must return failure and retain the prior LUT.");
    fake->rejectConfiguration = false;
    fake->configurationDelayMs = 30;
    profileSettings.exposureMs = 4.25;
    ok &= check(!controller.applyProfileSettings(profileSettings, 40, 180, 1)
                    && controller.error()
                           == QStringLiteral("Timed out waiting for the camera to apply the Setup Profile.")
                    && waitForIdle(controller, busySpy)
                    && controller.error()
                           == QStringLiteral("Timed out waiting for the camera to apply the Setup Profile.")
                    && controller.previewLutMinimum() == 12
                    && controller.previewLutMaximum() == 220,
                "A timed-out profile apply must retain its factual timeout and prior LUT after late completion.");
    fake->configurationDelayMs = 0;

    ok &= check(controller.start() && waitForIdle(controller, busySpy)
                    && controller.streaming(),
                "Controller must queue start and project Streaming.");
    if (controller.previewSource().isEmpty())
        previewSpy.wait(1000);
    ok &= check(controller.previewSource().startsWith(
                    QStringLiteral("image://camera-preview/frame?r=")),
                "Controller must publish a revisioned camera-preview URL.");
    bool blankPreviewWhileStreaming = false;
    QElapsedTimer stressClock;
    QMutex acceptedFramesMutex;
    QHash<quint64, qint64> acceptedAtMs;
    QVector<CapturedFrame> capturedFrames;
    QVector<PreviewSample> previewSamples;
    std::atomic<quint64> latestOfferedId = 0;
    quint64 minimumNextDeliveryId = 0;
    int previewRevisionsInFlight = 0;
    int maximumPreviewRevisionsInFlight = 0;
    fake->manualFramesOnly = true;
    controller.acknowledgePreviewReady(controller.previewSource());
    QCoreApplication::processEvents();
    controller.acknowledgePreviewReady(controller.previewSource());
    QCoreApplication::processEvents();
    fake->movingFrames = true;
    frameSpy.clear();
    stressClock.start();
    const QMetaObject::Connection frameConnection = QObject::connect(
        &controller, &CameraController::frameReady, service,
        [&](const CameraFrame &frame) {
            QMutexLocker locker(&acceptedFramesMutex);
            acceptedAtMs.insert(frame.deliveryId, stressClock.elapsed());
            capturedFrames.append({frame.deliveryId, frame.monotonicTimestampNs});
            latestOfferedId = frame.deliveryId;
        }, Qt::DirectConnection);
    const QMetaObject::Connection previewConnection = QObject::connect(
        &controller, &CameraController::previewSourceChanged, &controller, [&] {
            const QString source = controller.previewSource();
            blankPreviewWhileStreaming |= controller.streaming() && source.isEmpty();
            if (source.isEmpty())
                return;
            ++previewRevisionsInFlight;
            maximumPreviewRevisionsInFlight =
                std::max(maximumPreviewRevisionsInFlight,
                         previewRevisionsInFlight);
            const quint64 deliveryId = controller.latestDeliveryId();
            qint64 acceptedAt = -1;
            {
                QMutexLocker locker(&acceptedFramesMutex);
                acceptedAt = acceptedAtMs.value(deliveryId, -1);
            }
            const QImage image =
                provider.requestImage(QStringLiteral("frame"), nullptr, {});
            previewSamples.append({stressClock.elapsed(), acceptedAt, deliveryId,
                                   minimumNextDeliveryId,
                                   static_cast<uchar>(image.isNull()
                                       ? 0 : image.constScanLine(0)[0])});
            QTimer::singleShot(20, &controller, [&, source] {
                minimumNextDeliveryId = latestOfferedId.load();
                --previewRevisionsInFlight;
                controller.acknowledgePreviewReady(source);
            });
        });

    QTimer *stressProducer = nullptr;
    const bool producerStarted = QMetaObject::invokeMethod(service,
        [&] {
            stressProducer = new QTimer(service);
            stressProducer->setTimerType(Qt::PreciseTimer);
            stressProducer->setInterval(1);
            QObject::connect(stressProducer, &QTimer::timeout, service,
                [service, fake] {
                    const quint64 id = fake->delivery.fetch_add(1);
                    CameraFrame frame;
                    frame.pixelFormat = CameraPixelFormat::Mono8;
                    frame.width = 2;
                    frame.height = 1;
                    frame.rowBytes = 2;
                    frame.bitDepth = 8;
                    frame.deliveryId = id;
                    frame.monotonicTimestampNs =
                        static_cast<qint64>(id * 1'000'000);
                    frame.bytes = QByteArray(
                        2, static_cast<char>(id & 1 ? 0xff : 0x00));
                    emit service->frameReady(std::move(frame));
                });
            stressProducer->start();
        }, Qt::BlockingQueuedConnection);
    QEventLoop previewStressWait;
    QTimer::singleShot(1000, &previewStressWait, &QEventLoop::quit);
    previewStressWait.exec();
    const bool producerStopped = QMetaObject::invokeMethod(service,
        [frameConnection, stressProducer] {
            stressProducer->stop();
            QObject::disconnect(frameConnection);
            stressProducer->deleteLater();
        }, Qt::BlockingQueuedConnection);
    const int stressFrameSignals = frameSpy.count();
    const quint64 finalOfferedId = latestOfferedId.load();
    QElapsedTimer finalCatchupClock;
    finalCatchupClock.start();
    while (controller.latestDeliveryId() != finalOfferedId
           && finalCatchupClock.elapsed() < 70) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    const qint64 finalCatchupMs = finalCatchupClock.elapsed();
    const bool finalReached = controller.latestDeliveryId() == finalOfferedId;
    const qint64 stressDurationMs = stressClock.elapsed();
    QObject::disconnect(previewConnection);
    fake->manualFramesOnly = false;
    fake->movingFrames = false;
    QEventLoop staticPreviewWait;
    QTimer::singleShot(300, &staticPreviewWait, &QEventLoop::quit);
    staticPreviewWait.exec();

    bool framesOrdered = stressFrameSignals == capturedFrames.size()
        && capturedFrames.size() >= 500;
    for (qsizetype index = 1; framesOrdered && index < capturedFrames.size(); ++index) {
        framesOrdered = capturedFrames.at(index - 1).deliveryId
                < capturedFrames.at(index).deliveryId
            && capturedFrames.at(index - 1).timestampNs
                < capturedFrames.at(index).timestampNs;
    }
    bool previewsFresh = previewSamples.size() >= 25;
    qint64 maximumFrameAgeMs = 0;
    for (qsizetype index = 0; previewsFresh && index < previewSamples.size(); ++index) {
        const PreviewSample &sample = previewSamples.at(index);
        const qint64 frameAgeMs = sample.publishedAtMs - sample.acceptedAtMs;
        maximumFrameAgeMs = std::max(maximumFrameAgeMs, frameAgeMs);
        previewsFresh = sample.acceptedAtMs >= 0
            && frameAgeMs <= 70
            && sample.deliveryId >= sample.minimumExpectedDeliveryId
            && sample.firstPixel == (sample.deliveryId & 1 ? 255 : 0);
        if (index > 0) {
            const PreviewSample &previous = previewSamples.at(index - 1);
            previewsFresh = previewsFresh
                && sample.deliveryId > previous.deliveryId;
        }
    }
    const double displayedFps = previewSamples.size() * 1000.0
        / std::max<qint64>(1, stressDurationMs);
    ok &= check(producerStarted && producerStopped
                    && framesOrdered && previewsFresh
                    && maximumPreviewRevisionsInFlight == 1
                    && finalReached
                    && finalCatchupMs <= 70
                    && displayedFps >= 25.0
                    && !controller.previewSource().isEmpty()
                    && !blankPreviewWhileStreaming,
                "Completion-driven preview delivery must retain ordered acquisition, "
                "one in-flight revision, newest-frame freshness, and final catch-up.");

    fake->manualFramesOnly = true;
    const QString gatedPreviewSource = controller.previewSource();
    previewSpy.clear();
    const quint64 olderPendingId = fake->delivery.fetch_add(1);
    const quint64 newestPendingId = fake->delivery.fetch_add(1);
    QMetaObject::invokeMethod(service, [=] {
        CameraFrame frame;
        frame.pixelFormat = CameraPixelFormat::Mono8;
        frame.width = 2;
        frame.height = 1;
        frame.rowBytes = 2;
        frame.bitDepth = 8;
        frame.deliveryId = olderPendingId;
        frame.monotonicTimestampNs =
            static_cast<qint64>(olderPendingId * 100);
        frame.bytes = QByteArray::fromHex("0010");
        emit service->frameReady(std::move(frame));

        frame.deliveryId = newestPendingId;
        frame.monotonicTimestampNs =
            static_cast<qint64>(newestPendingId * 100);
        frame.bytes = QByteArray::fromHex("141e");
        emit service->frameReady(std::move(frame));
    }, Qt::BlockingQueuedConnection);
    QCoreApplication::processEvents();
    const QImage providerBeforeDeferredLut =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    controller.setPreviewLutRange(10, 40);
    controller.setPreviewLutRange(20, 30);
    QCoreApplication::processEvents();
    const QImage providerWhileLutDeferred =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    const bool lutDeferredWhileInFlight =
        previewSpy.isEmpty()
        && controller.previewSource() == gatedPreviewSource
        && providerWhileLutDeferred == providerBeforeDeferredLut;
    controller.acknowledgePreviewReady(gatedPreviewSource);
    const bool lutPublishedAfterAck = previewSpy.wait(1000);
    QCoreApplication::processEvents();
    const QImage lutPreview =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(lutDeferredWhileInFlight
                    && lutPublishedAfterAck
                    && previewSpy.count() == 1
                    && controller.previewLutMinimum() == 20
                    && controller.previewLutMaximum() == 30
                    && controller.previewSource() != gatedPreviewSource
                    && controller.latestDeliveryId() == newestPendingId
                    && lutPreview.constScanLine(0)[0] == 0
                    && lutPreview.constScanLine(0)[1] == 255,
                "LUT and newest-frame updates must wait for the exact in-flight "
                "source acknowledgement, then publish exactly one revision.");
    controller.acknowledgePreviewReady(controller.previewSource());

    previewSpy.clear();
    const QString previewBeforeIdleLut = controller.previewSource();
    controller.setPreviewLutRange(30, 60);
    const QString idleLutSource = controller.previewSource();
    const QImage providerAfterIdleLut =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(previewSpy.count() == 1
                    && idleLutSource != previewBeforeIdleLut
                    && providerAfterIdleLut.constScanLine(0)[0] == 0
                    && providerAfterIdleLut.constScanLine(0)[1] == 0,
                "An idle LUT edit must update the provider and publish immediately.");

    previewSpy.clear();
    controller.setPreviewLutRange(40, 80);
    controller.setPreviewLutRange(30, 60);
    const QImage providerWhileRevertedLutDeferred =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    controller.acknowledgePreviewReady(idleLutSource);
    QCoreApplication::processEvents();
    const QImage providerAfterRevertedLutAck =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(previewSpy.isEmpty()
                    && controller.previewSource() == idleLutSource
                    && providerWhileRevertedLutDeferred == providerAfterIdleLut
                    && providerAfterRevertedLutAck == providerAfterIdleLut,
                "A gated LUT edit reverted to the applied range must not mutate "
                "the provider or publish another revision after acknowledgement.");
    fake->manualFramesOnly = false;

    ok &= check(controller.applyExposureMs(6.0)
                    && !controller.applyBitDepth(8)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.exposureMs() == QStringLiteral("6")
                    && controller.error().isEmpty()
                    && fake->stopCalls == 1 && fake->startCalls == 2,
                "Streaming configuration must reach the serialized service transaction.");

    fake->rejectConfiguration = true;
    ok &= check(controller.applyExposureMs(9.0)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.exposureMs() == QStringLiteral("6")
                    && controller.error()
                           == QStringLiteral("Camera configuration was rejected.")
                    && fake->stopCalls == 2 && fake->startCalls == 3,
                "Rejected streaming configuration must retain facts and project its error.");
    fake->rejectConfiguration = false;

    ok &= check(controller.selectCustomResolution()
                    && controller.resolution() == QStringLiteral("Custom")
                    && controller.applyResolution(1536, 1024)
                    && waitForIdle(controller, busySpy)
                    && controller.streaming()
                    && controller.resolution() == QStringLiteral("Custom")
                    && controller.customWidth() == QStringLiteral("1536")
                    && controller.customHeight() == QStringLiteral("1024")
                    && fake->stopCalls == 3 && fake->startCalls == 4,
                "Custom resolution must be selectable and factually applied while streaming.");

    ok &= check(controller.stop() && waitForIdle(controller, busySpy)
                    && !controller.streaming()
                    && controller.cameraStatus() == QStringLiteral("Connected"),
                "Stop must retain the Connected projection.");

    ok &= check(controller.start() && waitForIdle(controller, busySpy),
                "Controller must restart streaming before polling-fault coverage.");
    QSignalSpy stateSpy(&controller, &CameraController::stateChanged);
    fake->failFrames = true;
    if (controller.cameraStatus() != QStringLiteral("Unavailable"))
        stateSpy.wait(1000);
    ok &= check(controller.cameraStatus() == QStringLiteral("Unavailable")
                    && controller.error() == QStringLiteral("Camera transport failed.")
                    && controller.previewSource().isEmpty()
                    && fake->stopCalls == 5 && fake->closeCalls == 1,
                "A polling fault must project closed Unavailable truth through the facade.");
    QEventLoop unavailablePreviewWait;
    QTimer::singleShot(300, &unavailablePreviewWait, &QEventLoop::quit);
    unavailablePreviewWait.exec();
    ok &= check(controller.previewSource().isEmpty(),
                "A queued preview must not reappear after Unavailable cleanup.");
    fake->failFrames = false;
    ok &= check(controller.recover() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.error().isEmpty(),
                "Polling-fault recovery must remain an explicit controller action.");

    fake->failStart = true;
    ok &= check(controller.start() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Unavailable")
                    && controller.error()
                           == QStringLiteral("Camera stream could not be armed."),
                "Worker faults must project through the GUI facade.");
    fake->failStart = false;
    ok &= check(controller.recover() && waitForIdle(controller, busySpy)
                    && controller.cameraStatus() == QStringLiteral("Connected")
                    && controller.error().isEmpty(),
                "Recover must clear the factual fault without replacing the service.");

    controller.close();
    waitForIdle(controller, busySpy);
    worker.quit();
    worker.wait();

    ApplicationStateStore retryStore;
    auto retryDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *retryFake = retryDevice.get();
    retryFake->rejectConfiguration = true;
    auto *retryService =
        new CameraService(std::move(retryDevice), retryStore);
    CameraPreviewImageProvider retryProvider;
    CameraController retryController(*retryService, retryProvider);
    QThread retryWorker;
    retryService->moveToThread(&retryWorker);
    QObject::connect(&retryWorker, &QThread::finished,
                     retryService, &QObject::deleteLater);
    retryWorker.start();
    QSignalSpy retryBusySpy(&retryController,
                            &CameraController::busyChanged);

    ok &= check(retryController.open()
                    && waitForIdle(retryController, retryBusySpy)
                    && retryController.bitDepth() == QStringLiteral("12-bit"),
                "A failed initial 8-bit apply must retain factual readback.");
    retryFake->rejectConfiguration = false;
    ok &= check(retryController.applyBitDepth(12)
                    && waitForIdle(retryController, retryBusySpy)
                    && retryController.bitDepth() == QStringLiteral("12-bit"),
                "A successful explicit depth must resolve default initialization.");
    const int appliesBeforeRecovery = retryFake->applyConfigurationCalls;
    ok &= check(retryController.recover()
                    && waitForIdle(retryController, retryBusySpy)
                    && retryController.bitDepth() == QStringLiteral("12-bit")
                    && retryFake->applyConfigurationCalls
                        == appliesBeforeRecovery,
                "Recovery must not overwrite an accepted explicit depth.");

    retryController.close();
    waitForIdle(retryController, retryBusySpy);
    retryWorker.quit();
    retryWorker.wait();
    return ok ? 0 : 1;
}
