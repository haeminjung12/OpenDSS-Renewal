#include "run_manifest_v2.h"

#include "../../desktop_app/json_persistence.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace {

using namespace desktop_app::v2::run;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

bool only(const QJsonObject& object, std::initializer_list<const char*> fields,
          const QString& context, QString* error) {
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const bool known = std::any_of(fields.begin(), fields.end(), [&](const char* field) {
            return it.key() == QLatin1String(field);
        });
        if (!known)
            return fail(error, QString("Unknown field '%1' in %2.").arg(it.key(), context));
    }
    return true;
}

bool string(const QJsonObject& object, const char* key, QString& out, bool empty,
            QString* error) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString())
        return fail(error, QString("Field '%1' must be a string.").arg(key));
    out = value.toString();
    return empty || !out.trimmed().isEmpty()
               ? true
               : fail(error, QString("Field '%1' must not be empty.").arg(key));
}

bool timestamp(const QJsonObject& object, const char* key, QString& out, QString* error) {
    return string(object, key, out, false, error) &&
           (QDateTime::fromString(out, Qt::ISODate).isValid() ||
            fail(error, QString("Field '%1' must be an ISO-8601 timestamp.").arg(key)));
}

bool finitePositive(const QJsonValue& value, double& out, const QString& name,
                    QString* error) {
    if (!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble() <= 0.0)
        return fail(error, name + " must be a finite positive number.");
    out = value.toDouble();
    return true;
}

bool safeRelative(const QString& path, bool allowDirectory, QString* error) {
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains('\\')) {
        return fail(error, "Artifact paths must be nonempty relative paths using '/'.");
    }
    const QString clean = QDir::cleanPath(path);
    if (clean != path || clean == ".." || clean.startsWith("../") ||
        (!allowDirectory && clean == ".")) {
        return fail(error, "Artifact path is not contained in the Run folder.");
    }
    return true;
}

bool containedExistingFile(const QString& root, const QString& relative, QString* error) {
    const QString canonicalRoot =
        QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath());
    const QFileInfo candidate(QDir(root).filePath(relative));
    const QString canonicalCandidate =
        QDir::fromNativeSeparators(candidate.canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty() || !candidate.isFile())
        return fail(error, QString("Missing artifact '%1'.").arg(relative));
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    const QString prefix = canonicalRoot + '/';
    if (!canonicalCandidate.startsWith(prefix, sensitivity))
        return fail(error, QString("Artifact '%1' escapes the Run folder.").arg(relative));

    QString current = root;
    const QStringList parts = relative.split('/', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        current = QDir(current).filePath(part);
        const QFileInfo info(current);
        if (info.isSymLink())
            return fail(error, QString("Artifact '%1' traverses a link.").arg(relative));
    }
    return true;
}

QString operationText(RunOperation value) {
    return value == RunOperation::SequenceTest ? "sequence_test" : "live_sorting";
}
QString statusText(RunStatus value) {
    if (value == RunStatus::Completed)
        return "completed";
    if (value == RunStatus::Interrupted)
        return "interrupted";
    return "failed";
}
QString triggerText(TriggerMode value) {
    return value == TriggerMode::ClassBased ? "class_based" : "every_droplet";
}
QString hitSideText(HitSide value) {
    return value == HitSide::PositiveY ? "positive_y" : "negative_y";
}
template <typename T>
bool parseEnum(const QString& text, std::initializer_list<std::pair<const char*, T>> values,
               T& out) {
    for (const auto& [name, value] : values) {
        if (text == QLatin1String(name)) {
            out = value;
            return true;
        }
    }
    return false;
}

