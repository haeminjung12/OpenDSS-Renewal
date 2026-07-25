#include "dataset_manifest_v2.h"

#include "../../desktop_app/json_persistence.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace {
using namespace desktop_app::v2::dataset;
using desktop_app::v2::sequence::SequenceFrameRange;
using desktop_app::v2::sequence::SequenceLossCategory;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

bool only(const QJsonObject& value, std::initializer_list<const char*> fields,
          const QString& context, QString* error) {
    for (auto it = value.constBegin(); it != value.constEnd(); ++it) {
        if (std::none_of(fields.begin(), fields.end(),
                         [&](const char* field) { return it.key() == QLatin1String(field); }))
            return fail(error, "Unknown field '" + it.key() + "' in " + context + ".");
    }
    return true;
}

bool string(const QJsonObject& object, const char* key, QString& output, bool empty,
            QString* error) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString())
        return fail(error, QString("Required field '%1' must be a string.").arg(key));
    output = value.toString();
    return empty || !output.trimmed().isEmpty()
               ? true
               : fail(error, QString("Required field '%1' must not be empty.").arg(key));
}

bool timestamp(const QJsonObject& object, const char* key, QString& output, QString* error) {
    return string(object, key, output, false, error) &&
           (QDateTime::fromString(output, Qt::ISODate).isValid()
                ? true
                : fail(error, QString("Field '%1' must be an ISO-8601 timestamp.").arg(key)));
}

bool integer(const QJsonObject& object, const char* key, qint64& output, QString* error) {
    const QJsonValue value = object.value(QLatin1String(key));
    const double number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
    if (!value.isDouble() || !std::isfinite(number) || std::floor(number) != number ||
        number < 0 || number > static_cast<double>((std::numeric_limits<qint64>::max)()))
        return fail(error, QString("Field '%1' must be a nonnegative integer.").arg(key));
    output = static_cast<qint64>(number);
    return true;
}

bool positiveInt(const QJsonObject& object, const char* key, int& output, QString* error) {
    qint64 value = 0;
    if (!integer(object, key, value, error) || value == 0 ||
        value > (std::numeric_limits<int>::max)())
        return fail(error, QString("Field '%1' must be a positive integer.").arg(key));
    output = static_cast<int>(value);
    return true;
}

bool sha(const QString& value) {
    if (value.size() != 64)
        return false;
    return std::all_of(value.cbegin(), value.cend(), [](QChar ch) {
        const ushort c = ch.unicode();
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    });
}

QJsonObject categoryJson(const SequenceLossCategory& category) {
    QJsonArray ranges;
    for (const auto& range : category.ranges)
        ranges.push_back(QJsonObject{{"first", range.first}, {"last", range.last}});
    return {{"count", category.count}, {"ranges", ranges}};
}

bool category(const QJsonValue& value, const QString& name, SequenceLossCategory& output,
              QString* error) {
    if (!value.isObject())
        return fail(error, name + " must be an object.");
    const auto object = value.toObject();
    if (!only(object, {"count", "ranges"}, name, error) ||
        !integer(object, "count", output.count, error) || !object.value("ranges").isArray())
        return false;
    qint64 represented = 0;
    qint64 previous = -1;
    for (const auto& item : object.value("ranges").toArray()) {
        if (!item.isObject())
            return fail(error, name + " range must be an object.");
        const auto rangeObject = item.toObject();
        SequenceFrameRange range;
        if (!only(rangeObject, {"first", "last"}, name + " range", error) ||
            !integer(rangeObject, "first", range.first, error) ||
            !integer(rangeObject, "last", range.last, error) || range.first > range.last ||
            range.first <= previous)
            return fail(error, name + " ranges must be ordered and non-overlapping.");
        represented += range.last - range.first + 1;
        previous = range.last;
        output.ranges.push_back(range);
    }
    return represented == output.count
               ? true
               : fail(error, name + " count must equal its represented ranges.");
}

