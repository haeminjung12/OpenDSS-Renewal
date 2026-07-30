#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/sequence/capture_workflow_controller.h"
#include "v2/state/application_state_store.h"
#include "detection/droplet_detector.h"

#include <QCoreApplication>
#include <QDebug>

#include <memory>

using namespace desktop_app::v2;

namespace {

class IdleCamera final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("idle-camera"); }
    bool open(QString *) override { return true; }
    bool start(QString *) override { return true; }
    bool stop(QString *) override { return true; }
    bool close(QString *) override { return true; }
    CameraFrameResult latestFrame(CameraFrame &, QString *) override
    {
        return CameraFrameResult::NoFrame;
    }
};

class IdleDetector final : public IDropletDetector
{
public:
    void reset() override {}
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat &) override { return {}; }
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    ApplicationStateStore stateStore;
    OperationCoordinator operations;
    CameraService cameraService(std::make_unique<IdleCamera>(), stateStore);
    CameraPreviewImageProvider previewProvider;
    CameraController cameraController(cameraService, previewProvider);
    IdleDetector detector;
    sequence::CaptureWorkflowController controller(
        cameraService, cameraController, operations, detector, [] { return 0; },
        [] { return QJsonObject{}; }, QStringLiteral("2"));

    bool ok = check(controller.captureStartAvailable(),
                    "Capture starts must initially be available.");
    auto live = operations.acquire(OperationKind::LiveSorting,
                                   ResourceLock::Camera);
    ok &= check(live.acquired() && !controller.captureStartAvailable(),
                "A production Live operation must globally disable capture starts.");
    ok &= check(!controller.startSequence()
                    && controller.sequenceError()
                           == QStringLiteral("Another operation is active.")
                    && !controller.startDataset()
                    && controller.datasetError()
                           == QStringLiteral("Another operation is active."),
                "Both production capture entry points must recheck the global operation gate.");
    live.lease.release();
    ok &= check(controller.captureStartAvailable(),
                "Capture starts must recover after the owning operation releases its lease.");
    return ok ? 0 : 1;
}
