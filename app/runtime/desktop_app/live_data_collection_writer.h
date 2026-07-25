#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QDateTime>
#include <QImage>
#include <QString>

#include "pipeline_runner.h"

class LiveDataCollectionWriter {
  public:
    struct IntegrityRange {
        std::uint64_t first = 0;
        std::uint64_t last = 0;
    };

    struct Integrity {
        std::uint64_t handoffAccepted = 0;
        std::uint64_t sourceGapCount = 0;
        std::uint64_t queueRejectedCount = 0;
        std::uint64_t consumerFailureCount = 0;
        std::vector<IntegrityRange> sourceGaps;
        std::vector<IntegrityRange> queueRejected;
        std::vector<IntegrityRange> consumerFailures;
    };

    bool start(const QString& collectionsRoot, std::string& err);
    bool writeFrame(const QImage& image, const PipelineEvent& event, bool detectorProcessed, qint64 sourceFrameNumber,
                    std::string& err);
    bool finish(const QString& stopReason, std::string& err);
    void setIntegrity(Integrity integrity);

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
    Integrity integrity_;
};
