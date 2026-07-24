#include "run_writer_v2.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

using namespace desktop_app::v2::run;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

QString csvField(QString value) {
    if (value.contains('"'))
        value.replace("\"", "\"\"");
    if (value.contains(',') || value.contains('"') || value.contains('\r') ||
        value.contains('\n')) {
        return '"' + value + '"';
    }
    return value;
}

QString routeText(Route value) {
    if (value == Route::Hit)
        return "Hit";
    if (value == Route::Waste)
        return "Waste";
    return "Unresolved";
}

QString pulseText(DaqPulseStatus value) {
    switch (value) {
    case DaqPulseStatus::NotRequested:
        return "not_requested";
    case DaqPulseStatus::Requested:
        return "requested";
    case DaqPulseStatus::Issued:
        return "issued";
    case DaqPulseStatus::SuppressedNotIssued:
        return "suppressed_not_issued";
    case DaqPulseStatus::Failed:
        return "failed";
    }
    return {};
}

QByteArray csvRow(const RunEvent& event) {
    QStringList fields{
        event.eventId,
        event.detectionTimestamp,
        QString::number(event.sourceFrameIndex),
        event.effectiveConfigurationId,
        event.cropPath,
        event.predictedClassId.value_or(QString()),
    };
    for (int i = 0; i < 3; ++i) {
        fields.push_back(i < event.scores.size()
                             ? QString::number(event.scores.at(i), 'g', 17)
                             : QString());
    }
    fields.push_back(routeText(event.decision));
    fields.push_back(routeText(event.observedRoute));
    fields.push_back(pulseText(event.daqPulseStatus));
    fields.push_back(event.inferenceTimeMs
                         ? QString::number(*event.inferenceTimeMs, 'g', 17)
                         : QString());
    std::transform(fields.begin(), fields.end(), fields.begin(), csvField);
    return (fields.join(',') + '\n').toUtf8();
}

const QByteArray CsvHeader =
    "event_id,detection_timestamp,source_frame_index,effective_configuration_id,"
    "crop_path,predicted_class_id,score_class_0,score_class_1,score_class_2,"
    "decision,observed_route,daq_pulse_status,inference_time_ms\n";

bool safeCropPath(const QString& path) {
    return !path.isEmpty() && !QDir::isAbsolutePath(path) && !path.contains('\\') &&
           QDir::cleanPath(path) == path && path.startsWith("crops/");
}

bool safeRelativePath(const QString& path) {
    return !path.isEmpty() && !QDir::isAbsolutePath(path) && !path.contains('\\') &&
           QDir::cleanPath(path) == path && path != ".." && !path.startsWith("../");
}

bool safeOutputPath(const QString& root, const QString& relative, QString* error) {
    const QString canonicalRoot =
        QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath());
    if (canonicalRoot.isEmpty())
        return fail(error, "Run folder is not canonical.");
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    QString current = root;
    const QStringList parts = relative.split('/', Qt::SkipEmptyParts);
    for (int i = 0; i + 1 < parts.size(); ++i) {
        current = QDir(current).filePath(parts.at(i));
        const QFileInfo info(current);
        if (!info.exists() || !info.isDir() || info.isSymLink())
            return fail(error, "Droplet Crop path traverses an invalid directory.");
        const QString canonical =
            QDir::fromNativeSeparators(info.canonicalFilePath());
        if (canonical != canonicalRoot &&
            !canonical.startsWith(canonicalRoot + '/', sensitivity)) {
            return fail(error, "Droplet Crop path escapes the Run folder.");
        }
    }
    const QFileInfo target(QDir(root).filePath(relative));
    if (target.exists() || target.isSymLink())
        return fail(error, "Droplet Crop path already exists or is linked.");
    return true;
}

