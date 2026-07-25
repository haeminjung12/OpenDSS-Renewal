#include "v2/camera/frame_conversion.h"
#include "v2/camera/single_image_capture_service.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QRegularExpression>
#include <QTemporaryDir>

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
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

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;
    QString savedPath;
    SingleImageCaptureService service;

    QTemporaryDir destination;
    ok &= check(destination.isValid(), "Capture destination must be available.");
    ok &= check(service.save(testFrame(), destination.path(),
                             QStringLiteral(" bad:name.TIFF.tif "),
                             &savedPath, &error),
                "A supplied valid frame must save successfully.");
    ok &= check(QFileInfo(savedPath).isAbsolute()
                    && QFileInfo(savedPath).fileName() == QStringLiteral("bad_name.tif"),
                "The saver must return one normalized absolute TIFF path.");
    ok &= check(QDir(destination.path()).entryList(QDir::Files)
                    == QStringList{QStringLiteral("bad_name.tif")},
                "One save must create exactly one TIFF and no sidecar.");
    ok &= check(QImageReader(savedPath).read() == convertCameraFrame(testFrame()),
                "The TIFF must decode to the supplied CameraFrame.");

    QFile existing(savedPath);
    ok &= check(existing.open(QIODevice::ReadOnly), "Saved TIFF must be readable.");
    const QByteArray originalBytes = existing.readAll();
    existing.close();
    ok &= check(!service.save(testFrame(), destination.path(),
                              QStringLiteral("bad_name.tif"),
                              &savedPath, &error)
                    && error.contains("already exists"),
                "An existing capture must never be overwritten.");
    ok &= check(existing.open(QIODevice::ReadOnly)
                    && existing.readAll() == originalBytes,
                "Collision handling must preserve existing bytes.");

    QTemporaryDir timestampDestination;
    ok &= check(service.save(testFrame(), timestampDestination.path(), {},
                             &savedPath, &error),
                "A blank name must use a timestamp.");
    ok &= check(QRegularExpression(
                    QStringLiteral(R"(^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.tif$)"))
                    .match(QFileInfo(savedPath).fileName()).hasMatch(),
                "Timestamp filenames must use the approved local shape.");

    QTemporaryDir names;
    ok &= check(service.save(testFrame(), names.path(), QStringLiteral("../AUX"),
                             &savedPath, &error)
                    && QFileInfo(savedPath).fileName() == QStringLiteral("_AUX.tif"),
                "Traversal and reserved names must remain contained and normalized.");

    QFile notDirectory(destination.filePath(QStringLiteral("regular-file")));
    ok &= check(notDirectory.open(QIODevice::WriteOnly),
                "Invalid-location fixture must be created.");
    notDirectory.close();
    ok &= check(!service.save(testFrame(), notDirectory.fileName(),
                              QStringLiteral("fail"), &savedPath, &error)
                    && error.contains("not a directory"),
                "A non-directory save location must fail factually.");

    CameraFrame invalid = testFrame();
    invalid.bytes.clear();
    ok &= check(!service.save(invalid, names.path(), QStringLiteral("invalid"),
                              &savedPath, &error)
                    && error.contains("incomplete"),
                "Invalid supplied frames must fail before publication.");
    return ok ? 0 : 1;
}
