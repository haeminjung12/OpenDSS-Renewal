#include "sequence_manifest_v2.h"

#include "../../desktop_app/json_persistence.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace {

using desktop_app::v2::sequence::SequenceFrameRange;
using desktop_app::v2::sequence::SequenceLossCategory;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

bool hasOnlyFields(const QJsonObject& object, std::initializer_list<const char*> allowed,
                   const QString& context, QString* error) {
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const bool known = std::any_of(allowed.begin(), allowed.end(), [&](const char* field) {
            return it.key() == QLatin1String(field);
        });
        if (!known)
            return fail(error, QString("Unknown field '%1' in %2.").arg(it.key(), context));
    }
    return true;
}

bool requiredString(const QJsonObject& object, const QString& key, QString& value,
                    bool allowEmpty, QString* error) {
    if (!object.value(key).isString())
        return fail(error, QString("Required field '%1' must be a string.").arg(key));
    value = object.value(key).toString();
    if (!allowEmpty && value.trimmed().isEmpty())
        return fail(error, QString("Required field '%1' must not be empty.").arg(key));
    return true;
}

bool requiredTimestamp(const QJsonObject& object, const QString& key, QString& value,
                       QString* error) {
    if (!requiredString(object, key, value, false, error))
        return false;
    if (!QDateTime::fromString(value, Qt::ISODate).isValid())
        return fail(error, QString("Field '%1' must be a valid ISO-8601 timestamp.").arg(key));
    return true;
}

bool requiredInteger(const QJsonObject& object, const QString& key, qint64& value,
                     QString* error) {
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        return fail(error, QString("Field '%1' must be an integer.").arg(key));
    }
    const qint64 integer = jsonValue.toInteger(-1);
    if (integer < 0) {
        return fail(error, QString("Field '%1' must be a nonnegative integer.").arg(key));
    }
    value = integer;
    return true;
}

QJsonObject categoryJson(const SequenceLossCategory& category) {
    QJsonArray ranges;
    for (const SequenceFrameRange& range : category.ranges)
        ranges.push_back(QJsonObject{{"first", range.first}, {"last", range.last}});
    return QJsonObject{{"count", category.count}, {"ranges", ranges}};
}

bool parseCategory(const QJsonValue& value, const QString& name,
                   SequenceLossCategory& category, QString* error) {
    if (!value.isObject())
        return fail(error, name + " must be an object.");
    const QJsonObject object = value.toObject();
    if (!hasOnlyFields(object, {"count", "ranges"}, name, error))
        return false;
    if (!requiredInteger(object, "count", category.count, error))
        return false;
    if (!object.value("ranges").isArray())
        return fail(error, name + ".ranges must be an array.");

    qint64 represented = 0;
    qint64 previousLast = -1;
    for (const QJsonValue& rangeValue : object.value("ranges").toArray()) {
        if (!rangeValue.isObject())
            return fail(error, name + " range must be an object.");
        const QJsonObject rangeObject = rangeValue.toObject();
        if (!hasOnlyFields(rangeObject, {"first", "last"}, name + " range", error))
            return false;
        SequenceFrameRange range;
        if (!requiredInteger(rangeObject, "first", range.first, error) ||
            !requiredInteger(rangeObject, "last", range.last, error)) {
            return false;
        }
        if (range.first > range.last)
            return fail(error, name + " range requires first <= last.");
        if (range.first <= previousLast)
            return fail(error, name + " ranges must be ordered and non-overlapping.");
        const quint64 length = static_cast<quint64>(range.last) -
                               static_cast<quint64>(range.first) + 1;
        if (length > static_cast<quint64>((std::numeric_limits<qint64>::max)()) ||
            represented > (std::numeric_limits<qint64>::max)() -
                              static_cast<qint64>(length)) {
            return fail(error, name + " represented count is too large.");
        }
        represented += static_cast<qint64>(length);
        previousLast = range.last;
        category.ranges.push_back(range);
    }
    if (represented != category.count)
        return fail(error, name + " count must equal the IDs represented by ranges.");
    return true;
}

} // namespace

namespace desktop_app::v2::sequence {

std::optional<SequenceManifestV2> SequenceManifestV2::load(const QString& path, QString* error) {
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, "Could not read sequence.json.");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, "sequence.json is not a valid JSON object.");
        return std::nullopt;
    }
    return fromJsonObject(document.object(), error);
}

bool SequenceManifestV2::save(const QString& path, const SequenceManifestData& data,
                              QString* error) {
    QJsonObject root{
        {"schema_version", SchemaVersion},
        {"sequence_id", data.sequenceId},
        {"name", data.name},
        {"experiment_type", data.experimentType},
        {"notes", data.notes},
        {"status", data.status},
        {"created_at", data.createdAt},
        {"started_at", data.startedAt},
        {"ended_at", data.endedAt},
        {"requested_duration_seconds",
         data.requestedDurationSeconds ? QJsonValue(*data.requestedDurationSeconds)
                                       : QJsonValue(QJsonValue::Null)},
        {"stop_reason", data.stopReason},
        {"opendss_version", data.opendssVersion},
        {"frame_format", "tiff"},
        {"frame_count", data.frameCount},
        {"frame_filename_pattern", "frames/frame_%08d.tif"},
        {"camera_settings", data.cameraSettings},
        {"image", QJsonObject{{"width", data.imageWidth},
                              {"height", data.imageHeight},
                              {"bit_depth", data.bitDepth}}},
        {"timing", QJsonObject{{"timestamps_file", QJsonValue(QJsonValue::Null)},
                               {"nominal_fps", data.nominalFps}}},
        {"integrity",
         QJsonObject{{"source_frame_gaps", categoryJson(data.integrity.sourceFrameGaps)},
                     {"queue_rejections", categoryJson(data.integrity.queueRejections)},
                     {"consumer_failures", categoryJson(data.integrity.consumerFailures)}}},
    };
    if (!fromJsonObject(root, error))
        return false;
    if (!desktop_app::writeJsonObjectAtomically(path, root, error))
        return false;
    return load(path, error).has_value();
}