bool validateInitial(const RunManifestData& data, QString* error) {
    const bool sequenceTest = data.operation == RunOperation::SequenceTest;
    const bool liveSorting = data.operation == RunOperation::LiveSorting;
    if ((!sequenceTest && !liveSorting) ||
        data.runId.trimmed().isEmpty() || data.runName.trimmed().isEmpty() ||
        data.opendssVersion.trimmed().isEmpty() ||
        !QDateTime::fromString(data.startedAt, Qt::ISODate).isValid() ||
        (sequenceTest &&
         (data.sourceSequence.id.trimmed().isEmpty() ||
          data.sourceSequence.name.trimmed().isEmpty() ||
          !safeRelativePath(data.sourceSequence.manifestPath))) ||
        (liveSorting &&
         (!data.sourceSequence.id.isEmpty() || !data.sourceSequence.name.isEmpty() ||
          !data.sourceSequence.manifestPath.isEmpty())) ||
        (sequenceTest && (!std::isfinite(data.requestedProcessingFps) ||
                          data.requestedProcessingFps <= 0.0)) ||
        (liveSorting && data.requestedProcessingFps != 0.0) ||
        !std::isfinite(data.hitBoundary.boundaryY) ||
        data.hitBoundary.boundaryY < 0.0 ||
        data.hitBoundary.imageWidth <= 0 || data.hitBoundary.imageHeight <= 0 ||
        data.hitBoundary.boundaryY >= data.hitBoundary.imageHeight ||
        (data.hitBoundary.hitSide != HitSide::PositiveY &&
         data.hitBoundary.hitSide != HitSide::NegativeY) ||
        data.files.eventsCsv != "events.csv" || data.files.cropsPath != "crops" ||
        (data.files.sequencePath && !safeRelativePath(*data.files.sequencePath))) {
        return fail(error, "Initial Run metadata is invalid.");
    }
    if (data.routing.triggerMode != TriggerMode::ClassBased &&
        data.routing.triggerMode != TriggerMode::EveryDroplet)
        return fail(error, "Initial routing trigger mode is invalid.");
    if (liveSorting && !data.routing.physicalDaqOutputEnabled)
        return fail(error, "Live Sorting requires physical DAQ output.");
    if (data.requestedDurationSeconds &&
        (!std::isfinite(*data.requestedDurationSeconds) ||
         *data.requestedDurationSeconds <= 0.0)) {
        return fail(error, "Requested duration must be null or finite and positive.");
    }
    if (data.model) {
        if (data.model->id.trimmed().isEmpty() || data.model->name.trimmed().isEmpty() ||
            !QRegularExpression("^[0-9a-fA-F]{64}$").match(data.model->sha256).hasMatch() ||
            (data.model->classes.size() != 2 && data.model->classes.size() != 3)) {
            return fail(error, "Initial model snapshot is invalid.");
        }
        QSet<QString> ids;
        for (const auto& cls : data.model->classes) {
            if (cls.id.trimmed().isEmpty() || cls.name.trimmed().isEmpty() ||
                ids.contains(cls.id)) {
                return fail(error, "Initial model classes are invalid.");
            }
            ids.insert(cls.id);
        }
    }
    if (data.routing.triggerMode == TriggerMode::ClassBased) {
        if (!data.model || !data.routing.hitClassId ||
            std::none_of(data.model->classes.begin(), data.model->classes.end(),
                         [&](const RunClassSnapshot& cls) {
                             return cls.id == *data.routing.hitClassId;
                         })) {
            return fail(error, "Class-Based Sorting requires a valid model and Hit Class.");
        }
    } else if (data.routing.hitClassId) {
        return fail(error, "Trigger Every Droplet must not contain a Hit Class.");
    }
    return true;
}

