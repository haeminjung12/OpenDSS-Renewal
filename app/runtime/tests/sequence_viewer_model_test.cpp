#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence/sequence_viewer_model.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <iostream>

namespace {

using namespace desktop_app::v2::sequence;

int fail(int code, const QString& message) {
    std::cerr << message.toStdString() << '\n';
    return code;
}

SequenceManifestData manifestData(qint64 frameCount) {
    return {
        "sequence-001",
        "Viewer fixture",
        "Characterization",
        "",
        "completed",
        "2026-07-24T10:15:00-05:00",
        "2026-07-24T10:15:01-05:00",
        "2026-07-24T10:15:11-05:00",
        10.0,
        "duration",
        "2.0.0",
        frameCount,
        {},
        2,
        2,
        8,
        25.0,
    };
}

QString framePath(const QString& root, qint64 frame) {
    return QDir(root).filePath(
        QStringLiteral("frames/frame_%1.tif").arg(frame, 8, 10, QLatin1Char('0')));
}

bool writeFrame(const QString& root, qint64 frame, QRgb color) {
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(color);
    return image.save(framePath(root, frame), "TIFF");
}

bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

QByteArray hash(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid() || !QDir().mkpath(QDir(temp.path()).filePath("frames")))
        return fail(1, "Could not create viewer fixture.");

    const QString manifestPath = QDir(temp.path()).filePath("sequence.json");
    QString error;
    if (!SequenceManifestV2::save(manifestPath, manifestData(5), &error) ||
        !writeFrame(temp.path(), 1, qRgb(1, 2, 3)) ||
        !writeFrame(temp.path(), 3, qRgb(3, 4, 5)) ||
        !writeFrame(temp.path(), 5, qRgb(5, 6, 7)) ||
        !writeBytes(framePath(temp.path(), 4), "not an image")) {
        return fail(2, "Could not write viewer fixture: " + error);
    }

    const QByteArray manifestHash = hash(manifestPath);
    const QByteArray firstHash = hash(framePath(temp.path(), 1));
    const QByteArray thirdHash = hash(framePath(temp.path(), 3));
    const QByteArray corruptHash = hash(framePath(temp.path(), 4));
    const QByteArray fifthHash = hash(framePath(temp.path(), 5));

    SequenceViewerModel viewer;
    if (!viewer.open(manifestPath, &error))
        return fail(3, "Viewer did not open: " + error);
    auto snapshot = viewer.snapshot();
    if (snapshot.status != SequenceViewerStatus::Ready ||
        snapshot.sequencePath != QFileInfo(manifestPath).absoluteFilePath() ||
        snapshot.sequenceId != "sequence-001" || snapshot.name != "Viewer fixture" ||
        snapshot.sequenceStatus != "completed" || snapshot.currentFrame != 1 ||
        snapshot.frameCount != 5 || snapshot.image.pixel(0, 0) != qRgb(1, 2, 3)) {
        return fail(4, "Initial viewer snapshot is incorrect.");
    }

    if (!viewer.next() || viewer.snapshot().currentFrame != 3 ||
        !viewer.next() || viewer.snapshot().currentFrame != 5 ||
        viewer.next() || viewer.snapshot().currentFrame != 5 ||
        !viewer.previous() || viewer.snapshot().currentFrame != 3 ||
        !viewer.previous() || viewer.snapshot().currentFrame != 1 ||
        viewer.previous() || viewer.snapshot().currentFrame != 1) {
        return fail(5, "Previous/next ordering or bounds are incorrect.");
    }

    if (!viewer.seek(4, &error) || viewer.snapshot().currentFrame != 5 ||
        !viewer.seek(2, &error) || viewer.snapshot().currentFrame != 3 ||
        viewer.seek(0, &error) || error.isEmpty() ||
        viewer.snapshot().currentFrame != 3) {
        return fail(6, "Seek did not use requested-forward-backward ordering or bounds.");
    }

    if (hash(manifestPath) != manifestHash || hash(framePath(temp.path(), 1)) != firstHash ||
        hash(framePath(temp.path(), 3)) != thirdHash ||
        hash(framePath(temp.path(), 4)) != corruptHash ||
        hash(framePath(temp.path(), 5)) != fifthHash) {
        return fail(7, "Viewer changed source files.");
    }

    QFile::remove(framePath(temp.path(), 5));
    if (!viewer.seek(4, &error) || viewer.snapshot().currentFrame != 3)
        return fail(8, "Seek did not skip missing/corrupt frames backward.");

    if (hash(manifestPath) != manifestHash || hash(framePath(temp.path(), 1)) != firstHash ||
        hash(framePath(temp.path(), 3)) != thirdHash ||
        hash(framePath(temp.path(), 4)) != corruptHash) {
        return fail(9, "Viewer changed remaining source files.");
    }

    viewer.clear();
    snapshot = viewer.snapshot();
    if (snapshot.status != SequenceViewerStatus::Empty || !snapshot.sequencePath.isEmpty() ||
        snapshot.currentFrame != 0 || snapshot.frameCount != 0 || !snapshot.image.isNull() ||
        !snapshot.fault.isEmpty()) {
        return fail(10, "Clear did not restore the empty snapshot.");
    }

    QTemporaryDir unreadable;
    if (!unreadable.isValid() ||
        !QDir().mkpath(QDir(unreadable.path()).filePath("frames")) ||
        !SequenceManifestV2::save(QDir(unreadable.path()).filePath("sequence.json"),
                                  manifestData(2), &error) ||
        !writeBytes(framePath(unreadable.path(), 1), "broken") ||
        viewer.open(QDir(unreadable.path()).filePath("sequence.json"), &error)) {
        return fail(11, "Could not establish all-unreadable failure.");
    }
    snapshot = viewer.snapshot();
    if (snapshot.status != SequenceViewerStatus::Failed || snapshot.fault.isEmpty() ||
        snapshot.frameCount != 2 || snapshot.currentFrame != 0 || !snapshot.image.isNull()) {
        return fail(12, "All-unreadable Sequence did not produce a failed snapshot.");
    }

    QTemporaryDir empty;
    if (!empty.isValid() || !SequenceManifestV2::save(
                                QDir(empty.path()).filePath("sequence.json"),
                                manifestData(0), &error) ||
        viewer.open(QDir(empty.path()).filePath("sequence.json"), &error) ||
        viewer.snapshot().status != SequenceViewerStatus::Failed) {
        return fail(13, "Zero-frame Sequence was accepted.");
    }

    return 0;
}
