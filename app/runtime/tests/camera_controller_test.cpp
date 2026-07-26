#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QVector>

#include <atomic>

using namespace desktop_app::v2;

namespace {

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
        const quint64 id = delivery.fetch_add(1);
        frame.pixelFormat = CameraPixelFormat::Mono8;
        frame.width = 2;
        frame.height = 1;
        frame.rowBytes = 2;
        frame.bitDepth = 8;
        frame.deliveryId = id;
        frame.monotonicTimestampNs = static_cast<qint64>(id * 100);
        frame.bytes = QByteArray::fromHex("1122");
        return CameraFrameResult::Frame;
    }

    std::atomic_bool failStart = false;
    std::atomic_bool failFrames = false;
    std::atomic_bool rejectConfiguration = false;
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
                    && controller.bitDepth() == QStringLiteral("12-bit")
                    && controller.exposureMs() == QStringLiteral("10")
                    && controller.readoutMode() == QStringLiteral("Fast"),
                "Controller must cache the queued open projection and factual settings.");

    ok &= check(controller.applyExposureMs(4.5)
                    && !controller.applyBitDepth(8)
                    && waitForIdle(controller, busySpy)
                    && controller.exposureMs() == QStringLiteral("4.5")
                    && fake->applyConfigurationCalls == 1,
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
    const int previewSignalsBeforeThrottleWindow = previewSpy.count();
    const int framesBeforeThrottleWindow = frameSpy.count();
    const quint64 deliveryBeforeThrottleWindow = controller.latestDeliveryId();
    QEventLoop previewThrottleWait;
    QTimer::singleShot(600, &previewThrottleWait, &QEventLoop::quit);
    previewThrottleWait.exec();
    const int previewSignalsInThrottleWindow =
        previewSpy.count() - previewSignalsBeforeThrottleWindow;
    const int framesInThrottleWindow = frameSpy.count() - framesBeforeThrottleWindow;
    ok &= check(controller.latestDeliveryId() > deliveryBeforeThrottleWindow
                    && previewSignalsInThrottleWindow >= 2
                    && previewSignalsInThrottleWindow <= 3
                    && framesInThrottleWindow > previewSignalsInThrottleWindow,
                "High-rate acquisition must outpace bounded preview URL publication "
                "while pending latest frames continue to reach the preview.");
    const QImage preview = provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(!preview.isNull() && preview.size() == QSize(2, 1),
                "Controller frames must immediately feed the preview provider.");

    bool blankPreviewWhileStreaming = false;
    QObject::connect(&controller, &CameraController::previewSourceChanged,
                     &controller, [&] {
        blankPreviewWhileStreaming |= controller.streaming()
            && controller.previewSource().isEmpty();
    });
    const int previewSignalsBeforeBurst = previewSpy.count();
    const int frameSignalsBeforeBurst = frameSpy.count();
    constexpr quint64 firstBurstDeliveryId = 100000;
    constexpr int burstFrameCount = 128;
    QVector<quint64> burstFrameIds;
    QObject::connect(&controller, &CameraController::frameReady, service,
                     [&burstFrameIds](const CameraFrame &frame) {
        if (frame.deliveryId >= firstBurstDeliveryId
            && frame.deliveryId < firstBurstDeliveryId + burstFrameCount) {
            burstFrameIds.append(frame.deliveryId);
        }
    }, Qt::DirectConnection);
    const bool burstQueued = QMetaObject::invokeMethod(service, [service, fake] {
        fake->delivery = firstBurstDeliveryId;
        for (int index = 0; index < burstFrameCount; ++index) {
            CameraFrame frame;
            frame.pixelFormat = CameraPixelFormat::Mono8;
            frame.width = 2;
            frame.height = 1;
            frame.rowBytes = 2;
            frame.bitDepth = 8;
            frame.deliveryId = firstBurstDeliveryId + index;
            frame.monotonicTimestampNs = static_cast<qint64>(frame.deliveryId * 100);
            frame.bytes = index == burstFrameCount - 1
                ? QByteArray::fromHex("00ff")
                : QByteArray::fromHex("1122");
            emit service->frameReady(std::move(frame));
        }
        fake->delivery = firstBurstDeliveryId + burstFrameCount;
    }, Qt::BlockingQueuedConnection);
    QCoreApplication::processEvents();
    const QImage burstPreview =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    const int burstFrameSignals = frameSpy.count() - frameSignalsBeforeBurst;
    const int burstPreviewSignals = previewSpy.count() - previewSignalsBeforeBurst;
    bool burstFrameOrderPreserved = burstFrameIds.size() == burstFrameCount;
    for (int index = 0; burstFrameOrderPreserved && index < burstFrameCount; ++index)
        burstFrameOrderPreserved = burstFrameIds.at(index)
            == firstBurstDeliveryId + index;
    const bool burstCorrect = burstQueued
        && burstFrameSignals == burstFrameCount
        && burstFrameOrderPreserved
        && controller.latestDeliveryId() == firstBurstDeliveryId + burstFrameCount - 1
        && burstPreviewSignals <= 1
        && !burstPreview.isNull()
        && burstPreview.constScanLine(0)[0] == 0
        && burstPreview.constScanLine(0)[1] == 255
        && !controller.previewSource().isEmpty() && !blankPreviewWhileStreaming;
    ok &= check(burstCorrect,
                "A producer burst must retain every ordered frameReady signal while "
                "coalescing preview delivery to the newest nonblank frame.");

    const QString previewBeforeLut = controller.previewSource();
    controller.setPreviewLutRange(20, 30);
    const QImage lutPreview =
        provider.requestImage(QStringLiteral("frame"), nullptr, {});
    ok &= check(controller.previewLutMinimum() == 20
                    && controller.previewLutMaximum() == 30
                    && controller.previewSource() != previewBeforeLut
                    && lutPreview.constScanLine(0)[0] == 0
                    && lutPreview.constScanLine(0)[1] == 255,
                "Preview LUT changes must republish only the rendered camera image.");

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
    return ok ? 0 : 1;
}
