#pragma once

#include <QFile>
#include <QString>
#include <QTextStream>

#include <vector>

#include "app_types.h"

struct RuntimeModelLogFields {
    QString registryEntryId;
    QString modelStateAtStart;
    QString liveUseMode;
    QString modelSha256;
    QString metadataSha256;
};

struct SequenceLogMetadata {
    QString sequenceFolder;
    double fps = 0.0;
    int frameCount = 0;
    int displayEvery = 1;
    QString outputDir;
    QString onnxResolved;
    QString metadataResolved;
    QString targetLabel;
    bool includeModelAuditFields = false;
    RuntimeModelLogFields model;
    bool pipelineEnabledBefore = false;
    bool pipelineForced = false;
    int frameSkip = 0;
    int bgFrames = 0;
    int bgUpdate = 0;
    int resetFrames = 0;
    double minArea = 0.0;
    double minAreaFrac = 0.0;
    double maxAreaFrac = 0.0;
    int minBbox = 0;
    int margin = 0;
    int diffThresh = 0;
    int blurRadius = 0;
    int morphRadius = 0;
    double scale = 1.0;
    int gapFireShift = 0;
    QString daqChannel;
    double daqAmplitude = 0.0;
    double daqFrequencyHz = 0.0;
    double daqDurationMs = 0.0;
    double daqDelayMs = 0.0;
};

struct SequenceLogFrameRow {
    int index = 0;
    QString filename;
    double scheduledMs = 0.0;
    double actualMs = 0.0;
    double jitterMs = 0.0;
    QString wallTime;
    double procMs = 0.0;
    bool processed = false;
    bool pipelineEnabled = false;
    bool pipelineReady = false;
    int bgRemaining = 0;
    QString skipReason;
    bool detected = false;
    bool fired = false;
    double area = 0.0;
    int bboxX = 0;
    int bboxY = 0;
    int bboxW = 0;
    int bboxH = 0;
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    QString cropPath;
    QString label;
    double score = 0.0;
    bool triggered = false;
    bool triggerOk = false;
    qint64 frameNumber = 0;
    QString eventDir;
    int decisionFrame = -1;
    int decisionEventId = 0;
};

struct SequenceSummaryMetadata {
    QString targetLabel;
    int totalFrames = 0;
    double fps = 0.0;
    QString sequenceFolder;
    QString outputDir;
    QString onnxResolved;
    QString metadataResolved;
    RuntimeModelLogFields model;
};

class SequenceLogWriter {
  public:
    SequenceLogWriter() = default;
    SequenceLogWriter(const SequenceLogWriter&) = delete;
    SequenceLogWriter& operator=(const SequenceLogWriter&) = delete;
    ~SequenceLogWriter();

    bool open(const QString& path, const SequenceLogMetadata& metadata);
    bool isOpen() const;
    void writeFrame(const SequenceLogFrameRow& row);
    void flush();
    void close();

  private:
    QFile file_;
    QTextStream stream_;
    bool open_ = false;
};

QString writeEventTrajectoryCsv(const QString& outDir, const QString& filename,
                                const std::vector<SequenceEventRecord>& events);

QString writeSequenceSummaryCsv(const QString& outDir, const QString& filename,
                                const std::vector<SequenceEventRecord>& events,
                                const SequenceSummaryMetadata& metadata);