std::optional<SequenceManifestV2>
SequenceManifestV2::fromJsonObject(const QJsonObject& root, QString* error) {
    if (error)
        error->clear();
    if (root.value("schema_version").toString() != SchemaVersion) {
        fail(error, "Unsupported Sequence schema_version.");
        return std::nullopt;
    }
    if (!hasOnlyFields(root,
                       {"schema_version", "sequence_id", "name", "experiment_type", "notes",
                        "status", "created_at", "started_at", "ended_at",
                        "requested_duration_seconds", "stop_reason", "opendss_version",
                        "frame_format", "frame_count", "frame_filename_pattern",
                        "camera_settings", "image", "timing", "integrity"},
                       "Sequence root", error)) {
        return std::nullopt;
    }

    SequenceManifestV2 manifest;
    auto& data = manifest.data_;
    if (!requiredString(root, "sequence_id", data.sequenceId, false, error) ||
        !requiredString(root, "name", data.name, false, error) ||
        !requiredString(root, "experiment_type", data.experimentType, true, error) ||
        !requiredString(root, "notes", data.notes, true, error) ||
        !requiredString(root, "status", data.status, false, error) ||
        !requiredTimestamp(root, "created_at", data.createdAt, error) ||
        !requiredTimestamp(root, "started_at", data.startedAt, error) ||
        !requiredTimestamp(root, "ended_at", data.endedAt, error) ||
        !requiredString(root, "stop_reason", data.stopReason, false, error) ||
        !requiredString(root, "opendss_version", data.opendssVersion, false, error)) {
        return std::nullopt;
    }

    const QJsonValue duration = root.value("requested_duration_seconds");
    if (duration.isNull()) {
        data.requestedDurationSeconds.reset();
    } else if (duration.isDouble() && std::isfinite(duration.toDouble()) &&
               duration.toDouble() > 0.0) {
        data.requestedDurationSeconds = duration.toDouble();
    } else {
        fail(error, "requested_duration_seconds must be null or a finite positive number.");
        return std::nullopt;
    }
    if (root.value("frame_format").toString() != "tiff") {
        fail(error, "frame_format must be 'tiff'.");
        return std::nullopt;
    }
    if (root.value("frame_filename_pattern").toString() != "frames/frame_%08d.tif") {
        fail(error, "frame_filename_pattern must be 'frames/frame_%08d.tif'.");
        return std::nullopt;
    }
    if (!requiredInteger(root, "frame_count", data.frameCount, error))
        return std::nullopt;
    if (!root.value("camera_settings").isObject()) {
        fail(error, "camera_settings must be an object.");
        return std::nullopt;
    }
    data.cameraSettings = root.value("camera_settings").toObject();

    if (!root.value("image").isObject()) {
        fail(error, "image must be an object.");
        return std::nullopt;
    }
    const QJsonObject image = root.value("image").toObject();
    if (!hasOnlyFields(image, {"width", "height", "bit_depth"}, "image", error))
        return std::nullopt;
    qint64 width = 0;
    qint64 height = 0;
    qint64 bitDepth = 0;
    if (!requiredInteger(image, "width", width, error) ||
        !requiredInteger(image, "height", height, error) ||
        !requiredInteger(image, "bit_depth", bitDepth, error) ||
        width <= 0 || height <= 0 || bitDepth <= 0 ||
        width > (std::numeric_limits<int>::max)() ||
        height > (std::numeric_limits<int>::max)() ||
        bitDepth > (std::numeric_limits<int>::max)()) {
        fail(error, "image width, height, and bit_depth must be positive integers.");
        return std::nullopt;
    }
    data.imageWidth = static_cast<int>(width);
    data.imageHeight = static_cast<int>(height);
    data.bitDepth = static_cast<int>(bitDepth);

    if (!root.value("timing").isObject()) {
        fail(error, "timing must be an object.");
        return std::nullopt;
    }
    const QJsonObject timing = root.value("timing").toObject();
    if (!hasOnlyFields(timing, {"timestamps_file", "nominal_fps"}, "timing", error))
        return std::nullopt;
    if (!timing.value("timestamps_file").isNull()) {
        fail(error, "timestamps_file must be null.");
        return std::nullopt;
    }
    if (!timing.value("nominal_fps").isDouble() ||
        !std::isfinite(timing.value("nominal_fps").toDouble()) ||
        timing.value("nominal_fps").toDouble() <= 0.0) {
        fail(error, "nominal_fps must be a finite positive number.");
        return std::nullopt;
    }
    data.nominalFps = timing.value("nominal_fps").toDouble();

    if (!root.value("integrity").isObject()) {
        fail(error, "integrity must be an object.");
        return std::nullopt;
    }
    const QJsonObject integrity = root.value("integrity").toObject();
    if (!hasOnlyFields(integrity,
                       {"source_frame_gaps", "queue_rejections", "consumer_failures"},
                       "integrity", error) ||
        !parseCategory(integrity.value("source_frame_gaps"), "source_frame_gaps",
                       data.integrity.sourceFrameGaps, error) ||
        !parseCategory(integrity.value("queue_rejections"), "queue_rejections",
                       data.integrity.queueRejections, error) ||
        !parseCategory(integrity.value("consumer_failures"), "consumer_failures",
                       data.integrity.consumerFailures, error)) {
        return std::nullopt;
    }
    return manifest;
}

const SequenceManifestData& SequenceManifestV2::data() const noexcept {
    return data_;
}

} // namespace desktop_app::v2::sequence