bool relativePath(const QString& root, const QString& path, QString* resolved, QString* error) {
    QString portable = path;
    portable.replace('\\', '/');
    if (QFileInfo(portable).isAbsolute() || portable.startsWith("//") ||
        portable.contains(':'))
        return fail(error, "Crop path must be relative to dataset.json.");
    const QString clean = QDir::cleanPath(portable);
    if (clean.isEmpty() || clean == "." || clean == ".." || clean.startsWith("../"))
        return fail(error, "Crop path escapes the Dataset folder.");
    *resolved = QDir(root).absoluteFilePath(clean);
    return true;
}

QString fileSha(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

DatasetCounts derive(const DatasetManifestData& data) {
    DatasetCounts counts;
    counts.total = data.records.size();
    QHash<QString, UserLabelRecord> labels;
    for (const auto& label : data.labels)
        labels.insert(label.recordId, label);
    for (const auto& datasetClass : data.classes)
        counts.byClass.insert(datasetClass.id, 0);
    for (const auto& record : data.records) {
        const auto it = labels.constFind(record.recordId);
        if (it == labels.cend()) {
            ++counts.unlabeled;
        } else if (it->excluded) {
            ++counts.removed;
        } else {
            ++counts.labeled;
            counts.byClass[it->classId] = counts.byClass.value(it->classId).toInt() + 1;
        }
    }
    return counts;
}

QJsonObject countsJson(const DatasetCounts& value) {
    return {{"total", value.total}, {"unlabeled", value.unlabeled},
            {"labeled", value.labeled}, {"removed", value.removed},
            {"by_class", value.byClass}};
}
} // namespace

namespace desktop_app::v2::dataset {

std::optional<DatasetManifestV2> DatasetManifestV2::load(const QString& path, QString* error) {
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, "Could not read dataset.json.");
        return std::nullopt;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, "dataset.json is not a valid JSON object.");
        return std::nullopt;
    }
    return fromJsonObject(document.object(), path, error);
}

