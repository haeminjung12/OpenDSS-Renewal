#pragma once

#include "sequence_viewer_model.h"

#include <QObject>

namespace desktop_app::v2::sequence {

class SequenceViewerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString presentation READ presentation NOTIFY presentationChanged)
    Q_PROPERTY(qint64 currentFrame READ currentFrame NOTIFY currentFrameChanged)
    Q_PROPERTY(qint64 totalFrames READ totalFrames NOTIFY totalFramesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
    explicit SequenceViewerController(QObject* parent = nullptr);

    QString presentation() const;
    qint64 currentFrame() const;
    qint64 totalFrames() const;
    QString error() const;

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

  private:
    static QString presentationFor(const SequenceViewerSnapshot& snapshot);
    void publishChanges(const SequenceViewerSnapshot& previous, const QString& previousError);

    SequenceViewerModel model_;
    QString error_;
};

} // namespace desktop_app::v2::sequence