bool parseCsv(const QByteArray& bytes, QVector<QStringList>& rows, QString* error) {
    const QString text = QString::fromUtf8(bytes);
    QStringList row;
    QString field;
    bool quoted = false;
    bool afterQuote = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text.at(i + 1) == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                    afterQuote = true;
                }
            } else {
                field += c;
            }
        } else if (afterQuote) {
            if (c == ',') {
                row.push_back(field);
                field.clear();
                afterQuote = false;
            } else if (c == '\n' || c == '\r') {
                row.push_back(field);
                field.clear();
                afterQuote = false;
                rows.push_back(row);
                row.clear();
                if (c == '\r' && i + 1 < text.size() && text.at(i + 1) == '\n')
                    ++i;
            } else {
                return fail(error, "Invalid character after a quoted CSV field.");
            }
        } else if (c == '"' && field.isEmpty()) {
            quoted = true;
        } else if (c == ',') {
            row.push_back(field);
            field.clear();
        } else if (c == '\n' || c == '\r') {
            row.push_back(field);
            field.clear();
            rows.push_back(row);
            row.clear();
            if (c == '\r' && i + 1 < text.size() && text.at(i + 1) == '\n')
                ++i;
        } else if (c == '"') {
            return fail(error, "Unescaped quote in CSV field.");
        } else {
            field += c;
        }
    }
    if (quoted)
        return fail(error, "Unterminated quoted CSV field.");
    if (afterQuote || !field.isEmpty() || !row.isEmpty()) {
        row.push_back(field);
        rows.push_back(row);
    }
    return true;
}

const QStringList CsvHeader{
    "event_id",          "detection_timestamp", "source_frame_index",
    "effective_configuration_id", "crop_path", "predicted_class_id",
    "score_class_0",     "score_class_1",        "score_class_2",
    "decision",          "observed_route",       "daq_pulse_status",
    "inference_time_ms",
};

bool validateModel(const std::optional<ModelSnapshot>& model, QString* error) {
    if (!model)
        return true;
    if (model->id.trimmed().isEmpty() || model->name.trimmed().isEmpty() ||
        !QRegularExpression("^[0-9a-fA-F]{64}$").match(model->sha256).hasMatch()) {
        return fail(error, "Model identity, name, and SHA-256 are required.");
    }
    if (model->classes.size() != 2 && model->classes.size() != 3)
        return fail(error, "Model must contain exactly two or three ordered classes.");
    QSet<QString> ids;
    for (const auto& item : model->classes) {
        if (item.id.trimmed().isEmpty() || item.name.trimmed().isEmpty() ||
            ids.contains(item.id)) {
            return fail(error, "Model Class IDs and names must be nonempty and unique.");
        }
        ids.insert(item.id);
    }
    return true;
}

