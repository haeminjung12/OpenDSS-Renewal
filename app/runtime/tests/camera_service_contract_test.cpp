#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QSignalSpy>
#include <QThread>

#include <atomic>

using namespace desktop_app::v2;

namespace {

class FakeCameraDevice final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("fake-camera"); }

    bool open(QString *error) override
    {
        ++openCalls;
        if (!openError.isEmpty()) {
            *error = openError;
            return false;
        }
        return true;
    }

    bool start(QString *error) override
    {
        ++startCalls;
        if (!startError.isEmpty()) {
            *error = startError;
            return false;
        }
        return true;
    }

    bool stop(QString *error) override
    {
        ++stopCalls;
        if (!stopError.isEmpty()) {
            *error = stopError;
            return false;
        }
        return true;
    }

    bool close(QString *error) override
    {
        ++closeCalls;
        if (!closeError.isEmpty()) {
            *error = closeError;
            return false;
        }
        return true;
    }

    CameraConfigurationSupport configurationSupport(QString *error) const override
    {
        *error = configurationSupportError;
        return configurationSupportResult;
    }

    bool readConfiguration(CameraAppliedSettings &settings, QString *error) override
    {
        ++readConfigurationCalls;
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
        if (configurationResult != CameraConfigurationResult::Applied) {
            *error = configurationError;
            return configurationResult;
        }
        appliedSettings = requested;
        applied = appliedSettings;
        error->clear();
        return CameraConfigurationResult::Applied;
    }

    CameraFrameResult drainFrames(std::vector<CameraFrame> &output,
                                  QString *error) override
    {
        ++drainFrameCalls;
        if (failFrames.load()) {
            *error = QStringLiteral("Camera transport failed.");
            return CameraFrameResult::Error;
        }
        QMutexLocker locker(&mutex);
        if (frames.isEmpty()) {
            error->clear();
            return CameraFrameResult::NoFrame;
        }
        output.reserve(output.size() + frames.size());
        while (!frames.isEmpty())
            output.push_back(frames.dequeue());
        error->clear();
        return CameraFrameResult::Frame;
    }

    void enqueue(CameraFrame frame)
    {
        QMutexLocker locker(&mutex);
        frames.enqueue(std::move(frame));
    }

    QMutex mutex;
    QQueue<CameraFrame> frames;
    QString openError;
    QString startError;
    QString stopError;
    QString closeError;
    QString configurationError;
    CameraAppliedSettings appliedSettings = [] {
        CameraAppliedSettings settings;
        settings.width = 1024;
        settings.height = 1024;
        settings.bitDepth = 12;
        settings.pixelType = CameraPixelType::Mono16;
        settings.exposureMs = 10.0;
        settings.readoutMode = CameraReadoutMode::Fast;
        return settings;
    }();
    CameraConfigurationResult configurationResult = CameraConfigurationResult::Applied;
    CameraConfigurationSupport configurationSupportResult =
        CameraConfigurationSupport::Supported;
    QString configurationSupportError;
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
    int readConfigurationCalls = 0;
    int applyConfigurationCalls = 0;
    std::atomic_bool failFrames = false;
    std::atomic_int drainFrameCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

CameraFrame frame(quint64 deliveryId, qint64 timestamp)
{
    CameraFrame result;
    result.pixelFormat = CameraPixelFormat::Mono8;
    result.width = 2;
    result.height = 1;
    result.rowBytes = 2;
    result.bitDepth = 8;
    result.deliveryId = deliveryId;
    result.monotonicTimestampNs = timestamp;
    result.bytes = QByteArray::fromHex("1122");
    return result;
}

