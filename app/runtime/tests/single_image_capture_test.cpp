#include "v2/camera/camera_service.h"
#include "v2/camera/frame_conversion.h"
#include "v2/camera/single_image_capture_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QRegularExpression>
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
    CameraFrameResult latestFrame(CameraFrame &output, QString *error) override
    {
        if (!frameAvailable) {
            *error = QStringLiteral("No current frame is available.");
            return CameraFrameResult::Error;
        }
        output = frame;
        return CameraFrameResult::Frame;
    }

    bool frameAvailable = true;
    CameraFrame frame;
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
    frame.width = 3;
    frame.height = 2;
    frame.rowBytes = 5;
    frame.bitDepth = 8;
    frame.deliveryId = 1;
    frame.monotonicTimestampNs = 100;
    frame.bytes = QByteArray::fromHex("001122aabb334455ccdd");
    return frame;
}

struct CameraFixture {
    ApplicationStateStore store;
    FixedCameraDevice *device = nullptr;
    std::unique_ptr<CameraService> camera;

    CameraFixture()
    {
        auto ownedDevice = std::make_unique<FixedCameraDevice>();
        device = ownedDevice.get();
        device->frame = testFrame();
        camera = std::make_unique<CameraService>(std::move(ownedDevice), store);
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
    QString error;
    QString savedPath;

    QTemporaryDir destination;
    ok &= check(destination.isValid(), "Capture destination must be available.");
    CameraFixture fixture;
    ok &= check(fixture.stream(), "Capture camera must start.");
    OperationCoordinator operations;
    SingleImageCaptureService capture(*fixture.camera, operations);

    const bool firstCapture = capture.capture(destination.path(),
                                              QStringLiteral(" bad:name.TIFF.tif "),
                                              &savedPath,
                                              &error);
    if (!firstCapture) {
        std::fprintf(stderr, "First capture error: %s\n", qPrintable(error));
    }
    ok &= check(firstCapture,
                "Explicitly named capture should succeed.");
    ok &= check(QFileInfo(savedPath).isAbsolute()
                    && QFileInfo(savedPath).fileName() == QStringLiteral("bad_name.tif"),
                "Capture must return a confirmed absolute sanitized path with one .tif suffix.");
    const QStringList files = QDir(destination.path()).entryList(QDir::Files);
    ok &= check(files == QStringList{QStringLiteral("bad_name.tif")},
                "Capture must create exactly one TIFF and no sidecars.");
    const QImage decoded = QImageReader(savedPath).read();
    const QImage expected = convertCameraFrame(testFrame());
    ok &= check(!decoded.isNull() && decoded == expected,
                "Saved TIFF must decode to the converted camera frame.");

    QFile existing(savedPath);
    ok &= check(existing.open(QIODevice::ReadOnly), "Saved TIFF should be readable.");
    const QByteArray originalBytes = existing.readAll();
    existing.close();
    ok &= check(!capture.capture(destination.path(),
                                 QStringLiteral("bad_name.tif"),
                                 &savedPath,
                                 &error)
                    && error.contains("already exists"),
                "An existing capture must not be overwritten.");
    ok &= check(existing.open(QIODevice::ReadOnly) && existing.readAll() == originalBytes,
                "Collision handling must preserve the existing TIFF.");
    existing.close();
    ok &= check(QDir(destination.path()).entryList(QDir::Files)
                    == QStringList{QStringLiteral("bad_name.tif")},
                "Collision failure must clean up its completed temporary TIFF.");

    QTemporaryDir timestampDestination;
    savedPath.clear();
    ok &= check(capture.capture(timestampDestination.path(), {}, &savedPath, &error),
                "Blank filenames should use a local timestamp.");
    const QRegularExpression timestampName(
        QStringLiteral(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.tif$)"));
    ok &= check(timestampName.match(QFileInfo(savedPath).fileName()).hasMatch(),
                "Blank capture name must use the required local timestamp shape.");

    struct NameCase {
        const char *requested;
        const char *expected;
    };
    const NameCase nameCases[] = {
        {"CON", "_CON.tif"},
        {"prn.TIFF", "_prn.tif"},
        {"COM1", "_COM1.tif"},
        {"lpt9.data", "_lpt9.data.tif"},
        {"../AUX", "_AUX.tif"},
        {"..\\NUL", "_NUL.tif"},
        {"../safe-name.tiff", "safe-name.tif"},
    };
    QTemporaryDir nameDestination;
    for (const NameCase &nameCase : nameCases) {
        savedPath.clear();
        ok &= check(capture.capture(nameDestination.path(),
                                    QString::fromLatin1(nameCase.requested),
                                    &savedPath,
                                    &error),
                    "Reserved/traversal filename capture should succeed.");
        ok &= check(QFileInfo(savedPath).fileName() == QString::fromLatin1(nameCase.expected),
                    "Reserved/traversal filename must be safely normalized.");
    }

    QTemporaryDir outsideDefault;
    savedPath.clear();
    ok &= check(capture.capture(outsideDefault.path(),
                                QStringLiteral("outside-default"),
                                &savedPath,
                                &error)
                    && QFileInfo(savedPath).absolutePath()
                           == QFileInfo(outsideDefault.path()).absoluteFilePath(),
                "A writable location outside the default must be accepted.");

    QFile notDirectory(destination.filePath(QStringLiteral("regular-file")));
    ok &= check(notDirectory.open(QIODevice::WriteOnly), "Write-failure fixture should be created.");
    notDirectory.close();
    ok &= check(!capture.capture(notDirectory.fileName(), QStringLiteral("fail"), &savedPath, &error)
                    && error.contains("not a directory"),
                "A deterministic invalid write location must fail contextually.");

    CameraFixture unavailableFixture;
    OperationCoordinator unavailableOperations;
    SingleImageCaptureService unavailable(*unavailableFixture.camera, unavailableOperations);
    ok &= check(!unavailable.capture(destination.path(), QStringLiteral("unavailable"), &savedPath, &error)
                    && error.contains("No camera"),
                "Capture must reject an unavailable camera.");

    CameraFixture stoppedFixture;
    ok &= check(stoppedFixture.camera->open(&error), "Stopped-camera fixture should open.");
    OperationCoordinator stoppedOperations;
    SingleImageCaptureService stopped(*stoppedFixture.camera, stoppedOperations);
    ok &= check(!stopped.capture(destination.path(), QStringLiteral("stopped"), &savedPath, &error)
                    && error.contains("not streaming"),
                "Capture must reject a camera that is not streaming.");

    CameraFixture emptyFixture;
    ok &= check(emptyFixture.stream(), "No-frame fixture camera must start.");
    emptyFixture.device->frameAvailable = false;
    OperationCoordinator emptyOperations;
    SingleImageCaptureService empty(*emptyFixture.camera, emptyOperations);
    ok &= check(!empty.capture(destination.path(), QStringLiteral("empty"), &savedPath, &error)
                    && error == QStringLiteral("No current frame is available."),
                "Capture must report that no current frame is available.");

    CameraFixture conflictFixture;
    ok &= check(conflictFixture.stream(), "Conflict fixture camera must start.");
    OperationCoordinator conflictOperations;
    auto longOperation =
        conflictOperations.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    ok &= check(longOperation.acquired(), "Conflicting operation should acquire the camera.");
    SingleImageCaptureService conflicted(*conflictFixture.camera, conflictOperations);
    ok &= check(!conflicted.capture(destination.path(),
                                    QStringLiteral("conflict"),
                                    &savedPath,
                                    &error)
                    && !error.isEmpty(),
                "Capture must report a coordinator conflict.");

    return ok ? 0 : 1;
}