std::optional<DatasetManifestV2>
DatasetManifestV2::fromJsonObject(const QJsonObject& root, const QString& path, QString* error) {
    if (root.value("schema_version").toString() != SchemaVersion) {
        fail(error, "Unsupported Dataset schema_version.");
        return std::nullopt;
    }
    if (!only(root, {"schema_version", "dataset_id", "name", "experiment_type", "notes",
                     "status", "created_at", "updated_at", "opendss_version", "capture",
                     "counts", "classes", "records", "labels"}, "Dataset root", error))
        return std::nullopt;

    DatasetManifestV2 manifest;
    manifest.datasetRoot_ = QFileInfo(path).absolutePath();
    auto& data = manifest.data_;
    auto& provenance = data.provenance;
    if (!string(root, "dataset_id", data.datasetId, false, error) ||
        !string(root, "name", provenance.name, false, error) ||
        !string(root, "experiment_type", provenance.experimentType, true, error) ||
        !string(root, "notes", provenance.notes, true, error) ||
        !string(root, "status", provenance.status, false, error) ||
        !timestamp(root, "created_at", provenance.createdAt, error) ||
        !timestamp(root, "updated_at", provenance.updatedAt, error) ||
        !string(root, "opendss_version", provenance.opendssVersion, false, error))
        return std::nullopt;
    if (provenance.status != "completed" && provenance.status != "interrupted") {
        fail(error, "Dataset status must be completed or interrupted.");
        return std::nullopt;
    }

    if (!root.value("capture").isObject())
        return fail(error, "capture must be an object."), std::nullopt;
    const auto capture = root.value("capture").toObject();
    if (!only(capture, {"started_at", "ended_at", "requested_duration_seconds",
                        "stop_reason", "sequence", "crop_settings", "camera_settings",
                        "detection_settings", "program_settings"}, "capture", error) ||
        !timestamp(capture, "started_at", provenance.captureStartedAt, error) ||
        !timestamp(capture, "ended_at", provenance.captureEndedAt, error) ||
        !string(capture, "stop_reason", provenance.stopReason, false, error))
        return std::nullopt;
    const auto duration = capture.value("requested_duration_seconds");
    if (duration.isNull()) {
        provenance.requestedDurationSeconds.reset();
    } else if (duration.isDouble() && std::isfinite(duration.toDouble()) &&
               duration.toDouble() > 0) {
        provenance.requestedDurationSeconds = duration.toDouble();
    } else {
        fail(error, "requested_duration_seconds must be null or finite and positive.");
        return std::nullopt;
    }
    if (!capture.value("camera_settings").isObject() ||
        !capture.value("detection_settings").isObject() ||
        !capture.value("program_settings").isObject())
        return fail(error, "Capture settings must be objects."), std::nullopt;
    provenance.cameraSettings = capture.value("camera_settings").toObject();
    provenance.detectionSettings = capture.value("detection_settings").toObject();
    provenance.programSettings = capture.value("program_settings").toObject();

    if (!capture.value("sequence").isObject())
        return fail(error, "capture.sequence must be an object."), std::nullopt;
    const auto sequence = capture.value("sequence").toObject();
    auto& sequenceData = provenance.sequence;
    if (!only(sequence, {"folder", "frame_filename_pattern", "frame_count", "image",
                         "nominal_fps", "integrity"}, "capture.sequence", error) ||
        !string(sequence, "folder", sequenceData.folder, false, error) ||
        !string(sequence, "frame_filename_pattern", sequenceData.frameFilenamePattern,
                false, error) ||
        sequenceData.folder != "sequence" ||
        sequenceData.frameFilenamePattern != "sequence/frame_%08d.tif" ||
        !integer(sequence, "frame_count", sequenceData.frameCount, error))
        return fail(error, "Dataset sequence folder or frame pattern is not canonical."),
               std::nullopt;
    if (!sequence.value("image").isObject() || !sequence.value("integrity").isObject())
        return fail(error, "Dataset sequence image and integrity must be objects."),
               std::nullopt;
    const auto image = sequence.value("image").toObject();
    if (!only(image, {"width", "height", "bit_depth"}, "capture.sequence.image", error))
        return std::nullopt;
    qint64 width = 0, height = 0, bitDepth = 0;
    if (!integer(image, "width", width, error) || !integer(image, "height", height, error) ||
        !integer(image, "bit_depth", bitDepth, error))
        return std::nullopt;
    const auto fpsValue = sequence.value("nominal_fps");
    if (sequenceData.frameCount > 0 &&
        (width <= 0 || height <= 0 || bitDepth <= 0 || !fpsValue.isDouble() ||
         !std::isfinite(fpsValue.toDouble()) || fpsValue.toDouble() <= 0))
        return fail(error, "Nonempty Dataset sequence requires positive image data and FPS."),
               std::nullopt;
    if (width > (std::numeric_limits<int>::max)() ||
        height > (std::numeric_limits<int>::max)() ||
        bitDepth > (std::numeric_limits<int>::max)())
        return fail(error, "Dataset sequence image dimensions are too large."), std::nullopt;
    sequenceData.imageWidth = static_cast<int>(width);
    sequenceData.imageHeight = static_cast<int>(height);
    sequenceData.bitDepth = static_cast<int>(bitDepth);
    sequenceData.nominalFps = fpsValue.toDouble();
    const auto integrity = sequence.value("integrity").toObject();
    if (!only(integrity, {"source_frame_gaps", "queue_rejections", "consumer_failures"},
              "capture.sequence.integrity", error) ||
        !category(integrity.value("source_frame_gaps"), "source_frame_gaps",
                  sequenceData.integrity.sourceFrameGaps, error) ||
        !category(integrity.value("queue_rejections"), "queue_rejections",
                  sequenceData.integrity.queueRejections, error) ||
        !category(integrity.value("consumer_failures"), "consumer_failures",
                  sequenceData.integrity.consumerFailures, error))
        return std::nullopt;

    if (!capture.value("crop_settings").isObject())
        return fail(error, "crop_settings must be an object."), std::nullopt;
    const auto crop = capture.value("crop_settings").toObject();
    auto& cropData = provenance.crop;
    if (!only(crop, {"width", "height", "pixel_format", "file_format", "method",
                     "interpolation"}, "crop_settings", error) ||
        !positiveInt(crop, "width", cropData.width, error) ||
        !positiveInt(crop, "height", cropData.height, error) ||
        !string(crop, "pixel_format", cropData.pixelFormat, false, error) ||
        !string(crop, "file_format", cropData.fileFormat, false, error) ||
        !string(crop, "method", cropData.method, false, error) ||
        !string(crop, "interpolation", cropData.interpolation, false, error) ||
        cropData.width != 64 || cropData.height != 64 || cropData.pixelFormat != "gray8" ||
        cropData.fileFormat != "png" || cropData.method != "centered_max_bbox_clamped" ||
        cropData.interpolation != "area")
        return fail(error, "Dataset crop settings are fixed at 64x64 gray8 PNG area."),
               std::nullopt;

    if (!root.value("classes").isArray() || !root.value("records").isArray() ||
        !root.value("labels").isArray() || !root.value("counts").isObject())
        return fail(error, "classes, records, labels, and counts have invalid types."),
               std::nullopt;
    const auto classes = root.value("classes").toArray();
    if (!classes.isEmpty() && classes.size() != 2 && classes.size() != 3)
        return fail(error, "A Dataset must have zero, two, or three classes."), std::nullopt;
    QSet<QString> classNames;
    for (qsizetype index = 0; index < classes.size(); ++index) {
        if (!classes.at(index).isObject())
            return fail(error, "Class definition must be an object."), std::nullopt;
        const auto object = classes.at(index).toObject();
        DatasetClass value;
        if (!only(object, {"id", "name"}, "class", error) ||
            !string(object, "id", value.id, false, error) ||
            !string(object, "name", value.name, false, error) ||
            value.id != QString::number(index) ||
            classNames.contains(value.name.toCaseFolded()))
            return fail(error, "Classes require stable ordered IDs and unique names."),
                   std::nullopt;
        classNames.insert(value.name.toCaseFolded());
        data.classes.push_back(value);
    }

    const QString datasetRoot = QFileInfo(path).absolutePath();
    QSet<QString> recordIds;
    for (const auto& item : root.value("records").toArray()) {
        if (!item.isObject())
            return fail(error, "Neutral record must be an object."), std::nullopt;
        const auto object = item.toObject();
        DatasetRecord value;
        if (!only(object, {"record_id", "crop_path", "crop_sha256", "source_frame_id",
                           "source_frame_index", "source_event_id", "timestamp",
                           "crop_rect"}, "neutral record", error) ||
            !string(object, "record_id", value.recordId, false, error) ||
            !string(object, "crop_path", value.cropPath, false, error) ||
            !string(object, "crop_sha256", value.cropSha256, false, error) ||
            !string(object, "source_frame_id", value.sourceFrameId, false, error) ||
            !integer(object, "source_frame_index", value.sourceFrameIndex, error) ||
            !string(object, "source_event_id", value.sourceEventId, false, error) ||
            !timestamp(object, "timestamp", value.timestamp, error) ||
            recordIds.contains(value.recordId) || !sha(value.cropSha256) ||
            value.sourceFrameIndex < 1 ||
            value.sourceFrameIndex > sequenceData.frameCount)
            return fail(error, "Neutral record identity, hash, or source frame is invalid."),
                   std::nullopt;
        QString resolved;
        if (!relativePath(datasetRoot, value.cropPath, &resolved, error))
            return std::nullopt;
        if (!object.value("crop_rect").isObject())
            return fail(error, "crop_rect must be an object."), std::nullopt;
        const auto rect = object.value("crop_rect").toObject();
        qint64 x = 0, y = 0, rectWidth = 0, rectHeight = 0;
        if (!only(rect, {"x", "y", "width", "height"}, "crop_rect", error) ||
            !integer(rect, "x", x, error) || !integer(rect, "y", y, error) ||
            !integer(rect, "width", rectWidth, error) ||
            !integer(rect, "height", rectHeight, error) || rectWidth <= 0 ||
            rectHeight <= 0 || x + rectWidth > sequenceData.imageWidth ||
            y + rectHeight > sequenceData.imageHeight)
            return fail(error, "Crop rectangle must be within source image dimensions."),
                   std::nullopt;
        value.cropRect = QRect(static_cast<int>(x), static_cast<int>(y),
                               static_cast<int>(rectWidth), static_cast<int>(rectHeight));
        value.cropSha256 = value.cropSha256.toLower();
        recordIds.insert(value.recordId);
        data.records.push_back(value);
    }

    QSet<QString> labelIds, labeledRecords;
    for (const auto& item : root.value("labels").toArray()) {
        if (!item.isObject())
            return fail(error, "User label must be an object."), std::nullopt;
        const auto object = item.toObject();
        UserLabelRecord value;
        if (!only(object, {"label_id", "record_id", "class_id", "excluded"},
                  "user label", error) ||
            !string(object, "label_id", value.labelId, false, error) ||
            !string(object, "record_id", value.recordId, false, error) ||
            labelIds.contains(value.labelId) || labeledRecords.contains(value.recordId) ||
            !recordIds.contains(value.recordId))
            return fail(error, "User label identity or record reference is invalid."),
                   std::nullopt;
        const bool classed = object.contains("class_id");
        const bool excluded = object.value("excluded").toBool(false);
        if (classed == excluded || data.classes.isEmpty())
            return fail(error, "A label must assign one class or be excluded."), std::nullopt;
        if (classed) {
            if (!string(object, "class_id", value.classId, false, error) ||
                std::none_of(data.classes.cbegin(), data.classes.cend(),
                             [&](const auto& c) { return c.id == value.classId; }))
                return fail(error, "Label references a class outside configured classes."),
                       std::nullopt;
        } else {
            value.excluded = true;
        }
        labelIds.insert(value.labelId);
        labeledRecords.insert(value.recordId);
        data.labels.push_back(value);
    }

    const DatasetCounts expected = derive(data);
    const auto supplied = root.value("counts").toObject();
    if (!only(supplied, {"total", "unlabeled", "labeled", "removed", "by_class"},
              "counts", error) ||
        !supplied.value("by_class").isObject() ||
        supplied != countsJson(expected))
        return fail(error, "Dataset counts must be derived from records and labels."),
               std::nullopt;
    return manifest;
}

