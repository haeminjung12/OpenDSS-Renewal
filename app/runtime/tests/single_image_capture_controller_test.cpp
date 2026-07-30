#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/camera/single_image_capture_controller.h"
#include "v2/camera/single_image_capture_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>

#include <atomic>
#include <functional>

using namespace desktop_app::v2;

namespace {

class FixedCameraDevice final : public ICameraDevice
{
public:
    QString deviceId() const override { return QStringLiteral("fixed-camera"); }
    bool open(QString *) override { return true; }
    bool start(QString *) override { return true; }
    bool stop(QString *) override { return true; }
    bool close(QString *) override { return true; }
    CameraFrameResult latestFrame(CameraFrame &frame, QString *error) override
    {
        if (!available.load()) {
            error->clear();
            return CameraFrameResult::NoFrame;
        }
        const quint64 id = delivery.load();
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

    std::atomic_bool available = true;
    std::atomic<quint64> delivery = 1;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool waitFor(const std::function<bool()> &predicate, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    QCoreApplication::processEvents();
    return predicate();
}

bool waitForIdle(CameraController &controller)
{
    return waitFor([&controller]() { return !controller.busy(); }, 1000);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ApplicationStateStore store;
    OperationCoordinator operations;
    auto device = std::make_unique<FixedCameraDevice>();
    FixedCameraDevice *fake = device.get();
    auto *cameraService = new CameraService(std::move(device), store);
    CameraPreviewImageProvider provider;
    CameraController camera(*cameraService, provider);
    SingleImageCaptureService saveService;
    SingleImageCaptureController capture(saveService, camera, operations);
    QThread worker;
    cameraService->moveToThread(&worker);
    QObject::connect(&worker, &QThread::finished,
                     cameraService, &QObject::deleteLater);
    worker.start();

    ok &= check(camera.open() && waitForIdle(camera)
                    && camera.start() && waitForIdle(camera)
                    && waitFor([&camera]() { return camera.hasFrame(); }, 1000),
                "Camera fixture must reach Streaming with one current frame.");

    QTemporaryDir documents;
    const QString defaultFolder =
        QDir(documents.path()).filePath(QStringLiteral("OpenDropletSortingSuite/Images"));
    ok &= check(capture.initializeDefaultOutputFolder(documents.path())
                    && capture.outputFolder().toLocalFile() == defaultFolder
                    && QFileInfo(defaultFolder).isDir() && capture.error().isEmpty()
                    && capture.canCapture(),
                "Production default initialization must create the canonical Images folder.");

    QTemporaryFile blockedRoot(QDir(documents.path()).filePath(QStringLiteral("blocked")));
    ok &= check(blockedRoot.open()
                    && !capture.initializeDefaultOutputFolder(blockedRoot.fileName())
                    && capture.error().contains("could not be created"),
                "An unavailable default location must expose a truthful initialization error.");

    QTemporaryDir destination;
    QTemporaryDir editedDestination;
    capture.setOutputFolder(QUrl::fromLocalFile(destination.path()));
    capture.setFileName(QStringLiteral("newer-frame"));
    ok &= check(capture.canCapture() && capture.capture()
                    && capture.presentation() == QStringLiteral("capturing")
                    && !operations.momentaryAvailable(
                        ResourceLock::Camera | ResourceLock::Storage),
                "Accepted capture must retain the Camera and Storage lease.");
    capture.setOutputFolder(QUrl::fromLocalFile(editedDestination.path()));
    capture.setFileName(QStringLiteral("edited-after-acceptance"));
    ok &= check(capture.outputFolder().toLocalFile() == destination.path()
                    && capture.fileName() == QStringLiteral("newer-frame"),
                "Accepted capture inputs must ignore edits until the operation finishes.");
    QThread::msleep(80);
    QCoreApplication::processEvents();
    ok &= check(capture.presentation() == QStringLiteral("capturing"),
                "Repeated delivery of the baseline frame must not be saved.");

    fake->delivery = 2;
    ok &= check(waitFor([&capture]() {
                    return capture.presentation() == QStringLiteral("completed");
                }, 2000)
                    && QFileInfo::exists(capture.savedArtifactUrl().toLocalFile())
                    && QFileInfo(capture.savedArtifactUrl().toLocalFile()).absolutePath()
                           == destination.path()
                    && QFileInfo(capture.savedArtifactUrl().toLocalFile()).completeBaseName()
                           == QStringLiteral("newer-frame")
                    && operations.momentaryAvailable(
                        ResourceLock::Camera | ResourceLock::Storage),
                "The first newer frame must save to the accepted destination and release the lease.");

    capture.setOutputFolder(QUrl::fromLocalFile(editedDestination.path()));
    capture.setFileName(QStringLiteral("timeout"));
    ok &= check(capture.outputFolder().toLocalFile() == editedDestination.path()
                    && capture.fileName() == QStringLiteral("timeout")
                    && capture.capture(),
                "Capture inputs must become editable again after completion.");
    ok &= check(waitFor([&capture]() {
                    return capture.presentation() == QStringLiteral("error");
                }, 3000)
                    && capture.error().contains("newer frame")
                    && operations.momentaryAvailable(
                        ResourceLock::Camera | ResourceLock::Storage),
                "No newer frame must time out truthfully and release its lease.");

    capture.setFileName(QStringLiteral("conflict"));
    auto longOperation =
        operations.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    ok &= check(longOperation.acquired() && !capture.canCapture()
                    && !capture.capture() && !capture.error().isEmpty(),
                "A conflicting camera owner must reject the momentary request.");
    longOperation.lease.release();

    capture.setOutputFolder(QUrl(QStringLiteral("https://example.invalid/output")));
    ok &= check(!capture.canCapture() && !capture.capture()
                    && capture.error().contains("local folder"),
                "Non-local output URLs must fail before acquiring resources.");

    camera.close();
    waitForIdle(camera);
    worker.quit();
    worker.wait();
    return ok ? 0 : 1;
}
