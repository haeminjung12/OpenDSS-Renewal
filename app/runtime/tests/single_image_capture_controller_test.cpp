#include "v2/camera/camera_service.h"
#include "v2/camera/single_image_capture_controller.h"
#include "v2/camera/single_image_capture_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

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
    CameraFrameResult latestFrame(CameraFrame &output, QString *) override
    {
        ++frameCalls;
        if (frameResult != CameraFrameResult::Frame) {
            return frameResult;
        }
        output = frame;
        return CameraFrameResult::Frame;
    }

    CameraFrame frame;
    CameraFrameResult frameResult = CameraFrameResult::Frame;
    int frameCalls = 0;
};

bool check(bool condition, const char *message)
{
    if (!condition) {
        qCritical().noquote() << message;
        std::fprintf(stderr, "%s\n", message);
    }
    return condition;
}

CameraFrame testFrame()
{
    CameraFrame frame;
    frame.pixelFormat = CameraPixelFormat::Mono8;
    frame.width = 2;
    frame.height = 1;
    frame.rowBytes = 2;
    frame.bitDepth = 8;
    frame.deliveryId = 1;
    frame.monotonicTimestampNs = 100;
    frame.bytes = QByteArray::fromHex("1122");
    return frame;
}

struct Fixture {
    ApplicationStateStore store;
    FixedCameraDevice *device = nullptr;
    std::unique_ptr<CameraService> camera;
    OperationCoordinator operations;
    std::unique_ptr<SingleImageCaptureService> captureService;
    std::unique_ptr<SingleImageCaptureController> controller;

    Fixture()
    {
        auto ownedDevice = std::make_unique<FixedCameraDevice>();
        device = ownedDevice.get();
        device->frame = testFrame();
        camera = std::make_unique<CameraService>(std::move(ownedDevice), store);
        captureService =
            std::make_unique<SingleImageCaptureService>(*camera, operations);
        controller = std::make_unique<SingleImageCaptureController>(
            *captureService, *camera, store, operations);
    }