bool validateEvent(const RunManifestData& data, const RunEvent& event, QString* error) {
    if (event.eventId.trimmed().isEmpty() ||
        !QDateTime::fromString(event.detectionTimestamp, Qt::ISODate).isValid() ||
        event.sourceFrameIndex <= 0 || event.effectiveConfigurationId != "initial") {
        return fail(error, "Event identity, timestamp, frame index, or configuration is invalid.");
    }
    if (!safeRelative(event.cropPath, false, error) ||
        !(event.cropPath == "crops" || event.cropPath.startsWith("crops/"))) {
        return fail(error, "Event crop_path must be inside crops/.");
    }
    if (!data.model) {
        if (event.predictedClassId || !event.scores.isEmpty() || event.inferenceTimeMs)
            return fail(error, "Events without a model cannot contain inference facts.");
    } else {
        if (!event.predictedClassId || event.scores.size() != data.model->classes.size() ||
            !event.inferenceTimeMs || !std::isfinite(*event.inferenceTimeMs) ||
            *event.inferenceTimeMs < 0.0) {
            return fail(error, "Modeled events require a prediction, one score per class, and inference time.");
        }
        bool known = false;
        for (int i = 0; i < event.scores.size(); ++i) {
            known = known || data.model->classes.at(i).id == *event.predictedClassId;
            if (!std::isfinite(event.scores.at(i)))
                return fail(error, "Class Scores must be finite.");
        }
        if (!known)
            return fail(error, "Predicted Class ID is not in the model snapshot.");
        int bestIndex = 0;
        for (int i = 1; i < event.scores.size(); ++i) {
            if (event.scores.at(i) > event.scores.at(bestIndex))
                bestIndex = i;
        }
        if (*event.predictedClassId != data.model->classes.at(bestIndex).id)
            return fail(error, "Predicted Class ID must be the first argmax Class Score.");
    }
    if (event.decision != Route::Hit && event.decision != Route::Waste)
        return fail(error, "Decision must be Hit or Waste.");
    if (event.observedRoute != Route::Hit && event.observedRoute != Route::Waste &&
        event.observedRoute != Route::Unresolved)
        return fail(error, "Observed Route is invalid.");
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

RunDerivedCounts derive(const RunManifestData& data) {
    RunDerivedCounts result;
    if (data.model)
        result.predictedByClass.fill(0, data.model->classes.size());
    for (const auto& event : data.events) {
        ++result.total;
        if (event.predictedClassId && data.model) {
            for (int i = 0; i < data.model->classes.size(); ++i) {
                if (data.model->classes.at(i).id == *event.predictedClassId)
                    ++result.predictedByClass[i];
            }
        } else {
            ++result.unclassified;
        }
        event.decision == Route::Hit ? ++result.decisionHit : ++result.decisionWaste;
        if (event.observedRoute == Route::Hit)
            ++result.observedHit;
        else if (event.observedRoute == Route::Waste)
            ++result.observedWaste;
        else
            ++result.observedUnresolved;

        if (event.decision == Route::Hit && event.observedRoute == Route::Hit)
            ++result.hitDecisionHitObserved;
        else if (event.decision == Route::Hit && event.observedRoute == Route::Waste)
            ++result.hitDecisionWasteObserved;
        else if (event.decision == Route::Hit)
            ++result.hitDecisionUnresolved;
        else if (event.observedRoute == Route::Hit)
            ++result.wasteDecisionHitObserved;
        else if (event.observedRoute == Route::Waste)
            ++result.wasteDecisionWasteObserved;
        else
            ++result.wasteDecisionUnresolved;
    }
    return result;
}

bool validateData(const RunManifestData& data, QString* error) {
    if (data.runId.trimmed().isEmpty() || data.runName.trimmed().isEmpty() ||
        data.stopReason.trimmed().isEmpty() || data.opendssVersion.trimmed().isEmpty()) {
        return fail(error, "Run identity, name, stop reason, and OpenDSS version are required.");
    }
    if (data.operation != RunOperation::SequenceTest)
        return fail(error, "live_sorting is not supported by the Sequence Test contract gate.");
    if (data.status != RunStatus::Completed && data.status != RunStatus::Interrupted &&
        data.status != RunStatus::Failed)
        return fail(error, "Run status is invalid.");
    const auto start = QDateTime::fromString(data.startedAt, Qt::ISODate);
    const auto end = QDateTime::fromString(data.endedAt, Qt::ISODate);
    if (!start.isValid() || !end.isValid() || end < start)
        return fail(error, "Run timestamps must be valid and ordered.");
    if (data.requestedDurationSeconds &&
        (!std::isfinite(*data.requestedDurationSeconds) ||
         *data.requestedDurationSeconds <= 0.0)) {
        return fail(error, "Requested duration must be null or finite and positive.");
    }
    if (data.sourceSequence.id.trimmed().isEmpty() ||
        data.sourceSequence.name.trimmed().isEmpty() ||
        !safeRelative(data.sourceSequence.manifestPath, false, error)) {
        return fail(error, "Sequence Test requires source Sequence identity and a contained manifest path.");
    }
    if (!validateModel(data.model, error))
        return false;
    if (data.routing.triggerMode != TriggerMode::ClassBased &&
        data.routing.triggerMode != TriggerMode::EveryDroplet)
        return fail(error, "Routing trigger mode is invalid.");
    if (data.routing.triggerMode == TriggerMode::ClassBased) {
        if (!data.model || !data.routing.hitClassId)
            return fail(error, "Class-Based Sorting requires a model and Hit Class.");
        const bool known = std::any_of(data.model->classes.begin(), data.model->classes.end(),
                                       [&](const auto& item) {
                                           return item.id == *data.routing.hitClassId;
                                       });
        if (!known)
            return fail(error, "Hit Class is not in the model snapshot.");
    } else if (data.routing.hitClassId) {
        return fail(error, "Trigger Every Droplet must not contain a Hit Class.");
    }
    if (!std::isfinite(data.requestedProcessingFps) || data.requestedProcessingFps <= 0.0 ||
        !std::isfinite(data.achievedProcessingFps) || data.achievedProcessingFps < 0.0 ||
        (data.status == RunStatus::Completed && data.achievedProcessingFps <= 0.0)) {
        return fail(error, "Sequence Test processing FPS values are invalid for Run status.");
    }
    if (!std::isfinite(data.hitBoundary.boundaryY) ||
        data.hitBoundary.imageWidth <= 0 || data.hitBoundary.imageHeight <= 0 ||
        (data.hitBoundary.hitSide != HitSide::PositiveY &&
         data.hitBoundary.hitSide != HitSide::NegativeY))
        return fail(error, "Hit boundary snapshot is invalid.");
    if (data.files.eventsCsv != "events.csv" || data.files.cropsPath != "crops")
        return fail(error, "Run files must use events.csv and crops.");
    if (data.files.sequencePath &&
        !safeRelative(*data.files.sequencePath, true, error))
        return false;
    QSet<QString> ids;
    QSet<QString> crops;
    for (const auto& event : data.events) {
        if (!validateEvent(data, event, error))
            return false;
        if (ids.contains(event.eventId) || crops.contains(event.cropPath))
            return fail(error, "Event IDs and crop paths must be unique.");
        ids.insert(event.eventId);
        crops.insert(event.cropPath);
    }
    return true;
}

QJsonObject derivedJson(const RunManifestData& data, const RunDerivedCounts& value) {
    QJsonArray predicted;
    if (data.model) {
        for (int i = 0; i < data.model->classes.size(); ++i) {
            predicted.push_back(QJsonObject{{"class_id", data.model->classes.at(i).id},
                                            {"class_name", data.model->classes.at(i).name},
                                            {"count", value.predictedByClass.at(i)}});
        }
    }
    return QJsonObject{
        {"total", value.total},
        {"predicted_classes", predicted},
        {"unclassified", value.unclassified},
        {"decision", QJsonObject{{"hit", value.decisionHit}, {"waste", value.decisionWaste}}},
        {"observed", QJsonObject{{"hit", value.observedHit},
                                  {"waste", value.observedWaste},
                                  {"unresolved", value.observedUnresolved}}},
    };
}

QJsonObject matrixJson(const RunDerivedCounts& value) {
    return QJsonObject{{"hit_decision_hit_observed", value.hitDecisionHitObserved},
                       {"hit_decision_waste_observed", value.hitDecisionWasteObserved},
                       {"hit_decision_unresolved", value.hitDecisionUnresolved},
                       {"waste_decision_hit_observed", value.wasteDecisionHitObserved},
                       {"waste_decision_waste_observed", value.wasteDecisionWasteObserved},
                       {"waste_decision_unresolved", value.wasteDecisionUnresolved}};
}

bool exactObject(const QJsonObject& actual, const QJsonObject& expected,
                 const QString& name, QString* error) {
    return actual == expected || fail(error, name + " does not match finalized events.");
}

std::optional<RunEvent> eventFromRow(const QStringList& row, QString* error) {
    if (row.size() != CsvHeader.size()) {
        fail(error, "Droplet Log row has the wrong column count.");
        return std::nullopt;
    }
    RunEvent event;
    event.eventId = row.at(0);
    event.detectionTimestamp = row.at(1);
    bool integerOk = false;
    event.sourceFrameIndex = row.at(2).toLongLong(&integerOk);
    if (!integerOk) {
        fail(error, "source_frame_index must be an integer.");
        return std::nullopt;
    }
    event.effectiveConfigurationId = row.at(3);
    event.cropPath = row.at(4);
    if (!row.at(5).isEmpty())
        event.predictedClassId = row.at(5);
    bool emptyScoreSeen = false;
    for (int i = 6; i <= 8; ++i) {
        if (row.at(i).isEmpty()) {
            emptyScoreSeen = true;
            continue;
        }
        if (emptyScoreSeen) {
            fail(error, "Class Score columns must be contiguous.");
            return std::nullopt;
        }
        bool ok = false;
        const double value = row.at(i).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            fail(error, "Class Score must be a finite number.");
            return std::nullopt;
        }
        event.scores.push_back(value);
    }
    if (!parseEnum(row.at(9), {{"Hit", Route::Hit}, {"Waste", Route::Waste}},
                   event.decision) ||
        !parseEnum(row.at(10), {{"Hit", Route::Hit}, {"Waste", Route::Waste},
                                {"Unresolved", Route::Unresolved}},
                   event.observedRoute) ||
        !parseEnum(row.at(11),
                   {{"not_requested", DaqPulseStatus::NotRequested},
                    {"requested", DaqPulseStatus::Requested},
                    {"issued", DaqPulseStatus::Issued},
                    {"suppressed_not_issued", DaqPulseStatus::SuppressedNotIssued},
                    {"failed", DaqPulseStatus::Failed}},
                   event.daqPulseStatus)) {
        fail(error, "Droplet Log contains an unsupported enum value.");
        return std::nullopt;
    }
    if (!row.at(12).isEmpty()) {
        bool ok = false;
        const double value = row.at(12).toDouble(&ok);
        if (!ok || !std::isfinite(value) || value < 0.0) {
            fail(error, "inference_time_ms must be finite and nonnegative.");
            return std::nullopt;
        }
        event.inferenceTimeMs = value;
    }
    return event;
}