bool DatasetManifestV2::save(const QString& path, const DatasetManifestData& data,
                             QString* error) {
    if (error)
        error->clear();
    const auto& p = data.provenance;
    const auto& s = p.sequence;
    QJsonArray classes, records, labels;
    for (const auto& value : data.classes)
        classes.push_back(QJsonObject{{"id", value.id}, {"name", value.name}});
    for (const auto& value : data.records) {
        records.push_back(QJsonObject{
            {"record_id", value.recordId}, {"crop_path", value.cropPath},
            {"crop_sha256", value.cropSha256}, {"source_frame_id", value.sourceFrameId},
            {"source_frame_index", value.sourceFrameIndex},
            {"source_event_id", value.sourceEventId}, {"timestamp", value.timestamp},
            {"crop_rect", QJsonObject{{"x", value.cropRect.x()}, {"y", value.cropRect.y()},
                                       {"width", value.cropRect.width()},
                                       {"height", value.cropRect.height()}}}});
    }
    for (const auto& value : data.labels) {
        QJsonObject object{{"label_id", value.labelId}, {"record_id", value.recordId}};
        if (value.excluded)
            object.insert("excluded", true);
        else
            object.insert("class_id", value.classId);
        labels.push_back(object);
    }
    const QJsonObject integrity{
        {"source_frame_gaps", categoryJson(s.integrity.sourceFrameGaps)},
        {"queue_rejections", categoryJson(s.integrity.queueRejections)},
        {"consumer_failures", categoryJson(s.integrity.consumerFailures)}};
    const QJsonObject root{
        {"schema_version", SchemaVersion}, {"dataset_id", data.datasetId},
        {"name", p.name}, {"experiment_type", p.experimentType}, {"notes", p.notes},
        {"status", p.status}, {"created_at", p.createdAt}, {"updated_at", p.updatedAt},
        {"opendss_version", p.opendssVersion},
        {"capture", QJsonObject{
             {"started_at", p.captureStartedAt}, {"ended_at", p.captureEndedAt},
             {"requested_duration_seconds",
              p.requestedDurationSeconds ? QJsonValue(*p.requestedDurationSeconds)
                                         : QJsonValue(QJsonValue::Null)},
             {"stop_reason", p.stopReason},
             {"sequence", QJsonObject{
                  {"folder", s.folder}, {"frame_filename_pattern", s.frameFilenamePattern},
                  {"frame_count", s.frameCount},
                  {"image", QJsonObject{{"width", s.imageWidth}, {"height", s.imageHeight},
                                         {"bit_depth", s.bitDepth}}},
                  {"nominal_fps", s.nominalFps}, {"integrity", integrity}}},
             {"crop_settings", QJsonObject{
                  {"width", p.crop.width}, {"height", p.crop.height},
                  {"pixel_format", p.crop.pixelFormat}, {"file_format", p.crop.fileFormat},
                  {"method", p.crop.method}, {"interpolation", p.crop.interpolation}}},
             {"camera_settings", p.cameraSettings},
             {"detection_settings", p.detectionSettings},
             {"program_settings", p.programSettings}}},
        {"counts", countsJson(derive(data))}, {"classes", classes},
        {"records", records}, {"labels", labels}};
    if (!fromJsonObject(root, path, error))
        return false;
    return desktop_app::writeJsonObjectAtomically(path, root, error) &&
           load(path, error).has_value();
}