bool validateEventForAppend(const RunManifestData& data, const RunEvent& event,
                            QString* error) {
    if (event.eventId.trimmed().isEmpty() ||
        !QDateTime::fromString(event.detectionTimestamp, Qt::ISODate).isValid() ||
        event.sourceFrameIndex <= 0 || event.effectiveConfigurationId != "initial" ||
        !safeCropPath(event.cropPath) ||
        (event.decision != Route::Hit && event.decision != Route::Waste) ||
        (event.observedRoute != Route::Hit && event.observedRoute != Route::Waste &&
         event.observedRoute != Route::Unresolved)) {
        return fail(error, "Run event identity, timestamp, frame, crop, decision, or configuration is invalid.");
    }
    if (std::any_of(data.events.begin(), data.events.end(), [&](const RunEvent& old) {
            return old.eventId == event.eventId || old.cropPath == event.cropPath;
        })) {
        return fail(error, "Run event ID and crop path must be unique.");
    }
    if (!data.model) {
        if (event.predictedClassId || !event.scores.isEmpty() || event.inferenceTimeMs)
            return fail(error, "Events without a model cannot contain inference facts.");
    } else {
        const bool known =
            event.predictedClassId &&
            std::any_of(data.model->classes.begin(), data.model->classes.end(),
                        [&](const RunClassSnapshot& cls) {
                            return cls.id == *event.predictedClassId;
                        });
        if (!known || event.scores.size() != data.model->classes.size() ||
            std::any_of(event.scores.begin(), event.scores.end(),
                        [](double value) { return !std::isfinite(value); }) ||
            !event.inferenceTimeMs || !std::isfinite(*event.inferenceTimeMs) ||
            *event.inferenceTimeMs < 0.0) {
            return fail(error, "Modeled events require a valid prediction, scores, and inference time.");
        }
        int bestIndex = 0;
        for (int i = 1; i < event.scores.size(); ++i) {
            if (event.scores.at(i) > event.scores.at(bestIndex))
                bestIndex = i;
        }
        if (*event.predictedClassId != data.model->classes.at(bestIndex).id)
            return fail(error, "Predicted Class ID must be the first argmax Class Score.");
    }
    if (data.routing.triggerMode == TriggerMode::EveryDroplet &&
        event.decision != Route::Hit) {
        return fail(error, "Trigger Every Droplet events must have a Hit decision.");
    }
    if (data.routing.triggerMode == TriggerMode::ClassBased) {
        const Route expected = event.predictedClassId == data.routing.hitClassId
                                   ? Route::Hit
                                   : Route::Waste;
        if (event.decision != expected)
            return fail(error, "Class-Based decision does not match the Hit Class.");
    }
    if (event.daqPulseStatus == DaqPulseStatus::Requested)
        return fail(error, "Finalized events cannot retain requested DAQ status.");
    if (event.decision == Route::Waste &&
        event.daqPulseStatus != DaqPulseStatus::NotRequested)
        return fail(error, "Waste decisions must use not_requested DAQ status.");
    if (event.decision == Route::Hit && !data.routing.physicalDaqOutputEnabled &&
        event.daqPulseStatus != DaqPulseStatus::SuppressedNotIssued)
        return fail(error, "DAQ-disabled Hit decisions must use suppressed_not_issued.");
    if (event.decision == Route::Hit && data.routing.physicalDaqOutputEnabled &&
        event.daqPulseStatus != DaqPulseStatus::Issued &&
        event.daqPulseStatus != DaqPulseStatus::Failed &&
        event.daqPulseStatus != DaqPulseStatus::SuppressedNotIssued)
        return fail(error, "DAQ-enabled Hit decisions require a final factual pulse status.");
    return true;
}

bool atomicWrite(const QString& path, const QByteArray& bytes, QString* error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QString("Could not open '%1': %2").arg(path, file.errorString()));
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return fail(error, QString("Could not completely write '%1'.").arg(path));
    }
    if (!file.commit())
        return fail(error, QString("Could not publish '%1': %2").arg(path, file.errorString()));
    return true;
}

} // namespace