    bool stream()
    {
        QString error;
        return camera->open(&error) && camera->start(&error);
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    QTemporaryDir destination;
    ok &= check(destination.isValid(), "Capture destination must exist.");

    Fixture unavailable;
    unavailable.controller->setOutputFolder(QUrl::fromLocalFile(destination.path()));
    ok &= check(!unavailable.controller->canCapture()
                    && unavailable.controller->presentation()
                           == QStringLiteral("unavailable"),
                "Unavailable camera must disable capture.");
    ok &= check(!unavailable.controller->capture()
                    && unavailable.controller->error().contains("No camera"),
                "Unavailable capture must preserve the service error.");

    Fixture ready;
    QString cameraError;
    ok &= check(ready.camera->open(&cameraError), "Ready fixture must open.");
    ready.controller->setOutputFolder(QUrl::fromLocalFile(destination.path()));
    ok &= check(!ready.controller->canCapture()
                    && !ready.controller->capture()
                    && ready.controller->error().contains("not streaming"),
                "Ready but stopped camera must remain non-capturable.");

    Fixture streaming;
    ok &= check(streaming.stream(), "Streaming fixture must start.");
    ok &= check(!streaming.controller->canCapture(),
                "A missing output folder must disable capture.");
    streaming.controller->setOutputFolder(QUrl::fromLocalFile(destination.path()));
    streaming.controller->setFileName(QStringLiteral(" bad:name.TIFF.tif "));
    bool sawCapturing = false;
    bool reentryAttempted = false;
    bool reentryResult = true;
    bool outerStatePreserved = false;
    QObject::connect(streaming.controller.get(),
                     &SingleImageCaptureController::stateChanged,
                     [&]() {
                         if (streaming.controller->presentation()
                             == QStringLiteral("capturing")) {
                             sawCapturing = !streaming.controller->canCapture();
                             if (!reentryAttempted) {
                                 reentryAttempted = true;
                                 reentryResult = streaming.controller->capture();
                                 outerStatePreserved =
                                     streaming.controller->presentation()
                                         == QStringLiteral("capturing")
                                     && streaming.controller->error().isEmpty();
                             }
                         }
                     });
    ok &= check(streaming.controller->canCapture()
                    && streaming.controller->presentation() == QStringLiteral("ready"),
                "Streaming camera must enable capture.");
    ok &= check(streaming.controller->capture() && sawCapturing
                    && reentryAttempted && !reentryResult && outerStatePreserved
                    && streaming.device->frameCalls == 1
                    && streaming.controller->presentation()
                           == QStringLiteral("completed")
                    && streaming.controller->error().isEmpty(),
                "Valid capture must complete without a controller-side writer.");
    const QUrl firstArtifact = streaming.controller->savedArtifactUrl();
    ok &= check(firstArtifact.isLocalFile()
                    && QFileInfo(firstArtifact.toLocalFile()).fileName()
                           == QStringLiteral("bad_name.tif")
                    && QFileInfo::exists(firstArtifact.toLocalFile()),
                "Saved artifact URL must identify the service-confirmed normalized TIFF.");

    streaming.controller->setOutputFolder(QUrl(QStringLiteral("https://example.invalid/output")));
    ok &= check(!streaming.controller->canCapture()
                    && !streaming.controller->capture()
                    && streaming.controller->error().contains("local folder")
                    && streaming.controller->savedArtifactUrl().isEmpty(),
                "Non-local output URLs must be rejected factually.");

    QFile regularFile(destination.filePath(QStringLiteral("not-a-folder")));
    ok &= check(regularFile.open(QIODevice::WriteOnly),
                "Invalid-folder fixture must be created.");
    regularFile.close();
    streaming.controller->setOutputFolder(QUrl::fromLocalFile(regularFile.fileName()));
    ok &= check(!streaming.controller->canCapture()
                    && !streaming.controller->capture()
                    && streaming.controller->error().contains("not a directory"),
                "A non-directory output location must preserve the service error.");

    streaming.controller->setOutputFolder(
        QUrl::fromLocalFile(destination.filePath(QStringLiteral("missing-folder"))));
    ok &= check(!streaming.controller->canCapture(),
                "A missing output directory must disable capture.");

    QUrl queriedFolder = QUrl::fromLocalFile(destination.path());
    queriedFolder.setQuery(QStringLiteral("unexpected=1"));
    streaming.controller->setOutputFolder(queriedFolder);
    ok &= check(!streaming.controller->canCapture(),
                "A local folder URL with a query must disable capture.");

#ifdef Q_OS_WIN
    const QString unwritablePath = QStringLiteral("C:/System Volume Information");
#else
    const QString unwritablePath = QStringLiteral("/proc/sys");
#endif
    const QFileInfo unwritableInfo(unwritablePath);
    streaming.controller->setOutputFolder(QUrl::fromLocalFile(unwritablePath));
    ok &= check(unwritableInfo.exists() && unwritableInfo.isDir()
                    && !streaming.controller->canCapture(),
                "An unwritable output directory must disable capture.");

    QTemporaryDir names;
    streaming.controller->setOutputFolder(QUrl::fromLocalFile(names.path()));
    streaming.controller->setFileName(QStringLiteral("../safe-name.tiff"));
    ok &= check(streaming.controller->capture()
                    && QFileInfo(streaming.controller->savedArtifactUrl().toLocalFile())
                           .absolutePath()
                           == QFileInfo(names.path()).absoluteFilePath()
                    && QFileInfo(streaming.controller->savedArtifactUrl().toLocalFile())
                           .fileName()
                           == QStringLiteral("safe-name.tif"),
                "Traversal input must remain contained and normalize to one TIFF suffix.");

    streaming.controller->setFileName(QStringLiteral("safe-name.tif"));
    ok &= check(!streaming.controller->capture()
                    && streaming.controller->error().contains("already exists"),
                "Collision must preserve the service's factual error.");
    streaming.controller->setFileName(QStringLiteral("recovered-name"));
    ok &= check(streaming.controller->error().isEmpty()
                    && streaming.controller->capture()
                    && QFileInfo(streaming.controller->savedArtifactUrl().toLocalFile())
                           .fileName()
                           == QStringLiteral("recovered-name.tif"),
                "Editing after an error must restore a successful capture path.");

    streaming.controller->setFileName(QString(400, QLatin1Char('x')));
    ok &= check(!streaming.controller->capture()
                    && (streaming.controller->error().contains("could not be published")
                        || streaming.controller->error().contains("could not be opened")),
                "An unwritable target name must preserve the service write error.");

    Fixture conflicted;
    ok &= check(conflicted.stream(), "Conflict fixture must stream.");
    conflicted.controller->setOutputFolder(QUrl::fromLocalFile(names.path()));
    conflicted.controller->setFileName(QStringLiteral("locked"));
    auto lease =
        conflicted.operations.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    ok &= check(lease.acquired(), "Conflict fixture must hold the camera lock.");
    ok &= check(!conflicted.controller->capture()
                    && !conflicted.controller->error().isEmpty(),
                "Lock conflict must preserve the coordinator's factual error.");
    ok &= check(!conflicted.controller->canCapture(),
                "Held Camera lock must disable capture.");
    lease.lease.release();
    ok &= check(conflicted.controller->canCapture(),
                "Releasing the Camera lock must restore capture readiness.");

    Fixture noFrame;
    ok &= check(noFrame.stream(), "No-frame fixture must stream.");
    QTemporaryDir noFrameDestination;
    noFrame.controller->setOutputFolder(
        QUrl::fromLocalFile(noFrameDestination.path()));
    noFrame.controller->setFileName(QStringLiteral("no-frame"));
    noFrame.device->frameResult = CameraFrameResult::NoFrame;
    ok &= check(!noFrame.controller->capture()
                    && noFrame.controller->presentation() == QStringLiteral("error")
                    && noFrame.controller->error()
                           == QStringLiteral("Camera did not provide a frame."),
                "NoFrame must produce one factual controller fallback.");
    noFrame.device->frameResult = CameraFrameResult::Frame;
    noFrame.controller->setFileName(QStringLiteral("after-no-frame"));
    ok &= check(noFrame.controller->capture()
                    && noFrame.controller->error().isEmpty()
                    && noFrame.controller->presentation()
                           == QStringLiteral("completed"),
                "A successful retry must clear the NoFrame error.");

    return ok ? 0 : 1;
}
