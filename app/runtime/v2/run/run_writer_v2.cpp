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

bool validateInitial(const RunManifestData& data, QString* error) {
    if (data.operation != RunOperation::SequenceTest ||
        data.runId.trimmed().isEmpty() || data.runName.trimmed().isEmpty() ||
        data.opendssVersion.trimmed().isEmpty() ||
        !QDateTime::fromString(data.startedAt, Qt::ISODate).isValid() ||
        data.sourceSequence.id.trimmed().isEmpty() ||
        data.sourceSequence.name.trimmed().isEmpty() ||
        !safeRelativePath(data.sourceSequence.manifestPath) ||
        !std::isfinite(data.requestedProcessingFps) ||
        data.requestedProcessingFps <= 0.0 ||
        data.files.eventsCsv != "events.csv" || data.files.cropsPath != "crops" ||
        (data.files.sequencePath && !safeRelativePath(*data.files.sequencePath))) {
        return fail(error, "Initial Sequence Test Run metadata is invalid.");
    }
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
        !safeCropPath(event.cropPath) || event.decision == Route::Unresolved) {
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
    if ((!data.routing.physicalDaqOutputEnabled &&
         event.daqPulseStatus != DaqPulseStatus::NotRequested) ||
        (data.routing.physicalDaqOutputEnabled &&
         event.daqPulseStatus == DaqPulseStatus::NotRequested)) {
        return fail(error, "DAQ pulse status does not match physical output configuration.");
    }
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
    initialData.endedAt.clear();
    initialData.achievedProcessingFps = 0.0;
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
    if (QFileInfo::exists(cropFile))
        return fail(error, "Droplet Crop path already exists.");
    if (!QDir().mkpath(QFileInfo(cropFile).absolutePath()) ||
        !atomicWrite(cropFile, cropBytes, error)) {
        return false;
    }
    const QByteArray row = csvRow(event);
    if (partialFile_->write(row) != row.size() || !partialFile_->flush()) {
        QFile::remove(cropFile);
        return fail(error, "Could not append the finalized event.");
    }
    data_.events.push_back(event);
    return true;
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
    const auto ended = QDateTime::fromString(endedAt, Qt::ISODate);
    const auto started = QDateTime::fromString(data_.startedAt, Qt::ISODate);
    if (!ended.isValid() || ended < started || stopReason.trimmed().isEmpty() ||
        !std::isfinite(achievedProcessingFps) || achievedProcessingFps <= 0.0) {
        return fail(error, "Final Run timing, stop reason, or achieved FPS is invalid.");
    }
    if (!flush(error))
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
    if (!RunManifestV2::save(
            QDir(runFolder_).filePath(QStringLiteral("run_summary.json")), data_, error)) {
        return false;
    }
    if (status == RunStatus::Completed && !QFile::remove(partialPath))
        return fail(error, "Run summary was saved, but the completed partial log could not be removed.");
    finalized_ = true;
    return true;
}

const RunManifestData& RunWriterV2::data() const noexcept {
    return data_;
}

} // namespace desktop_app::v2::run