QVariantList runCommand(CameraService *service, const char *method, QSignalSpy &spy)
{
    const int before = spy.count();
    QMetaObject::invokeMethod(service, method, Qt::QueuedConnection);
    if (spy.count() == before)
        spy.wait(1000);
    return spy.count() > before ? spy.at(before) : QVariantList{};
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
    QThread worker;
    service->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished, service, &QObject::deleteLater);
    worker.start();

    QSignalSpy commands(service, &CameraService::commandFinished);
    QSignalSpy frames(service, &CameraService::frameReady);
    QSignalSpy frameErrors(service, &CameraService::frameError);

    ok &= check(store.snapshot().camera.status == CameraStatus::Unavailable,
                "Construction must publish Unavailable.");
    QVariantList result = runCommand(service, "open", commands);
    ok &= check(result.value(0).toBool() && fake->openCalls == 1
                    && fake->readConfigurationCalls == 1
                    && service->state().status == CameraStatus::Ready
                    && service->state().configurationAvailable
                    && service->state().appliedSettings.bitDepth == 12,
                "Queued open must publish Ready with factual camera configuration.");

    CameraAppliedSettings requested = service->state().appliedSettings;
    requested.exposureMs = 4.5;
    const int beforeApply = commands.count();
    QMetaObject::invokeMethod(service, "applyConfiguration", Qt::QueuedConnection,
                              Q_ARG(desktop_app::v2::CameraAppliedSettings, requested));
    if (commands.count() == beforeApply)
        commands.wait(1000);
    ok &= check(commands.count() > beforeApply
                    && commands.at(beforeApply).value(0).toBool()
                    && fake->applyConfigurationCalls == 1
                    && service->state().appliedSettings.exposureMs == 4.5,
                "Queued configuration must publish only the factual applied result.");

    fake->enqueue(frame(7, 700));
    result = runCommand(service, "start", commands);
    ok &= check(result.value(0).toBool() && fake->startCalls == 1,
                "Queued start must be accepted by the worker.");

    requested.exposureMs = 6.0;
    const int beforeStreamingApply = commands.count();
    QMetaObject::invokeMethod(service, "applyConfiguration", Qt::QueuedConnection,
                              Q_ARG(desktop_app::v2::CameraAppliedSettings, requested));
    if (commands.count() == beforeStreamingApply)
        commands.wait(1000);
    ok &= check(commands.count() > beforeStreamingApply
                    && commands.at(beforeStreamingApply).value(0).toBool()
                    && fake->applyConfigurationCalls == 2
                    && fake->stopCalls == 1 && fake->startCalls == 2
                    && service->state().status == CameraStatus::Streaming
                    && service->state().appliedSettings.exposureMs == 6.0,
                "Streaming configuration must stop, apply, and restart transactionally.");

    fake->configurationResult = CameraConfigurationResult::Rejected;
    fake->configurationError = QStringLiteral("Exposure is unsupported.");
    requested.exposureMs = 9.0;
    const int beforeStreamingRejection = commands.count();
    QMetaObject::invokeMethod(service, "applyConfiguration", Qt::QueuedConnection,
                              Q_ARG(desktop_app::v2::CameraAppliedSettings, requested));
    if (commands.count() == beforeStreamingRejection)
        commands.wait(1000);
    ok &= check(commands.count() > beforeStreamingRejection
                    && !commands.at(beforeStreamingRejection).value(0).toBool()
                    && commands.at(beforeStreamingRejection).value(1).toString()
                           == QStringLiteral("Exposure is unsupported.")
                    && fake->stopCalls == 2 && fake->startCalls == 3
                    && service->state().status == CameraStatus::Streaming
                    && service->state().appliedSettings.exposureMs == 6.0,
                "Rejected streaming apply must restart with retained factual settings.");
    fake->configurationResult = CameraConfigurationResult::Applied;
    fake->configurationError.clear();
    if (frames.isEmpty())
        frames.wait(1000);
    ok &= check(!frames.isEmpty(), "Worker-affine polling must emit a frame.");
    if (!frames.isEmpty()) {
        const CameraFrame delivered = qvariant_cast<CameraFrame>(frames.first().first());
        ok &= check(delivered.bytes == QByteArray::fromHex("1122")
                        && delivered.deliveryId == 7,
                    "Frame delivery must preserve a deep-owned CameraFrame.");
    }

    fake->enqueue(frame(8, 800));
    fake->enqueue(frame(9, 900));
    fake->enqueue(frame(10, 1000));
    if (frames.count() < 4)
        frames.wait(1000);
    CameraFrame delivered8;
    CameraFrame delivered9;
    CameraFrame delivered10;
    if (frames.count() >= 4) {
        delivered8 = qvariant_cast<CameraFrame>(frames.at(1).first());
        delivered9 = qvariant_cast<CameraFrame>(frames.at(2).first());
        delivered10 = qvariant_cast<CameraFrame>(frames.at(3).first());
    }
    ok &= check(frames.count() >= 4
                    && delivered8.deliveryId == 8
                    && delivered9.deliveryId == 9
                    && delivered10.deliveryId == 10
                    && delivered9.monotonicTimestampNs
                        - delivered8.monotonicTimestampNs == 100
                    && delivered10.monotonicTimestampNs
                        - delivered9.monotonicTimestampNs == 100,
                "One acquisition wake must publish every available burst frame "
                "in order without changing source timestamp spacing.");

    fake->enqueue(frame(6, 1001));
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    ok &= check(!frameErrors.isEmpty()
                    && frameErrors.takeFirst().first().toString().contains("older"),
                "A regressed delivery identifier must be rejected factually.");

    fake->enqueue(frame(11, 999));
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    ok &= check(!frameErrors.isEmpty()
                    && frameErrors.takeFirst().first().toString().contains("timestamp"),
                "A regressed timestamp must be rejected factually.");

    fake->failFrames = true;
    if (frameErrors.isEmpty())
        frameErrors.wait(1000);
    const int callsAtFault = fake->drainFrameCalls.load();
    const int errorsAtFault = frameErrors.count();
    QThread::msleep(80);
    QCoreApplication::processEvents();
    ok &= check(service->state().status == CameraStatus::Faulted
                    && service->state().fault == QStringLiteral("Camera transport failed.")
                    && fake->stopCalls == 3 && fake->closeCalls == 1
                    && fake->drainFrameCalls.load() == callsAtFault
                    && errorsAtFault == 1 && frameErrors.count() == errorsAtFault,
                "A polling error must close the stream, publish Faulted, and stop error flooding.");

    fake->failFrames = false;
    result = runCommand(service, "recover", commands);
    ok &= check(result.value(0).toBool() && fake->closeCalls == 2
                    && fake->openCalls == 2
                    && service->state().status == CameraStatus::Ready,
                "Recovery after a polling fault must remain explicit.");

    result = runCommand(service, "start", commands);
    ok &= check(result.value(0).toBool(), "Recovered camera must stream again.");
    result = runCommand(service, "stop", commands);
    ok &= check(result.value(0).toBool() && fake->stopCalls == 4
                    && service->state().status == CameraStatus::Ready,
                "Queued stop must stop polling and publish Ready.");

    const CameraAppliedSettings retained = service->state().appliedSettings;
    fake->configurationResult = CameraConfigurationResult::Rejected;
    fake->configurationError = QStringLiteral("Exposure is unsupported.");
    requested.exposureMs = 9.0;
    const int beforeRejectedApply = commands.count();
    QMetaObject::invokeMethod(service, "applyConfiguration", Qt::QueuedConnection,
                              Q_ARG(desktop_app::v2::CameraAppliedSettings, requested));
    if (commands.count() == beforeRejectedApply)
        commands.wait(1000);
    ok &= check(commands.count() > beforeRejectedApply
                    && !commands.at(beforeRejectedApply).value(0).toBool()
                    && service->state().status == CameraStatus::Ready
                    && service->state().appliedSettings.exposureMs == retained.exposureMs,
                "A rejected apply must retain the last authoritative applied settings.");

    fake->configurationResult = CameraConfigurationResult::StateUnknown;
    fake->configurationError =
        QStringLiteral("Apply failed. Rollback also failed.");
    const int beforeUnknownApply = commands.count();
    QMetaObject::invokeMethod(service, "applyConfiguration", Qt::QueuedConnection,
                              Q_ARG(desktop_app::v2::CameraAppliedSettings, requested));
    if (commands.count() == beforeUnknownApply)
        commands.wait(1000);
    ok &= check(commands.count() > beforeUnknownApply
                    && !commands.at(beforeUnknownApply).value(0).toBool()
                    && service->state().status == CameraStatus::Faulted
                    && !service->state().configurationAvailable,
                "Rollback failure must fault the camera and clear configuration availability.");

    fake->configurationResult = CameraConfigurationResult::Applied;
    result = runCommand(service, "recover", commands);
    ok &= check(result.value(0).toBool()
                    && service->state().status == CameraStatus::Ready,
                "Camera must recover after unknown configuration state.");

    fake->startError = QStringLiteral("Camera stream could not be armed.");
    result = runCommand(service, "start", commands);
    ok &= check(!result.value(0).toBool()
                    && result.value(1).toString()
                           == QStringLiteral("Camera stream could not be armed.")
                    && service->state().status == CameraStatus::Faulted,
                "Lifecycle faults must remain factual.");

    runCommand(service, "close", commands);
    worker.quit();
    worker.wait();

    ApplicationStateStore lifecycleOnlyStore;
    auto lifecycleOnlyDevice = std::make_unique<FakeCameraDevice>();
    lifecycleOnlyDevice->configurationSupportResult =
        CameraConfigurationSupport::Unsupported;
    CameraService lifecycleOnlyService(std::move(lifecycleOnlyDevice),
                                       lifecycleOnlyStore);
    lifecycleOnlyService.open();
    ok &= check(lifecycleOnlyService.state().status == CameraStatus::Ready
                    && !lifecycleOnlyService.state().configurationAvailable,
                "Lifecycle-only camera consumers must remain Ready without claiming configuration.");
    lifecycleOnlyService.close();

    ApplicationStateStore probeFaultStore;
    auto probeFaultDevice = std::make_unique<FakeCameraDevice>();
    probeFaultDevice->configurationSupportResult =
        CameraConfigurationSupport::Error;
    probeFaultDevice->configurationSupportError =
        QStringLiteral("Camera capability probe failed.");
    CameraService probeFaultService(std::move(probeFaultDevice), probeFaultStore);
    probeFaultService.open();
    ok &= check(probeFaultService.state().status == CameraStatus::Faulted
                    && !probeFaultService.state().configurationAvailable
                    && probeFaultService.state().fault
                           == QStringLiteral("Camera capability probe failed."),
                "A capability probe error must fault and close the camera.");

    ApplicationStateStore restartFaultStore;
    auto restartFaultDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *restartFaultFake = restartFaultDevice.get();
    CameraService restartFaultService(std::move(restartFaultDevice),
                                      restartFaultStore);
    restartFaultService.open();
    restartFaultService.start();
    restartFaultFake->startError =
        QStringLiteral("Camera stream could not restart.");
    CameraAppliedSettings restartRequest =
        restartFaultService.state().appliedSettings;
    restartRequest.exposureMs = 3.0;
    QSignalSpy restartCommands(&restartFaultService,
                               &CameraService::commandFinished);
    restartFaultService.applyConfiguration(restartRequest);
    ok &= check(!restartCommands.isEmpty()
                    && !restartCommands.last().value(0).toBool()
                    && restartCommands.last().value(1).toString()
                           == QStringLiteral("Camera stream could not restart.")
                    && restartFaultService.state().status == CameraStatus::Faulted
                    && !restartFaultService.state().configurationAvailable
                    && restartFaultFake->stopCalls == 1
                    && restartFaultFake->startCalls == 2
                    && restartFaultFake->closeCalls == 1,
                "Restart failure after apply must close and publish factual Faulted state.");

    return ok ? 0 : 1;
}