bool loadEvents(const QString& path, QVector<RunEvent>& events, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, "Could not read events.csv.");
    QVector<QStringList> rows;
    if (!parseCsv(file.readAll(), rows, error) || rows.isEmpty() || rows.takeFirst() != CsvHeader)
        return error && !error->isEmpty() ? false : fail(error, "events.csv header is invalid.");
    for (const auto& row : rows) {
        if (row.size() == 1 && row.first().isEmpty())
            continue;
        auto event = eventFromRow(row, error);
        if (!event)
            return false;
        events.push_back(*event);
    }
    return true;
}

} // namespace

namespace desktop_app::v2::run {

std::optional<RunManifestV2> RunManifestV2::load(const QString& path, QString* error) {
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, "Could not read run_summary.json.");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, "run_summary.json is not a valid JSON object.");
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value("schema_version").toString() != SchemaVersion ||
        !only(root, {"schema_version", "run_id", "run_name", "operation",
                     "experiment_type", "notes", "status", "started_at", "ended_at",
                     "requested_duration_seconds", "stop_reason", "opendss_version",
                     "source_sequence", "model", "routing", "settings", "processing",
                     "hit_boundary", "counts", "decision_vs_observed", "files"},
              "Run root", error)) {
        if (error && error->isEmpty())
            *error = "Unsupported Run schema_version.";
        return std::nullopt;
    }

    RunManifestV2 manifest;
    auto& data = manifest.data_;
    if (!string(root, "run_id", data.runId, false, error) ||
        !string(root, "run_name", data.runName, false, error) ||
        !string(root, "experiment_type", data.experimentType, true, error) ||
        !string(root, "notes", data.notes, true, error) ||
        !timestamp(root, "started_at", data.startedAt, error) ||
        !timestamp(root, "ended_at", data.endedAt, error) ||
        !string(root, "stop_reason", data.stopReason, false, error) ||
        !string(root, "opendss_version", data.opendssVersion, false, error)) {
        return std::nullopt;
    }
    if (!parseEnum(root.value("operation").toString(),
                   {{"sequence_test", RunOperation::SequenceTest},
                    {"live_sorting", RunOperation::LiveSorting}},
                   data.operation) ||
        !parseEnum(root.value("status").toString(),
                   {{"completed", RunStatus::Completed},
                    {"interrupted", RunStatus::Interrupted},
                    {"failed", RunStatus::Failed}},
                   data.status)) {
        fail(error, "Run operation or status is unsupported.");
        return std::nullopt;
    }
    const auto duration = root.value("requested_duration_seconds");
    if (duration.isNull()) {
        data.requestedDurationSeconds.reset();
    } else {
        double value = 0.0;
        if (!finitePositive(duration, value, "requested_duration_seconds", error))
            return std::nullopt;
        data.requestedDurationSeconds = value;
    }

    if (!root.value("source_sequence").isObject()) {
        fail(error, "source_sequence must be an object.");
        return std::nullopt;
    }
    const auto source = root.value("source_sequence").toObject();
    if (!only(source, {"sequence_id", "sequence_name", "manifest_path"},
              "source_sequence", error) ||
        !string(source, "sequence_id", data.sourceSequence.id, false, error) ||
        !string(source, "sequence_name", data.sourceSequence.name, false, error) ||
        !string(source, "manifest_path", data.sourceSequence.manifestPath, false, error)) {
        return std::nullopt;
    }

    if (root.value("model").isNull()) {
        data.model.reset();
    } else if (root.value("model").isObject()) {
        ModelSnapshot model;
        const auto object = root.value("model").toObject();
        if (!only(object, {"model_id", "model_name", "model_sha256", "classes"},
                  "model", error) ||
            !string(object, "model_id", model.id, false, error) ||
            !string(object, "model_name", model.name, false, error) ||
            !string(object, "model_sha256", model.sha256, false, error) ||
            !object.value("classes").isArray()) {
            return std::nullopt;
        }
        for (const auto value : object.value("classes").toArray()) {
            if (!value.isObject()) {
                fail(error, "model.classes entries must be objects.");
                return std::nullopt;
            }
            const auto item = value.toObject();
            RunClassSnapshot cls;
            if (!only(item, {"id", "name"}, "model class", error) ||
                !string(item, "id", cls.id, false, error) ||
                !string(item, "name", cls.name, false, error)) {
                return std::nullopt;
            }
            model.classes.push_back(cls);
        }
        data.model = model;
    } else {
        fail(error, "model must be null or an object.");
        return std::nullopt;
    }

    if (!root.value("routing").isObject()) {
        fail(error, "routing must be an object.");
        return std::nullopt;
    }
    const auto routing = root.value("routing").toObject();
    if (!only(routing, {"trigger_mode", "hit_class_id",
                        "physical_daq_output_enabled"},
              "routing", error) ||
        !parseEnum(routing.value("trigger_mode").toString(),
                   {{"class_based", TriggerMode::ClassBased},
                    {"every_droplet", TriggerMode::EveryDroplet}},
                   data.routing.triggerMode) ||
        !routing.value("physical_daq_output_enabled").isBool()) {
        fail(error, "routing contains invalid values.");
        return std::nullopt;
    }
    data.routing.physicalDaqOutputEnabled =
        routing.value("physical_daq_output_enabled").toBool();
    if (routing.value("hit_class_id").isNull()) {
        data.routing.hitClassId.reset();
    } else if (routing.value("hit_class_id").isString()) {
        data.routing.hitClassId = routing.value("hit_class_id").toString();
    } else {
        fail(error, "hit_class_id must be null or a string.");
        return std::nullopt;
    }

    if (!root.value("settings").isObject()) {
        fail(error, "settings must be an object.");
        return std::nullopt;
    }
    const auto settings = root.value("settings").toObject();
    if (!only(settings, {"camera", "detector", "crop", "daq", "timing"}, "settings", error))
        return std::nullopt;
    for (const auto key : {"camera", "detector", "crop", "daq", "timing"}) {
        if (!settings.value(key).isObject()) {
            fail(error, QString("settings.%1 must be an object.").arg(key));
            return std::nullopt;
        }
    }
    data.cameraSettings = settings.value("camera").toObject();
    data.detectorSettings = settings.value("detector").toObject();
    data.cropSettings = settings.value("crop").toObject();
    data.daqSettings = settings.value("daq").toObject();
    data.timingSettings = settings.value("timing").toObject();

    if (!root.value("hit_boundary").isObject()) {
        fail(error, "hit_boundary must be an object.");
        return std::nullopt;
    }
    const auto boundary = root.value("hit_boundary").toObject();
    qint64 imageWidth = 0;
    qint64 imageHeight = 0;
    if (!only(boundary, {"boundary_y", "hit_side", "image_width", "image_height"},
              "hit_boundary", error) ||
        !boundary.value("boundary_y").isDouble() ||
        !std::isfinite(boundary.value("boundary_y").toDouble()) ||
        !parseEnum(boundary.value("hit_side").toString(),
                   {{"positive_y", HitSide::PositiveY},
                    {"negative_y", HitSide::NegativeY}},
                   data.hitBoundary.hitSide) ||
        !boundary.value("image_width").isDouble() ||
        !boundary.value("image_height").isDouble()) {
        fail(error, "hit_boundary contains invalid values.");
        return std::nullopt;
    }
    imageWidth = boundary.value("image_width").toInteger(-1);
    imageHeight = boundary.value("image_height").toInteger(-1);
    if (imageWidth <= 0 || imageHeight <= 0 ||
        imageWidth > (std::numeric_limits<int>::max)() ||
        imageHeight > (std::numeric_limits<int>::max)()) {
        fail(error, "hit_boundary image dimensions must be positive integers.");
        return std::nullopt;
    }
    data.hitBoundary.boundaryY = boundary.value("boundary_y").toDouble();
    data.hitBoundary.imageWidth = static_cast<int>(imageWidth);
    data.hitBoundary.imageHeight = static_cast<int>(imageHeight);

    if (!root.value("processing").isObject()) {
        fail(error, "processing must be an object.");
        return std::nullopt;
    }
    const auto processing = root.value("processing").toObject();
    if (!only(processing, {"requested_fps", "achieved_fps"}, "processing", error) ||
        !finitePositive(processing.value("requested_fps"), data.requestedProcessingFps,
                        "requested_fps", error) ||
        !processing.value("achieved_fps").isDouble() ||
        !std::isfinite(processing.value("achieved_fps").toDouble()) ||
        processing.value("achieved_fps").toDouble() < 0.0) {
        if (error && error->isEmpty())
            *error = "achieved_fps must be finite and nonnegative.";
        return std::nullopt;
    }
    data.achievedProcessingFps = processing.value("achieved_fps").toDouble();

    if (!root.value("files").isObject()) {
        fail(error, "files must be an object.");
        return std::nullopt;
    }
    const auto files = root.value("files").toObject();
    if (!only(files, {"events_csv", "crops_path", "sequence_path"}, "files", error) ||
        !string(files, "events_csv", data.files.eventsCsv, false, error) ||
        !string(files, "crops_path", data.files.cropsPath, false, error)) {
        return std::nullopt;
    }
    if (files.value("sequence_path").isNull()) {
        data.files.sequencePath.reset();
    } else if (files.value("sequence_path").isString()) {
        data.files.sequencePath = files.value("sequence_path").toString();
    } else {
        fail(error, "sequence_path must be null or a string.");
        return std::nullopt;
    }

    const QString runRoot = QFileInfo(path).absolutePath();
    const bool partialSummary =
        QFileInfo(path).fileName() == QStringLiteral("run_summary.partial.json");
    const QString eventFile = partialSummary ? QStringLiteral("events.partial.csv")
                                             : data.files.eventsCsv;
    if (!loadEvents(QDir(runRoot).filePath(eventFile), data.events, error) ||
        !validateData(data, error)) {
        return std::nullopt;
    }
    for (const auto& event : data.events) {
        if (!containedExistingFile(runRoot, event.cropPath, error))
            return std::nullopt;
    }
    manifest.derived_ = derive(data);
    if (!root.value("counts").isObject() ||
        !exactObject(root.value("counts").toObject(), derivedJson(data, manifest.derived_),
                     "counts", error) ||
        !root.value("decision_vs_observed").isObject() ||
        !exactObject(root.value("decision_vs_observed").toObject(),
                     matrixJson(manifest.derived_), "decision_vs_observed", error)) {
        return std::nullopt;
    }
    return manifest;
}

