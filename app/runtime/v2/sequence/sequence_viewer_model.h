#pragma once

#include <QImage>
#include <QString>

namespace desktop_app::v2::sequence {

enum class SequenceViewerStatus {
    Empty,
    Ready,
    Failed,
};

struct SequenceViewerSnapshot {
    SequenceViewerStatus status = SequenceViewerStatus::Empty;
    QString sequencePath;
    QString sequenceId;
    QString name;
    QString sequenceStatus;
    qint64 currentFrame = 0;
    qint64 frameCount = 0;
    QImage image;
    QString fault;
};

class SequenceViewerModel {
  public:
    bool open(const QString& sequenceJson, QString* error = nullptr);
    void clear();

    SequenceViewerSnapshot snapshot() const;
    bool previous();
    bool next();
    bool seek(qint64 oneBasedFrame, QString* error = nullptr);

  private:
    bool loadFrame(qint64 oneBasedFrame, QImage& image) const;
    bool selectFrom(qint64 start, qint64 end, qint64 step);
    void fail(const QString& message, QString* error);

    SequenceViewerSnapshot snapshot_;
    QString sequenceRoot_;
};

} // namespace desktop_app::v2::sequence
