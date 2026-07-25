#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>

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
        opened = true;
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
        opened = false;
        return true;
    }

    CameraFrameResult latestFrame(CameraFrame &output, QString *error) override
    {
        if (frameResult == CameraFrameResult::Error) {
            *error = frameError;
            return frameResult;
        }
        if (frameResult == CameraFrameResult::NoFrame) {
            error->clear();
            return frameResult;
        }
        output = frame;
        return frameResult;
    }

    QString openError;
    QString startError;
    QString stopError;
    QString closeError;
    QString frameError;
    CameraFrameResult frameResult = CameraFrameResult::Frame;
    CameraFrame frame;
    bool opened = false;
    int openCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int closeCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

CameraFrame validFrame()
{
    CameraFrame frame;
    frame.pixelFormat = CameraPixelFormat::Mono8;
    frame.width = 2;
    frame.height = 1;
    frame.rowBytes = 2;
    frame.bitDepth = 8;
    frame.deliveryId = 7;
    frame.monotonicTimestampNs = 700;
    frame.bytes = QByteArray::fromHex("1122");
    return frame;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;

    ApplicationStateStore store;
    int publications = 0;
    QObject::connect(&store, &ApplicationStateStore::changed,
                     [&publications]() { ++publications; });
    auto device = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *fake = device.get();
    fake->frame = validFrame();
    CameraService camera(std::move(device), store);

    ok &= check(publications == 1
                    && store.snapshot().camera.status == CameraStatus::Unavailable,
                "Construction must publish one authoritative Unavailable state.");
    ok &= check(camera.open(&error) && publications == 2 && fake->openCalls == 1
                    && camera.state().status == CameraStatus::Ready
                    && store.snapshot().camera.deviceId == QStringLiteral("fake-camera"),
                "Open must publish the ready device once.");
    ok &= check(!camera.open(&error) && publications == 2 && fake->openCalls == 1,
                "Repeated open must not touch the device or publish state.");
    ok &= check(camera.start(&error) && publications == 3 && fake->startCalls == 1
                    && store.snapshot().camera.status == CameraStatus::Streaming,
                "Start must publish Streaming once.");

    fake->frameResult = CameraFrameResult::NoFrame;
    error = QStringLiteral("stale");
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.isEmpty()
                    && publications == 3
                    && camera.state().status == CameraStatus::Streaming,
                "NoFrame must be non-faulting and clear stale errors.");

    fake->frameResult = CameraFrameResult::Error;
    fake->frameError = QStringLiteral("Camera transfer failed.");
    ok &= check(!camera.latestOwnedFrame(&error).has_value()
                    && error == QStringLiteral("Camera transfer failed.")
                    && publications == 3,
                "Frame Error must remain distinct and factual without republishing state.");

    fake->frameResult = CameraFrameResult::Frame;
    auto owned = camera.latestOwnedFrame(&error);
    ok &= check(owned.has_value() && owned->bytes == QByteArray::fromHex("1122"),
                "Frame must be returned.");
    fake->frame.bytes[0] = static_cast<char>(0x7f);
    ok &= check(owned->bytes == QByteArray::fromHex("1122"),
                "Returned frame bytes must be deeply owned.");

    fake->frame.deliveryId = 6;
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.contains("older"),
                "A regressed delivery identifier must be rejected.");
    fake->frame.deliveryId = 8;
    fake->frame.monotonicTimestampNs = 699;
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.contains("timestamp"),
                "A regressed timestamp must be rejected.");

    ok &= check(camera.stop(&error) && publications == 4 && fake->stopCalls == 1
                    && camera.state().status == CameraStatus::Ready,
                "Stop must publish Ready once.");
    ok &= check(camera.close(&error) && publications == 5 && fake->closeCalls == 1
                    && camera.state().status == CameraStatus::Unavailable,
                "Close must reset ownership and publish Unavailable.");
    ok &= check(camera.open(&error) && fake->openCalls == 2,
                "A closed camera must open again through the same service.");
    ok &= check(camera.recover(&error) && fake->closeCalls == 2 && fake->openCalls == 3
                    && camera.state().status == CameraStatus::Ready,
                "Recover must close and reopen without replacing CameraService.");

    ApplicationStateStore openFailureStore;
    auto openFailureDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *openFailureFake = openFailureDevice.get();
    openFailureFake->openError = QStringLiteral("Camera cable is disconnected.");
    CameraService openFailureCamera(std::move(openFailureDevice), openFailureStore);
    ok &= check(!openFailureCamera.open(&error) && openFailureFake->closeCalls == 1
                    && openFailureCamera.state().status == CameraStatus::Faulted
                    && error == QStringLiteral("Camera cable is disconnected."),
                "Open failure must clean up and publish the factual fault.");
    openFailureFake->openError.clear();
    ok &= check(openFailureCamera.recover(&error) && openFailureFake->closeCalls == 2
                    && openFailureFake->openCalls == 2
                    && openFailureCamera.state().status == CameraStatus::Ready,
                "A faulted service must recover without replacement.");

    ApplicationStateStore startFailureStore;
    auto startFailureDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *startFailureFake = startFailureDevice.get();
    CameraService startFailureCamera(std::move(startFailureDevice), startFailureStore);
    ok &= check(startFailureCamera.open(&error), "Start-failure setup must open.");
    startFailureFake->startError = QStringLiteral("Camera stream could not be armed.");
    ok &= check(!startFailureCamera.start(&error) && startFailureFake->closeCalls == 1
                    && startFailureCamera.state().status == CameraStatus::Faulted,
                "Start failure must clean up and publish Faulted.");

    ApplicationStateStore stopFailureStore;
    auto stopFailureDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *stopFailureFake = stopFailureDevice.get();
    CameraService stopFailureCamera(std::move(stopFailureDevice), stopFailureStore);
    ok &= check(stopFailureCamera.open(&error) && stopFailureCamera.start(&error),
                "Stop-failure setup must stream.");
    stopFailureFake->stopError = QStringLiteral("Camera stream could not be stopped.");
    ok &= check(!stopFailureCamera.stop(&error) && stopFailureFake->closeCalls == 1
                    && stopFailureCamera.state().status == CameraStatus::Faulted,
                "Stop failure must clean up and publish Faulted.");

    ApplicationStateStore closeFailureStore;
    auto closeFailureDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *closeFailureFake = closeFailureDevice.get();
    CameraService closeFailureCamera(std::move(closeFailureDevice), closeFailureStore);
    ok &= check(closeFailureCamera.open(&error), "Close-failure setup must open.");
    closeFailureFake->closeError = QStringLiteral("Camera could not be released.");
    ok &= check(!closeFailureCamera.close(&error)
                    && closeFailureCamera.state().status == CameraStatus::Faulted
                    && error == QStringLiteral("Camera could not be released."),
                "Close failure must publish the factual fault.");

    return ok ? 0 : 1;
}
