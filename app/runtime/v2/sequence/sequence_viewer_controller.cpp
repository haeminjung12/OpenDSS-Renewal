#include "sequence_viewer_controller.h"

#include <QMutexLocker>

namespace desktop_app::v2::sequence {

SequenceViewerController::SequenceViewerController(QObject* parent)
    : QObject(parent) {}

QString SequenceViewerController::presentation() const {
    QMutexLocker lock(&mutex_);
    return presentationFor(model_.snapshot());
}

qint64 SequenceViewerController::currentFrame() const {
    QMutexLocker lock(&mutex_);
    return model_.snapshot().currentFrame;
}

qint64 SequenceViewerController::totalFrames() const {
    QMutexLocker lock(&mutex_);
    return model_.snapshot().frameCount;
}

QString SequenceViewerController::error() const {
    QMutexLocker lock(&mutex_);
    return error_;
}

QUrl SequenceViewerController::currentFrameImageUrl() const {
    QMutexLocker lock(&mutex_);
    const auto snapshot = model_.snapshot();
    if (snapshot.status != SequenceViewerStatus::Ready || snapshot.image.isNull())
        return {};
    return QUrl(QStringLiteral("image://sequence-frame/current/%1").arg(imageRevision_));
}

QImage SequenceViewerController::currentImage() const {
    QMutexLocker lock(&mutex_);
    return model_.snapshot().image;
}

bool SequenceViewerController::open(const QString& path) {
    SequenceViewerSnapshot previous;
    SequenceViewerSnapshot current;
    QString previousError;
    QString currentError;
    bool imageChanged = false;
    bool opened = false;
    {
        QMutexLocker lock(&mutex_);
        previous = model_.snapshot();
        previousError = error_;
        QString modelError;
        opened = model_.open(path, &modelError);
        error_ = opened ? QString() : modelError;
        current = model_.snapshot();
        currentError = error_;
        imageChanged = previous.currentFrame != current.currentFrame || previous.image != current.image;
        if (imageChanged)
            ++imageRevision_;
    }
    publishChanges(previous, current, previousError, currentError, imageChanged);
    return opened;
}

void SequenceViewerController::clear() {
    SequenceViewerSnapshot previous;
    SequenceViewerSnapshot current;
    QString previousError;
    QString currentError;
    bool imageChanged = false;
    {
        QMutexLocker lock(&mutex_);
        previous = model_.snapshot();
        previousError = error_;
        model_.clear();
        error_.clear();
        current = model_.snapshot();
        currentError = error_;
        imageChanged = previous.currentFrame != current.currentFrame || previous.image != current.image;
        if (imageChanged)
            ++imageRevision_;
    }
    publishChanges(previous, current, previousError, currentError, imageChanged);
}

bool SequenceViewerController::previous() {
    SequenceViewerSnapshot previous;
    SequenceViewerSnapshot current;
    QString previousError;
    QString currentError;
    bool imageChanged = false;
    bool moved = false;
    {
        QMutexLocker lock(&mutex_);
        previous = model_.snapshot();
        previousError = error_;
        moved = model_.previous();
        if (moved)
            error_.clear();
        current = model_.snapshot();
        currentError = error_;
        imageChanged = previous.currentFrame != current.currentFrame || previous.image != current.image;
        if (imageChanged)
            ++imageRevision_;
    }
    publishChanges(previous, current, previousError, currentError, imageChanged);
    return moved;
}

bool SequenceViewerController::next() {
    SequenceViewerSnapshot previous;
    SequenceViewerSnapshot current;
    QString previousError;
    QString currentError;
    bool imageChanged = false;
    bool moved = false;
    {
        QMutexLocker lock(&mutex_);
        previous = model_.snapshot();
        previousError = error_;
        moved = model_.next();
        if (moved)
            error_.clear();
        current = model_.snapshot();
        currentError = error_;
        imageChanged = previous.currentFrame != current.currentFrame || previous.image != current.image;
        if (imageChanged)
            ++imageRevision_;
    }
    publishChanges(previous, current, previousError, currentError, imageChanged);
    return moved;
}

bool SequenceViewerController::seek(qint64 oneBasedFrame) {
    SequenceViewerSnapshot previous;
    SequenceViewerSnapshot current;
    QString previousError;
    QString currentError;
    bool imageChanged = false;
    bool moved = false;
    {
        QMutexLocker lock(&mutex_);
        previous = model_.snapshot();
        previousError = error_;
        QString modelError;
        moved = model_.seek(oneBasedFrame, &modelError);
        error_ = moved ? QString() : modelError;
        current = model_.snapshot();
        currentError = error_;
        imageChanged = previous.currentFrame != current.currentFrame || previous.image != current.image;
        if (imageChanged)
            ++imageRevision_;
    }
    publishChanges(previous, current, previousError, currentError, imageChanged);
    return moved;
}

int SequenceViewerController::imageWidth() const {
    QMutexLocker lock(&mutex_);
    return model_.snapshot().image.width();
}

int SequenceViewerController::imageHeight() const {
    QMutexLocker lock(&mutex_);
    return model_.snapshot().image.height();
}

bool SequenceViewerController::jump(qint64 delta)
{
    qint64 target = 0;
    {
        QMutexLocker lock(&mutex_);
        const auto snapshot = model_.snapshot();
        if (snapshot.status != SequenceViewerStatus::Ready || snapshot.frameCount <= 0)
            return false;
        target = qBound<qint64>(1, snapshot.currentFrame + delta, snapshot.frameCount);
    }
    return seek(target);
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
                                               const SequenceViewerSnapshot& current,
                                               const QString& previousError,
                                               const QString& currentError,
                                               bool imageChanged) {
    if (presentationFor(previous) != presentationFor(current))
        emit presentationChanged();
    if (previous.currentFrame != current.currentFrame)
        emit currentFrameChanged();
    if (previous.frameCount != current.frameCount)
        emit totalFramesChanged();
    if (previousError != currentError)
        emit errorChanged();
    if (imageChanged)
        emit currentFrameImageUrlChanged();
}

} // namespace desktop_app::v2::sequence
