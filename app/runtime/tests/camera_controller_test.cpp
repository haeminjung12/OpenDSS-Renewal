#include "v2/camera/camera_controller.h"
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
        ++startCalls;
        if (!startError.isEmpty()) {
            *error = startError;
            return false;
        }
        return true;
    }
    bool stop(QString *) override
    {
        ++stopCalls;
        return true;
    }
    bool close(QString *) override
    {
        ++closeCalls;
        return true;
    }
    CameraFrameResult latestFrame(CameraFrame &, QString *) override
    {
        return CameraFrameResult::NoFrame;
    }

    QString openError;
    QString startError;
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

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ApplicationStateStore store;
    int publications = 0;
    QObject::connect(&store, &ApplicationStateStore::changed,
                     [&publications]() { ++publications; });
    auto device = std::make_unique<FakeCameraDevice>();
    FakeCameraDevice *fake = device.get();
    CameraService service(std::move(device), store);
    CameraController controller(service);

    ok &= check(publications == 2 && fake->openCalls == 1
                    && controller.cameraStatus() == QStringLiteral("Ready")
                    && controller.deviceId() == QStringLiteral("fake-camera")
                    && controller.error().isEmpty(),
                "Controller construction must project the service's startup open.");
    ok &= check(controller.start() && publications == 3 && fake->startCalls == 1
                    && controller.streaming()
                    && store.snapshot().camera.status == CameraStatus::Streaming,
                "Start must delegate to the sole service state owner.");
    ok &= check(controller.stop() && publications == 4 && fake->stopCalls == 1
                    && !controller.streaming()
                    && controller.cameraStatus() == QStringLiteral("Ready"),
                "Stop must delegate and project Ready.");

    fake->startError = QStringLiteral("Camera stream could not be armed.");
    ok &= check(!controller.start() && publications == 5 && fake->closeCalls == 1
                    && controller.cameraStatus() == QStringLiteral("Faulted")
                    && controller.error()
                           == QStringLiteral("Camera stream could not be armed."),
                "Controller must project the service's factual start fault.");
    fake->startError.clear();
    ok &= check(controller.recover() && publications == 7 && fake->closeCalls == 2
                    && fake->openCalls == 2
                    && controller.cameraStatus() == QStringLiteral("Ready")
                    && controller.error().isEmpty(),
                "Recover must reuse the same service and restore Ready.");
    ok &= check(store.snapshot().camera.status == service.state().status
                    && store.snapshot().camera.deviceId == service.state().deviceId,
                "ApplicationStateStore publication must come solely from CameraService.");

    return ok ? 0 : 1;
}
