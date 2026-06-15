#include "live_log_writer.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {

QString csvQuote(const QString& s) {
    QString out = s;
    out.replace("\"", "\"\"");
    return "\"" + out + "\"";
}

double averagePositiveFps(const std::vector<LiveLogRecord>& records) {
    double avgFps = 0.0;
    int fpsCount = 0;
    for (const auto& rec : records) {
        if (rec.fps > 0.0) {
            avgFps += rec.fps;
            fpsCount++;
        }
    }
    if (fpsCount > 0) {
        avgFps /= fpsCount;
    }
    return avgFps;
}

} // namespace

QString writeLiveLogCsv(const QString& outDir, const QString& prefix, const std::vector<LiveLogRecord>& records) {
    if (outDir.isEmpty())
        return QString();
    QDir out(outDir);
    out.mkpath(".");
    QString path = out.filePath(prefix + "_live_log.csv");
    QFile logFile(path);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream ts(&logFile);
    ts << "wall_time,frame_index,delivered,dropped,fps,cam_fps,proc_ms,processed,pipeline_enabled,pipeline_ready,bg_"
          "remaining,skip_reason,"
          "detected,fired,area,bbox_x,bbox_y,bbox_w,bbox_h,crop_x,crop_y,crop_w,crop_h,crop_path,label,score,triggered,"
          "trigger_ok,"
          "event_dir,decision_frame,decision_event_id,hit_count,waste_count\n";
    for (const auto& rec : records) {
        ts << csvQuote(rec.wallTime) << "," << rec.frameIndex << "," << rec.delivered << "," << rec.dropped << ","
           << QString::number(rec.fps, 'f', 2) << "," << QString::number(rec.camFps, 'f', 2) << ","
           << QString::number(rec.procMs, 'f', 3) << "," << (rec.processed ? "1" : "0") << ","
           << (rec.pipelineEnabled ? "1" : "0") << "," << (rec.pipelineReady ? "1" : "0") << "," << rec.bgRemaining
           << "," << csvQuote(rec.skipReason) << "," << (rec.detected ? "1" : "0") << "," << (rec.fired ? "1" : "0")
           << "," << QString::number(rec.area, 'f', 1) << "," << rec.bboxX << "," << rec.bboxY << "," << rec.bboxW
           << "," << rec.bboxH << "," << rec.cropX << "," << rec.cropY << "," << rec.cropW << "," << rec.cropH << ","
           << csvQuote(rec.cropPath) << "," << csvQuote(rec.label) << "," << QString::number(rec.score, 'f', 4) << ","
           << (rec.triggered ? "1" : "0") << "," << (rec.triggerOk ? "1" : "0") << "," << csvQuote(rec.eventDir) << ","
           << rec.decisionFrame << "," << rec.decisionEventId << "," << rec.hitCount << "," << rec.wasteCount << "\n";
    }
    ts.flush();
    logFile.close();
    return path;
}

QString writeLiveSequenceLog(const QString& outDir, const QString& timestamp, const std::vector<LiveLogRecord>& records,
                             const SequenceLogMetadata& metadata) {
    if (outDir.isEmpty())
        return QString();
    QDir out(outDir);
    out.mkpath(".");
    QString logPath = out.filePath("sequence_test_log_live_" + timestamp + ".csv");

    SequenceLogMetadata liveMetadata = metadata;
    liveMetadata.sequenceFolder = "live";
    liveMetadata.fps = averagePositiveFps(records);
    liveMetadata.frameCount = static_cast<int>(records.size());
    liveMetadata.outputDir = outDir;
    liveMetadata.includeModelAuditFields = true;
    liveMetadata.pipelineEnabledBefore = (!records.empty() && records.front().pipelineEnabled);
    liveMetadata.pipelineForced = false;

    SequenceLogWriter writer;
    if (!writer.open(logPath, liveMetadata)) {
        return QString();
    }

    for (int i = 0; i < static_cast<int>(records.size()); ++i) {
        const auto& rec = records[i];
        double scheduledMs = (liveMetadata.fps > 0.0) ? (static_cast<double>(i) * 1000.0 / liveMetadata.fps) : 0.0;
        double actualMs = static_cast<double>(rec.elapsedMs);
        double jitterMs = actualMs - scheduledMs;

        SequenceLogFrameRow row;
        row.index = i;
        row.filename = QString("live_frame_%1").arg(rec.frameIndex);
        row.scheduledMs = scheduledMs;
        row.actualMs = actualMs;
        row.jitterMs = jitterMs;
        row.wallTime = rec.wallTime;
        row.procMs = rec.procMs;
        row.processed = rec.processed;
        row.pipelineEnabled = rec.pipelineEnabled;
        row.pipelineReady = rec.pipelineReady;
        row.bgRemaining = rec.bgRemaining;
        row.skipReason = rec.skipReason;
        row.detected = rec.detected;
        row.fired = rec.fired;
        row.area = rec.area;
        row.bboxX = rec.bboxX;
        row.bboxY = rec.bboxY;
        row.bboxW = rec.bboxW;
        row.bboxH = rec.bboxH;
        row.cropX = rec.cropX;
        row.cropY = rec.cropY;
        row.cropW = rec.cropW;
        row.cropH = rec.cropH;
        row.cropPath = rec.cropPath;
        row.label = rec.label;
        row.score = rec.score;
        row.triggered = rec.triggered;
        row.triggerOk = rec.triggerOk;
        row.frameNumber = rec.frameIndex;
        row.eventDir = rec.eventDir;
        row.decisionFrame = rec.decisionFrame;
        row.decisionEventId = rec.decisionEventId;
        writer.writeFrame(row);
    }
    writer.close();
    return logPath;
}
