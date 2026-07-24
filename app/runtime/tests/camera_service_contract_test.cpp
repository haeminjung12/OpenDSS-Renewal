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
        return true;
    }

    bool start(QString *error) override
    {
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

    bool latestFrame(CameraFrame &output, QString *error) override
    {
        if (!frameError.isEmpty()) {
            *error = frameError;
            return false;
        }
        output = frame;
        return true;
    }

    QString openError;
    QString startError;
    QString stopError;
    QString frameError;
    CameraFrame frame;
    int openCalls = 0;
    int stopCalls = 0;
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

    ApplicationStateStore store;
    auto device = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *fake = device.get();
    fake->frame = validFrame();
    CameraService camera(std::move(device), store);

    ok &= check(store.snapshot().camera.status == CameraStatus::Unavailable,
                "Camera must initially publish Unavailable.");
    QString error;
    ok &= check(camera.open(&error), "Camera open should succeed.");
    ok &= check(camera.state().status == CameraStatus::Ready
                    && store.snapshot().camera.deviceId == QStringLiteral("fake-camera"),
                "Open must publish the ready device.");
    ok &= check(!camera.open(&error) && fake->openCalls == 1
                    && camera.state().status == CameraStatus::Ready,
                "Repeated open must be rejected without touching the device or state.");
    ok &= check(camera.start(&error), "Camera start should succeed.");
    ok &= check(store.snapshot().camera.status == CameraStatus::Streaming,
                "Start must publish Streaming.");
    ok &= check(!camera.open(&error) && fake->openCalls == 1
                    && camera.state().status == CameraStatus::Streaming,
                "Open while streaming must be rejected without touching the device or state.");

    auto owned = camera.latestOwnedFrame(&error);
    ok &= check(owned.has_value() && owned->bytes == QByteArray::fromHex("1122"),
                "Latest frame should be returned.");
    fake->frame.bytes[0] = static_cast<char>(0x7f);
    ok &= check(owned->bytes == QByteArray::fromHex("1122"),
                "Published frame bytes must be deeply owned.");

    fake->frame.deliveryId = 6;
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.contains("older"),
                "A regressed delivery identifier must be rejected.");
    fake->frame.deliveryId = 8;
    fake->frame.monotonicTimestampNs = 699;
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.contains("timestamp"),
                "A regressed monotonic timestamp must be rejected.");

    fake->frame.deliveryId = 8;
    fake->frame.monotonicTimestampNs = 800;
    fake->frameError = QStringLiteral("No current frame is available.");
    ok &= check(!camera.latestOwnedFrame(&error).has_value()
                    && error == QStringLiteral("No current frame is available."),
                "Frame acquisition must preserve a factual device error.");
    fake->frameError.clear();

    ok &= check(camera.stop(&error), "Camera stop should succeed.");
    ok &= check(store.snapshot().camera.status == CameraStatus::Ready,
                "Stop must publish Ready.");
    ok &= check(!camera.latestOwnedFrame(&error).has_value() && error.contains("not streaming"),
                "Frames must not be provided while stopped.");

    ApplicationStateStore failedStore;
    auto failedDevice = std::make_unique<FakeCameraDevice>();
    failedDevice->openError = QStringLiteral("Camera cable is disconnected.");
    CameraService failedCamera(std::move(failedDevice), failedStore);
    ok &= check(!failedCamera.open(&error)
                    && failedStore.snapshot().camera.status == CameraStatus::Faulted
                    && failedStore.snapshot().camera.fault
                           == QStringLiteral("Camera cable is disconnected."),
                "Open failure must publish the factual fault.");

    ApplicationStateStore startStore;
    auto startDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *startFake = startDevice.get();
    CameraService startCamera(std::move(startDevice), startStore);
    ok &= check(startCamera.open(&error), "Start-failure setup should open.");
    startFake->startError = QStringLiteral("Camera stream could not be armed.");
    ok &= check(!startCamera.start(&error)
                    && startStore.snapshot().camera.status == CameraStatus::Faulted
                    && error == QStringLiteral("Camera stream could not be armed."),
                "Start failure must publish the factual fault.");

    ApplicationStateStore stopStore;
    auto stopDevice = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *stopFake = stopDevice.get();
    CameraService stopCamera(std::move(stopDevice), stopStore);
    ok &= check(stopCamera.open(&error) && stopCamera.start(&error),
                "Stop-failure setup should stream.");
    stopFake->stopError = QStringLiteral("Camera stream could not be stopped.");
    ok &= check(!stopCamera.stop(&error) && stopFake->stopCalls == 1
                    && stopStore.snapshot().camera.status == CameraStatus::Faulted
                    && stopStore.snapshot().camera.fault
                           == QStringLiteral("Camera stream could not be stopped."),
                "Stop failure must publish the factual fault.");

    return ok ? 0 : 1;
}