const DatasetManifestData& DatasetManifestV2::data() const noexcept { return data_; }
const QString& DatasetManifestV2::datasetId() const noexcept { return data_.datasetId; }
const QVector<DatasetClass>& DatasetManifestV2::classes() const noexcept { return data_.classes; }
const QVector<DatasetRecord>& DatasetManifestV2::records() const noexcept { return data_.records; }
const QVector<UserLabelRecord>& DatasetManifestV2::labels() const noexcept { return data_.labels; }
DatasetCounts DatasetManifestV2::counts() const { return derive(data_); }

QVector<TrainingSample> DatasetManifestV2::trainingSamples(QString* error) const {
    if (error)
        error->clear();
    QVector<TrainingSample> result;
    for (const auto& label : data_.labels) {
        if (label.excluded)
            continue;
        const auto record = std::find_if(data_.records.cbegin(), data_.records.cend(),
                                         [&](const auto& value) {
                                             return value.recordId == label.recordId;
                                         });
        if (record == data_.records.cend())
            return fail(error, "User label record join failed."), QVector<TrainingSample>{};
        QString path;
        if (!relativePath(datasetRoot_, record->cropPath, &path, error))
            return {};
        if (!QFileInfo(path).isFile())
            return fail(error, "Training crop is missing."), QVector<TrainingSample>{};
        const QString canonicalRoot = QFileInfo(datasetRoot_).canonicalFilePath();
        const QString canonicalCrop = QFileInfo(path).canonicalFilePath();
        const QString relativeCanonical = QDir::fromNativeSeparators(
            QDir(canonicalRoot).relativeFilePath(canonicalCrop));
        if (canonicalRoot.isEmpty() || canonicalCrop.isEmpty() ||
            relativeCanonical == ".." || relativeCanonical.startsWith("../"))
            return fail(error, "Training crop resolves outside the Dataset folder."),
                   QVector<TrainingSample>{};
        if (fileSha(path).compare(record->cropSha256, Qt::CaseInsensitive) != 0)
            return fail(error, "Training crop SHA-256 does not match dataset.json."),
                   QVector<TrainingSample>{};
        result.push_back({record->recordId, label.classId, QFileInfo(path).absoluteFilePath()});
    }
    return result;
}
} // namespace desktop_app::v2::dataset