bool RunManifestV2::save(const QString& path, const RunManifestData& data, QString* error) {
    if (error)
        error->clear();
    if (!validateData(data, error))
        return false;
    const QString runRoot = QFileInfo(path).absolutePath();
    const bool partialSummary =
        QFileInfo(path).fileName() == QStringLiteral("run_summary.partial.json");
    const QString eventFile = partialSummary ? QStringLiteral("events.partial.csv")
                                             : data.files.eventsCsv;
    QVector<RunEvent> persistedEvents;
    if (!loadEvents(QDir(runRoot).filePath(eventFile), persistedEvents, error))
        return false;
    RunManifestData persisted = data;
    persisted.events = persistedEvents;
    if (!validateData(persisted, error) || persisted.events.size() != data.events.size())
        return error && !error->isEmpty()
                   ? false
                   : fail(error, "events.csv does not match the Run event set.");
    for (int i = 0; i < data.events.size(); ++i) {
        const auto& a = data.events.at(i);
        const auto& b = persisted.events.at(i);
        if (a.eventId != b.eventId || a.detectionTimestamp != b.detectionTimestamp ||
            a.sourceFrameIndex != b.sourceFrameIndex ||
            a.effectiveConfigurationId != b.effectiveConfigurationId ||
            a.cropPath != b.cropPath || a.predictedClassId != b.predictedClassId ||
            a.scores != b.scores || a.decision != b.decision ||
            a.observedRoute != b.observedRoute ||
            a.daqPulseStatus != b.daqPulseStatus ||
            a.inferenceTimeMs != b.inferenceTimeMs) {
            return fail(error, "events.csv does not match the Run event set.");
        }
        if (!containedExistingFile(runRoot, data.events.at(i).cropPath, error))
            return false;
    }

    QJsonValue model = QJsonValue(QJsonValue::Null);
    if (data.model) {
        QJsonArray classes;
        for (const auto& item : data.model->classes)
            classes.push_back(QJsonObject{{"id", item.id}, {"name", item.name}});
        model = QJsonObject{{"model_id", data.model->id},
                            {"model_name", data.model->name},
                            {"model_sha256", data.model->sha256},
                            {"classes", classes}};
    }
    const auto counts = derive(data);
    const QJsonObject root{
        {"schema_version", SchemaVersion},
        {"run_id", data.runId},
        {"run_name", data.runName},
        {"operation", operationText(data.operation)},
        {"experiment_type", data.experimentType},
        {"notes", data.notes},
        {"status", statusText(data.status)},
        {"started_at", data.startedAt},
        {"ended_at", data.endedAt},
        {"requested_duration_seconds",
         data.requestedDurationSeconds ? QJsonValue(*data.requestedDurationSeconds)
                                       : QJsonValue(QJsonValue::Null)},
        {"stop_reason", data.stopReason},
        {"opendss_version", data.opendssVersion},
        {"source_sequence",
         QJsonObject{{"sequence_id", data.sourceSequence.id},
                     {"sequence_name", data.sourceSequence.name},
                     {"manifest_path", data.sourceSequence.manifestPath}}},
        {"model", model},
        {"routing",
         QJsonObject{{"trigger_mode", triggerText(data.routing.triggerMode)},
                     {"hit_class_id", data.routing.hitClassId
                                          ? QJsonValue(*data.routing.hitClassId)
                                          : QJsonValue(QJsonValue::Null)},
                     {"physical_daq_output_enabled",
                      data.routing.physicalDaqOutputEnabled}}},
        {"settings",
         QJsonObject{{"camera", data.cameraSettings},
                     {"detector", data.detectorSettings},
                     {"crop", data.cropSettings},
                     {"daq", data.daqSettings},
                     {"timing", data.timingSettings}}},
        {"hit_boundary",
         QJsonObject{{"boundary_y", data.hitBoundary.boundaryY},
                     {"hit_side", hitSideText(data.hitBoundary.hitSide)},
                     {"image_width", data.hitBoundary.imageWidth},
                     {"image_height", data.hitBoundary.imageHeight}}},
        {"processing",
         QJsonObject{{"requested_fps", data.requestedProcessingFps},
                     {"achieved_fps", data.achievedProcessingFps}}},
        {"counts", derivedJson(data, counts)},
        {"decision_vs_observed", matrixJson(counts)},
        {"files",
         QJsonObject{{"events_csv", data.files.eventsCsv},
                     {"crops_path", data.files.cropsPath},
                     {"sequence_path", data.files.sequencePath
                                           ? QJsonValue(*data.files.sequencePath)
                                           : QJsonValue(QJsonValue::Null)}}},
    };
    if (!desktop_app::writeJsonObjectAtomically(path, root, error))
        return false;
    return load(path, error).has_value();
}

bool RunManifestV2::savePartial(const QString& path, const RunManifestData& data,
                                QString* error) {
    if (QFileInfo(path).fileName() != QStringLiteral("run_summary.partial.json"))
        return fail(error, "Partial Run Summary must be named run_summary.partial.json.");
    return save(path, data, error);
}

const RunManifestData& RunManifestV2::data() const noexcept {
    return data_;
}

const RunDerivedCounts& RunManifestV2::derivedCounts() const noexcept {
    return derived_;
}

} // namespace desktop_app::v2::run