namespace desktop_app::v2::run {

std::optional<RunWriterV2> RunWriterV2::start(const QString& runFolder,
                                               RunManifestData initialData,
                                               QString* error) {
    if (error)
        error->clear();
    if (QFileInfo::exists(runFolder))
        return fail(error, "Run folder already exists."), std::nullopt;
    if (!validateInitial(initialData, error))
        return std::nullopt;
    initialData.events.clear();
    initialData.status = RunStatus::Interrupted;
    initialData.endedAt.clear();
    initialData.achievedProcessingFps = 0.0;
    initialData.stopReason = QStringLiteral("operation_in_progress");
    QDir directory;
    if (!directory.mkpath(QDir(runFolder).filePath("crops")))
        return fail(error, "Could not create the Run folder."), std::nullopt;
    auto partial = std::make_unique<QFile>(
        QDir(runFolder).filePath(QStringLiteral("events.partial.csv")));
    if (!partial->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(error, "Could not create events.partial.csv.");
        return std::nullopt;
    }
    if (partial->write(CsvHeader) != CsvHeader.size() || !partial->flush()) {
        fail(error, "Could not initialize events.partial.csv.");
        return std::nullopt;
    }
    initialData.endedAt = initialData.startedAt;
    if (!RunManifestV2::savePartial(
            QDir(runFolder).filePath(QStringLiteral("run_summary.partial.json")),
            initialData, error)) {
        return std::nullopt;
    }
    return RunWriterV2(runFolder, std::move(initialData), std::move(partial));
}

RunWriterV2::RunWriterV2(QString runFolder, RunManifestData data,
                         std::unique_ptr<QFile> partialFile)
    : runFolder_(std::move(runFolder)), data_(std::move(data)),
      partialFile_(std::move(partialFile)) {}

RunWriterV2::RunWriterV2(RunWriterV2&&) noexcept = default;
RunWriterV2& RunWriterV2::operator=(RunWriterV2&&) noexcept = default;
RunWriterV2::~RunWriterV2() = default;

bool RunWriterV2::appendEvent(const RunEvent& event, const QByteArray& cropBytes,
                              QString* error) {
    if (error)
        error->clear();
    if (finalized_ || !partialFile_ || !partialFile_->isOpen())
        return fail(error, "Run writer is not active.");
    if (cropBytes.isEmpty())
        return fail(error, "Droplet Crop bytes must not be empty.");
    if (!validateEventForAppend(data_, event, error))
        return false;
    const QString cropFile = QDir(runFolder_).filePath(event.cropPath);
    if (!safeOutputPath(runFolder_, event.cropPath, error))
        return false;
    if (!QDir().mkpath(QFileInfo(cropFile).absolutePath()) ||
        !atomicWrite(cropFile, cropBytes, error)) {
        return false;
    }
    const qint64 priorOffset = partialFile_->pos();
    const QByteArray row = csvRow(event);
    bool rowWritten = false;
    if (failNextCsvAppendForTest_) {
        failNextCsvAppendForTest_ = false;
        partialFile_->write(row.left(row.size() / 2));
        partialFile_->flush();
    } else {
        rowWritten = partialFile_->write(row) == row.size() && partialFile_->flush();
    }
    if (!rowWritten) {
        const bool rolledBack = partialFile_->resize(priorOffset) &&
                                partialFile_->seek(priorOffset) &&
                                partialFile_->flush();
        QFile::remove(cropFile);
        return fail(error, rolledBack ? "Could not append the finalized event."
                                      : "CSV append and rollback both failed.");
    }
    data_.events.push_back(event);
    return true;
}

bool RunWriterV2::checkpoint(const RunIntegrity& integrity, QString* error) {
    if (error)
        error->clear();
    if (finalized_ || !partialFile_ || !partialFile_->isOpen())
        return fail(error, "Run writer is not active.");
    if (!partialFile_->flush())
        return fail(error, "Could not flush events.partial.csv.");
    const RunIntegrity previous = data_.integrity;
    const RunStatus previousStatus = data_.status;
    data_.integrity = integrity;
    if (integrity.queueRejections.count > 0 ||
        integrity.consumerFailures.count > 0) {
        data_.status = RunStatus::Failed;
    }
    if (RunManifestV2::savePartial(
            QDir(runFolder_).filePath(QStringLiteral("run_summary.partial.json")),
            data_, error)) {
        return true;
    }
    data_.integrity = previous;
    data_.status = previousStatus;
    return false;
}

bool RunWriterV2::flush(QString* error) {
    if (error)
        error->clear();
    if (finalized_)
        return true;
    if (!partialFile_ || !partialFile_->isOpen() || !partialFile_->flush())
        return fail(error, "Could not flush events.partial.csv.");
    return true;
}

bool RunWriterV2::finalize(RunStatus status, const QString& endedAt,
                           const QString& stopReason, double achievedProcessingFps,
                           QString* error) {
    if (error)
        error->clear();
    if (finalized_)
        return fail(error, "Run has already been finalized.");
    if (status != RunStatus::Completed && status != RunStatus::Stopped &&
        status != RunStatus::Interrupted && status != RunStatus::Failed)
        return fail(error, "Final Run status is invalid.");
    const bool sourceLoss = data_.integrity.sourceFrameGaps.count > 0;
    const bool eventLoss = data_.integrity.queueRejections.count > 0 ||
                           data_.integrity.consumerFailures.count > 0;
    if ((eventLoss && status != RunStatus::Failed) ||
        (sourceLoss && status != RunStatus::Interrupted &&
         status != RunStatus::Failed)) {
        return fail(error, "Final Run status does not match recorded integrity loss.");
    }
    const auto ended = QDateTime::fromString(endedAt, Qt::ISODate);
    const auto started = QDateTime::fromString(data_.startedAt, Qt::ISODate);
    if (!ended.isValid() || ended < started || stopReason.trimmed().isEmpty() ||
        !std::isfinite(achievedProcessingFps) || achievedProcessingFps < 0.0 ||
        (data_.operation == RunOperation::SequenceTest &&
         status == RunStatus::Completed && achievedProcessingFps <= 0.0) ||
        (data_.operation == RunOperation::LiveSorting &&
         achievedProcessingFps != 0.0)) {
        return fail(error, "Final Run timing, stop reason, or achieved FPS is invalid.");
    }
    if (!checkpoint(data_.integrity, error))
        return false;
    partialFile_->close();

    const QString partialPath =
        QDir(runFolder_).filePath(QStringLiteral("events.partial.csv"));
    const QString finalPath = QDir(runFolder_).filePath(QStringLiteral("events.csv"));
    QFile partial(partialPath);
    if (!partial.open(QIODevice::ReadOnly))
        return fail(error, "Could not reopen events.partial.csv.");
    const QByteArray partialBytes = partial.readAll();
    partial.close();
    if (QFileInfo::exists(finalPath)) {
        QFile finalFile(finalPath);
        if (!finalFile.open(QIODevice::ReadOnly) || finalFile.readAll() != partialBytes)
            return fail(error, "Existing events.csv does not match the partial log.");
    } else if (!atomicWrite(finalPath, partialBytes, error)) {
        return false;
    }

    data_.status = status;
    data_.endedAt = endedAt;
    data_.stopReason = stopReason;
    data_.achievedProcessingFps = achievedProcessingFps;
    if (!RunManifestV2::savePartial(
            QDir(runFolder_).filePath(QStringLiteral("run_summary.partial.json")),
            data_, error)) {
        return false;
    }
    if (!RunManifestV2::save(
            QDir(runFolder_).filePath(QStringLiteral("run_summary.json")), data_, error)) {
        return false;
    }
    if (status == RunStatus::Completed || status == RunStatus::Stopped) {
        const QString partialSummary =
            QDir(runFolder_).filePath(QStringLiteral("run_summary.partial.json"));
        if (!QFile::remove(partialPath) || !QFile::remove(partialSummary))
            return fail(error, "Cleanly finalized Run could not remove recoverable partial files.");
    }
    finalized_ = true;
    return true;
}

const RunManifestData& RunWriterV2::data() const noexcept {
    return data_;
}

} // namespace desktop_app::v2::run
