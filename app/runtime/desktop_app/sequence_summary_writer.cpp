#include "sequence_summary_writer.h"

#include <QDir>

namespace {

QString csvQuote(const QString& s) {
    QString out = s;
    out.replace("\"", "\"\"");
    return "\"" + out + "\"";
}

void writeSequenceLogHeader(QTextStream& ts, const SequenceLogMetadata& metadata) {
    ts << "# sequence_folder=" << metadata.sequenceFolder << "\n";
    ts << "# fps=" << QString::number(metadata.fps, 'f', 2) << "\n";
    ts << "# frames=" << metadata.frameCount << "\n";
    ts << "# display_every=" << metadata.displayEvery << "\n";
    ts << "# output_dir=" << metadata.outputDir << "\n";
    ts << "# onnx=" << metadata.onnxResolved << "\n";
    ts << "# metadata=" << metadata.metadataResolved << "\n";
    ts << "# target_label=" << metadata.targetLabel << "\n";
    if (metadata.includeModelAuditFields) {
        ts << "# target_class_id=" << metadata.targetLabel << "\n";
        ts << "# model_registry_entry_id=" << metadata.model.registryEntryId << "\n";
        ts << "# model_state_at_start=" << metadata.model.modelStateAtStart << "\n";
        ts << "# live_use_mode=" << metadata.model.liveUseMode << "\n";
        ts << "# model_sha256=" << metadata.model.modelSha256 << "\n";
        ts << "# metadata_sha256=" << metadata.model.metadataSha256 << "\n";
    }
    ts << "# pipeline_enabled_before=" << (metadata.pipelineEnabledBefore ? 1 : 0) << "\n";
    ts << "# pipeline_forced=" << (metadata.pipelineForced ? 1 : 0) << "\n";
    ts << "# frame_skip=" << metadata.frameSkip << "\n";
    ts << "# detect_bg_frames=" << metadata.bgFrames << "\n";
    ts << "# detect_bg_update=" << metadata.bgUpdate << "\n";
    ts << "# detect_reset_frames=" << metadata.resetFrames << "\n";
    ts << "# detect_min_area=" << QString::number(metadata.minArea, 'f', 3) << "\n";
    ts << "# detect_min_area_frac=" << QString::number(metadata.minAreaFrac, 'f', 6) << "\n";
    ts << "# detect_max_area_frac=" << QString::number(metadata.maxAreaFrac, 'f', 6) << "\n";
    ts << "# detect_min_bbox=" << metadata.minBbox << "\n";
    ts << "# detect_margin=" << metadata.margin << "\n";
    ts << "# detect_diff_thresh=" << metadata.diffThresh << "\n";
    ts << "# detect_blur_radius=" << metadata.blurRadius << "\n";
    ts << "# detect_morph_radius=" << metadata.morphRadius << "\n";
    ts << "# detect_scale=" << QString::number(metadata.scale, 'f', 3) << "\n";
    ts << "# detect_gap_fire_shift=" << metadata.gapFireShift << "\n";
    ts << "# daq_channel=" << metadata.daqChannel << "\n";
    ts << "# daq_range_min=-10\n";
    ts << "# daq_range_max=10\n";
    ts << "# daq_amplitude_v=" << QString::number(metadata.daqAmplitude, 'f', 3) << "\n";
    ts << "# daq_frequency_hz=" << QString::number(metadata.daqFrequencyHz, 'f', 1) << "\n";
    ts << "# daq_duration_ms=" << QString::number(metadata.daqDurationMs, 'f', 3) << "\n";
    ts << "# daq_delay_ms=" << QString::number(metadata.daqDelayMs, 'f', 3) << "\n";
    ts << "index,filename,scheduled_ms,actual_ms,jitter_ms,wall_time,proc_ms,processed,pipeline_enabled,pipeline_ready,"
          "bg_remaining,skip_reason,"
          "detected,fired,area,bbox_x,bbox_y,bbox_w,bbox_h,crop_x,crop_y,crop_w,crop_h,crop_path,label,score,triggered,"
          "trigger_ok,frame_number,"
          "event_dir,decision_frame,decision_event_id\n";
}

} // namespace

SequenceLogWriter::~SequenceLogWriter() {
    close();
}

