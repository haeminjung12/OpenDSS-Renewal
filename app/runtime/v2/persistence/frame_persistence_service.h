#pragma once

#include "../camera/camera_device.h"
#include "../../desktop_app/frame_types.h"

#include <QImage>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>

namespace desktop_app::v2::persistence {

class FramePersistenceService final {
  public:
    using FrameWriter = std::function<bool(const QImage&, const QString&, QString*)>;

    struct Metrics {
        qint64 acceptedFrames = 0;
        qint64 persistedFrames = 0;
        qint64 rejectedFrames = 0;
        qint64 failureCount = 0;
        qint64 sequentialBytes = 0;
        qint64 peakBufferedBytes = 0;
        int queueHighWater = 0;
        int poolHighWater = 0;
    };

    FramePersistenceService();
    ~FramePersistenceService();

    bool start(const QString& path, QString* error);
    bool append(const QImage& image, const FrameMeta& meta, std::uint64_t handoffId,
                QString* error);
    bool stop(QString* error);
    Metrics metrics() const;
    bool finalize(const QString& framesFolder, qint64 totalFrames, int imageWidth,
                  int imageHeight, const FrameWriter& frameWriter,
                  qint64* savedFrameCount, qint64* failedOutputIndex, QString* error) const;

    static bool writeTiffWithoutReplace(const QImage& image, const QString& target,
                                        QString* error);

  private:
    class Spool;
    std::unique_ptr<Spool> spool_;
    QString path_;
};

} // namespace desktop_app::v2::persistence
