#include "sequence_viewer_controller.h"

namespace desktop_app::v2::sequence {

SequenceViewerController::SequenceViewerController(QObject* parent)
    : QObject(parent) {}

QString SequenceViewerController::presentation() const {
    return presentationFor(model_.snapshot());
}

qint64 SequenceViewerController::currentFrame() const {
    return model_.snapshot().currentFrame;
}

qint64 SequenceViewerController::totalFrames() const {
    return model_.snapshot().frameCount;
}

QString SequenceViewerController::error() const {
    return error_;
}

bool SequenceViewerController::open(const QString& path) {
    const auto previous = model_.snapshot();
    const auto previousError = error_;
    QString modelError;
    const bool opened = model_.open(path, &modelError);
    error_ = opened ? QString() : modelError;
    publishChanges(previous, previousError);
    return opened;
}

void SequenceViewerController::clear() {
    const auto previous = model_.snapshot();
    const auto previousError = error_;
    model_.clear();
    error_.clear();
    publishChanges(previous, previousError);
}

bool SequenceViewerController::previous() {
    const auto previous = model_.snapshot();
    const auto previousError = error_;
    const bool moved = model_.previous();
    if (moved)
        error_.clear();
    publishChanges(previous, previousError);
    return moved;
}

bool SequenceViewerController::next() {
    const auto previous = model_.snapshot();
    const auto previousError = error_;
    const bool moved = model_.next();
    if (moved)
        error_.clear();
    publishChanges(previous, previousError);
    return moved;
}

bool SequenceViewerController::seek(qint64 oneBasedFrame) {
    const auto previous = model_.snapshot();
    const auto previousError = error_;
    QString modelError;
    const bool moved = model_.seek(oneBasedFrame, &modelError);
    error_ = moved ? QString() : modelError;
    publishChanges(previous, previousError);
    return moved;
}

QString SequenceViewerController::presentationFor(const SequenceViewerSnapshot& snapshot) {
    switch (snapshot.status) {
    case SequenceViewerStatus::Ready:
        return QStringLiteral("ready");
    case SequenceViewerStatus::Failed:
        return QStringLiteral("error");
    case SequenceViewerStatus::Empty:
        return QStringLiteral("empty");
    }
    return QStringLiteral("empty");
}

void SequenceViewerController::publishChanges(const SequenceViewerSnapshot& previous,
                                               const QString& previousError) {
    const auto current = model_.snapshot();
    if (presentationFor(previous) != presentationFor(current))
        emit presentationChanged();
    if (previous.currentFrame != current.currentFrame)
        emit currentFrameChanged();
    if (previous.frameCount != current.frameCount)
        emit totalFramesChanged();
    if (previousError != error_)
        emit errorChanged();
}

} // namespace desktop_app::v2::sequence
