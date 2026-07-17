#pragma once

#include <cstdint>
#include <string>

#include <QDateTime>
#include <QImage>
#include <QString>

#include "pipeline_runner.h"

class LiveDataCollectionWriter {
  public:
    bool start(const QString& collectionsRoot, std::string& err);
    bool writeFrame(const QImage& image, const PipelineEvent& event, bool detectorProcessed, qint64 sourceFrameNumber,
                    std::string& err);
    bool finish(const QString& stopReason, std::string& err);

    bool isActive() const;
    QString sessionId() const;
    QString sessionDir() const;
    std::uint64_t framesSaved() const;
    std::uint64_t rowsLogged() const;

  private:
    bool writeMetadata(const QString& stopReason, std::string& err) const;
    bool appendDetectionRow(const QString& relativeImagePath, const PipelineEvent& event, bool detectorProcessed,
                            qint64 sourceFrameNumber, const QDateTime& timestampUtc, std::string& err);
    void reset();

    QString sessionId_;
    QString sessionDir_;
    QString streamDir_;
    QString csvPath_;
    QDateTime startedAtUtc_;
    QDateTime stoppedAtUtc_;
    std::uint64_t framesSaved_ = 0;
    std::uint64_t rowsLogged_ = 0;
    bool active_ = false;
};
