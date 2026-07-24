#include "sequence_viewer_model.h"

#include "sequence_manifest_v2.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>

namespace desktop_app::v2::sequence {

bool SequenceViewerModel::open(const QString& sequenceJson, QString* error) {
    clear();

    QString manifestError;
    const auto manifest = SequenceManifestV2::load(sequenceJson, &manifestError);
    if (!manifest) {
        fail(manifestError, error);
        return false;
    }

    const auto& data = manifest->data();
    snapshot_.sequencePath = QFileInfo(sequenceJson).absoluteFilePath();
    snapshot_.sequenceId = data.sequenceId;
    snapshot_.name = data.name;
    snapshot_.sequenceStatus = data.status;
    snapshot_.frameCount = data.frameCount;
    sequenceRoot_ = QFileInfo(sequenceJson).absolutePath();

    if (data.frameCount <= 0) {
        fail(QStringLiteral("Sequence has no frames."), error);
        return false;
    }
    if (!selectFrom(1, data.frameCount, 1)) {
        fail(QStringLiteral("Sequence has no readable frames."), error);
        return false;
    }

    if (error)
        error->clear();
    return true;
}

void SequenceViewerModel::clear() {
    snapshot_ = {};
    sequenceRoot_.clear();
}

SequenceViewerSnapshot SequenceViewerModel::snapshot() const {
    return snapshot_;
}

bool SequenceViewerModel::previous() {
    if (snapshot_.status != SequenceViewerStatus::Ready || snapshot_.currentFrame <= 1)
        return false;
    return selectFrom(snapshot_.currentFrame - 1, 1, -1);
}

bool SequenceViewerModel::next() {
    if (snapshot_.status != SequenceViewerStatus::Ready ||
        snapshot_.currentFrame >= snapshot_.frameCount) {
        return false;
    }
    return selectFrom(snapshot_.currentFrame + 1, snapshot_.frameCount, 1);
}

bool SequenceViewerModel::seek(qint64 oneBasedFrame, QString* error) {
    if (snapshot_.status != SequenceViewerStatus::Ready) {
        if (error)
            *error = QStringLiteral("No Sequence is open.");
        return false;
    }
    if (oneBasedFrame < 1 || oneBasedFrame > snapshot_.frameCount) {
        if (error)
            *error = QStringLiteral("Frame must be between 1 and %1.").arg(snapshot_.frameCount);
        return false;
    }

    if (selectFrom(oneBasedFrame, snapshot_.frameCount, 1) ||
        selectFrom(oneBasedFrame - 1, 1, -1)) {
        if (error)
            error->clear();
        return true;
    }

    fail(QStringLiteral("Sequence has no readable frames."), error);
    return false;
}

bool SequenceViewerModel::loadFrame(qint64 oneBasedFrame, QImage& image) const {
    const QString fileName =
        QStringLiteral("frame_%1.tif").arg(oneBasedFrame, 8, 10, QLatin1Char('0'));
    QImageReader reader(QDir(sequenceRoot_).filePath(QStringLiteral("frames/") + fileName));
    image = reader.read();
    return !image.isNull();
}

bool SequenceViewerModel::selectFrom(qint64 start, qint64 end, qint64 step) {
    for (qint64 frame = start; step > 0 ? frame <= end : frame >= end; frame += step) {
        QImage image;
        if (!loadFrame(frame, image))
            continue;
        snapshot_.status = SequenceViewerStatus::Ready;
        snapshot_.currentFrame = frame;
        snapshot_.image = image;
        snapshot_.fault.clear();
        return true;
    }
    return false;
}

void SequenceViewerModel::fail(const QString& message, QString* error) {
    snapshot_.status = SequenceViewerStatus::Failed;
    snapshot_.currentFrame = 0;
    snapshot_.image = {};
    snapshot_.fault = message;
    if (error)
        *error = message;
}

} // namespace desktop_app::v2::sequence
