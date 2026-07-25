#pragma once

#include "sequence_viewer_model.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QUrl>

namespace desktop_app::v2::sequence {

class SequenceViewerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString presentation READ presentation NOTIFY presentationChanged)
    Q_PROPERTY(qint64 currentFrame READ currentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(qint64 totalFrames READ totalFrames NOTIFY totalFramesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QUrl currentFrameImageUrl READ currentFrameImageUrl NOTIFY currentFrameImageUrlChanged)

  public:
    explicit SequenceViewerController(QObject* parent = nullptr);

    QString presentation() const;
    qint64 currentFrame() const;
    qint64 totalFrames() const;
    QString error() const;
    QUrl currentFrameImageUrl() const;
    QImage currentImage() const;

    Q_INVOKABLE bool open(const QString& path);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool previous();
    Q_INVOKABLE bool next();
    Q_INVOKABLE bool seek(qint64 oneBasedFrame);

  signals:
    void presentationChanged();
    void currentFrameChanged();
    void totalFramesChanged();
    void errorChanged();
    void currentFrameImageUrlChanged();

  private:
    static QString presentationFor(const SequenceViewerSnapshot& snapshot);
    void publishChanges(const SequenceViewerSnapshot& previous,
                        const SequenceViewerSnapshot& current,
                        const QString& previousError,
                        const QString& currentError,
                        bool imageChanged);

    mutable QMutex mutex_;
    SequenceViewerModel model_;
    QString error_;
    quint64 imageRevision_ = 0;
};

} // namespace desktop_app::v2::sequence