bool SequenceLogWriter::open(const QString& path, const SequenceLogMetadata& metadata) {
    close();
    file_.setFileName(path);
    if (!file_.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    stream_.setDevice(&file_);
    open_ = true;
    writeSequenceLogHeader(stream_, metadata);
    stream_.flush();
    return true;
}

bool SequenceLogWriter::isOpen() const {
    return open_;
}

void SequenceLogWriter::writeFrame(const SequenceLogFrameRow& row) {
    if (!open_)
        return;
    stream_ << row.index << "," << csvQuote(row.filename) << "," << QString::number(row.scheduledMs, 'f', 3) << ","
            << QString::number(row.actualMs, 'f', 3) << "," << QString::number(row.jitterMs, 'f', 3) << ","
            << csvQuote(row.wallTime) << "," << QString::number(row.procMs, 'f', 3) << ","
            << (row.processed ? "1" : "0") << "," << (row.pipelineEnabled ? "1" : "0") << ","
            << (row.pipelineReady ? "1" : "0") << "," << row.bgRemaining << "," << csvQuote(row.skipReason) << ","
            << (row.detected ? "1" : "0") << "," << (row.fired ? "1" : "0") << "," << QString::number(row.area, 'f', 1)
            << "," << row.bboxX << "," << row.bboxY << "," << row.bboxW << "," << row.bboxH << "," << row.cropX << ","
            << row.cropY << "," << row.cropW << "," << row.cropH << "," << csvQuote(row.cropPath) << ","
            << csvQuote(row.label) << "," << QString::number(row.score, 'f', 4) << "," << (row.triggered ? "1" : "0")
            << "," << (row.triggerOk ? "1" : "0") << "," << row.frameNumber << "," << csvQuote(row.eventDir) << ","
            << row.decisionFrame << "," << row.decisionEventId << "\n";
}

void SequenceLogWriter::flush() {
    if (open_) {
        stream_.flush();
    }
}

void SequenceLogWriter::close() {
    if (open_) {
        stream_.flush();
        stream_.setDevice(nullptr);
        file_.close();
        open_ = false;
    }
}

QString writeEventTrajectoryCsv(const QString& outDir, const QString& filename,
                                const std::vector<SequenceEventRecord>& events) {
    if (outDir.isEmpty())
        return QString();
    QDir out(outDir);
    out.mkpath(".");
    QString path = out.filePath(filename);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream ts(&file);
    ts << "event_id,label,detected_frame,decision_frame,decision_dir,fired_frame,frames_tracked,"
          "start_x,start_y,end_x,end_y,min_y,max_y,cumulative_dy,path_length,frame_height\n";
    for (const auto& rec : events) {
        ts << rec.eventId << "," << csvQuote(rec.label) << "," << rec.startFrame << "," << rec.decisionFrame << ","
           << csvQuote(rec.decisionDir) << "," << rec.firedFrame << "," << rec.framesTracked << ","
           << QString::number(rec.startX, 'f', 3) << "," << QString::number(rec.startY, 'f', 3) << ","
           << QString::number(rec.endX, 'f', 3) << "," << QString::number(rec.endY, 'f', 3) << ","
           << QString::number(rec.minY, 'f', 3) << "," << QString::number(rec.maxY, 'f', 3) << ","
           << QString::number(rec.cumulativeDy, 'f', 3) << "," << QString::number(rec.pathLength, 'f', 3) << ","
           << rec.frameHeight << "\n";
    }
    ts.flush();
    file.close();
    return path;
}

QString writeSequenceSummaryCsv(const QString& outDir, const QString& filename,
                                const std::vector<SequenceEventRecord>& events,
                                const SequenceSummaryMetadata& metadata) {
    if (outDir.isEmpty())
        return QString();
    QDir out(outDir);
    out.mkpath(".");
    QString path = out.filePath(filename);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream ts(&file);
    QString target = metadata.targetLabel.trimmed();
    if (target.isEmpty()) {
        target = "Single";
    }
    QString targetLower = target.toLower();
    const QString unclassifiedLower = normalizeEventLabel(QString()).toLower();
    QMap<QString, int> classCounts;
    int wentToHitCount = 0;
    int wentToWasteCount = 0;
    int classifiedHitCount = 0;
    int classifiedWasteCount = 0;
    int truePositive = 0;
    int trueNegative = 0;
    int falsePositive = 0;
    int falseNegative = 0;
    int consideredEvents = 0;
    for (const auto& rec : events) {
        QString label = normalizeEventLabel(rec.label);
        QString labelLower = label.toLower();
        bool isClassified = (labelLower != unclassifiedLower);
        bool eligible = (rec.firedFrame >= 0) && isClassified;
        if (!eligible) {
            continue;
        }
        consideredEvents++;
        classCounts[label] = classCounts.value(label) + 1;
        bool isTarget = (labelLower == targetLower);
        bool isHit = (rec.decisionDir == "Hit");
        bool isWaste = (rec.decisionDir == "Waste");
        if (isHit)
            wentToHitCount++;
        if (isWaste)
            wentToWasteCount++;
        if (isTarget) {
            classifiedHitCount++;
        } else {
            classifiedWasteCount++;
        }
        if (isTarget) {
            if (isHit) {
                truePositive++;
            } else if (isWaste) {
                falseNegative++;
            }
        } else {
            if (isWaste) {
                trueNegative++;
            } else if (isHit) {
                falsePositive++;
            }
        }
    }
    int totalDecisions = consideredEvents;
    double efficiency = totalDecisions > 0 ? static_cast<double>(truePositive + trueNegative) / totalDecisions : 0.0;
    double precision =
        (truePositive + falsePositive) > 0 ? static_cast<double>(truePositive) / (truePositive + falsePositive) : 0.0;
    double recall =
        (truePositive + falseNegative) > 0 ? static_cast<double>(truePositive) / (truePositive + falseNegative) : 0.0;

    ts << "metric,value\n";
    ts << "summary_schema,sequence_summary.classified_and_went_to.v3\n";
    ts << "summary_note,"
       << csvQuote(
              "Classified counts use the runtime target label; went-to counts use motion Hit/Waste channel decisions.")
       << "\n";
    ts << "sequence_folder," << csvQuote(metadata.sequenceFolder) << "\n";
    ts << "output_dir," << csvQuote(metadata.outputDir) << "\n";
    ts << "onnx," << csvQuote(metadata.onnxResolved) << "\n";
    ts << "metadata," << csvQuote(metadata.metadataResolved) << "\n";
    ts << "target_label," << csvQuote(target) << "\n";
    ts << "target_class_id," << csvQuote(target) << "\n";
    ts << "model_registry_entry_id," << csvQuote(metadata.model.registryEntryId) << "\n";
    ts << "model_state_at_start," << csvQuote(metadata.model.modelStateAtStart) << "\n";
    ts << "live_use_mode," << csvQuote(metadata.model.liveUseMode) << "\n";
    ts << "model_sha256," << csvQuote(metadata.model.modelSha256) << "\n";
    ts << "metadata_sha256," << csvQuote(metadata.model.metadataSha256) << "\n";
    ts << "fps," << QString::number(metadata.fps, 'f', 2) << "\n";
    ts << "frames_total," << metadata.totalFrames << "\n";
    ts << "events_detected," << events.size() << "\n";
    ts << "events_considered_fired_classified," << consideredEvents << "\n";
    ts << "classified_hit_count," << classifiedHitCount << "\n";
    ts << "classified_waste_count," << classifiedWasteCount << "\n";
    ts << "went_to_hit_count," << wentToHitCount << "\n";
    ts << "went_to_waste_count," << wentToWasteCount << "\n";
    ts << "motion_hit_count," << wentToHitCount << "\n";
    ts << "motion_waste_count," << wentToWasteCount << "\n";
    ts << "target_motion_hit_count," << truePositive << "\n";
    ts << "target_motion_waste_count," << falseNegative << "\n";
    ts << "non_target_motion_waste_count," << trueNegative << "\n";
    ts << "non_target_motion_hit_count," << falsePositive << "\n";
    ts << "target_vs_motion_alignment_rate," << QString::number(efficiency, 'f', 4) << "\n";
    ts << "target_motion_hit_precision," << QString::number(precision, 'f', 4) << "\n";
    ts << "target_motion_hit_recall," << QString::number(recall, 'f', 4) << "\n";

    ts << "\nclass,label,count\n";
    for (auto it = classCounts.begin(); it != classCounts.end(); ++it) {
        ts << "class," << csvQuote(it.key()) << "," << it.value() << "\n";
    }

    ts << "\ntarget_event_id,detected_frame,decision_frame,decision_dir,fired_frame,frames_tracked,start_y,end_y,"
          "cumulative_dy,path_length\n";
    for (const auto& rec : events) {
        QString labelLower = normalizeEventLabel(rec.label).toLower();
        bool isClassified = (labelLower != unclassifiedLower);
        if (rec.firedFrame < 0 || !isClassified)
            continue;
        if (labelLower != targetLower)
            continue;
        ts << rec.eventId << "," << rec.startFrame << "," << rec.decisionFrame << "," << csvQuote(rec.decisionDir)
           << "," << rec.firedFrame << "," << rec.framesTracked << "," << QString::number(rec.startY, 'f', 3) << ","
           << QString::number(rec.endY, 'f', 3) << "," << QString::number(rec.cumulativeDy, 'f', 3) << ","
           << QString::number(rec.pathLength, 'f', 3) << "\n";
    }
    ts.flush();
    file.close();
    return path;
}
