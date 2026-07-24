#include "dataset_manifest_v2.h"

#include "../../desktop_app/json_persistence.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

namespace {

using desktop_app::v2::dataset::DatasetManifestV2;
using desktop_app::v2::dataset::DatasetRecord;
using desktop_app::v2::dataset::UserLabelRecord;

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

bool requiredString(const QJsonObject& object, const QString& key, QString& value, QString* error) {
    if (!object.value(key).isString())
        return fail(error, QString("Required field '%1' must be a string.").arg(key));
    value = object.value(key).toString().trimmed();
    if (value.isEmpty())
        return fail(error, QString("Required field '%1' must not be empty.").arg(key));
    return true;
}

bool jsonInteger(const QJsonObject& object, const QString& key, int& value, QString* error) {
    const QJsonValue jsonValue = object.value(key);
    if (!jsonValue.isDouble()) {
        return fail(error, QString("Crop rectangle field '%1' must be an integer.").arg(key));
    }
    const double number = jsonValue.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>((std::numeric_limits<int>::min)()) ||
        number > static_cast<double>((std::numeric_limits<int>::max)())) {
        return fail(error, QString("Crop rectangle field '%1' must be an integer.").arg(key));
    }
    value = static_cast<int>(number);
    return true;
}

bool validSha256(const QString& value) {
    if (value.size() != 64)
        return false;
    for (const QChar character : value) {
        const ushort code = character.unicode();
        if (!((code >= '0' && code <= '9') || (code >= 'a' && code <= 'f') ||
              (code >= 'A' && code <= 'F'))) {
            return false;
        }
    }
    return true;
}

bool resolveContainedPath(const QString& datasetRoot, const QString& relativePath,
                          QString& resolvedPath, QString* error) {
    QString portablePath = relativePath;
    portablePath.replace('\\', '/');
    if (QFileInfo(portablePath).isAbsolute() || portablePath.startsWith("//") ||
        (portablePath.size() >= 2 && portablePath.at(0).isLetter() && portablePath.at(1) == ':') ||
        portablePath.contains(':')) {
        return fail(error, "Crop path must be relative to dataset.json.");
    }

    const QString cleanRelative = QDir::cleanPath(portablePath);
    if (cleanRelative.isEmpty() || cleanRelative == "." || cleanRelative == ".." ||
        cleanRelative.startsWith("../")) {
        return fail(error, "Crop path escapes the Dataset folder.");
    }

    resolvedPath = QDir::cleanPath(QDir(datasetRoot).absoluteFilePath(cleanRelative));
    const QString relativeCheck = QDir(datasetRoot).relativeFilePath(resolvedPath);
    if (QFileInfo(relativeCheck).isAbsolute() || relativeCheck == ".." || relativeCheck.startsWith("../"))
        return fail(error, "Crop path escapes the Dataset folder.");
    return true;
}

bool canonicalPathIsContained(const QString& datasetRoot, const QString& cropPath, QString* error) {
    QString canonicalRoot = QDir::fromNativeSeparators(QFileInfo(datasetRoot).canonicalFilePath());
    QString canonicalCrop = QDir::fromNativeSeparators(QFileInfo(cropPath).canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCrop.isEmpty())
        return fail(error, "Could not resolve canonical Training crop path.");
    canonicalRoot = QDir::cleanPath(canonicalRoot);
    canonicalCrop = QDir::cleanPath(canonicalCrop);
    const QString rootPrefix = canonicalRoot.endsWith('/') ? canonicalRoot : canonicalRoot + '/';
#ifdef Q_OS_WIN
    if (!canonicalCrop.startsWith(rootPrefix, Qt::CaseInsensitive))
#else
    if (!canonicalCrop.startsWith(rootPrefix))
#endif
        return fail(error, "Training crop resolves outside the Dataset folder.");
    return true;
}

QString sha256File(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
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
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
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
    if (!hasOnlyFields(root, {"schema_version", "dataset_id", "classes", "records", "labels"},
                       "Dataset root", error)) {
        return std::nullopt;
    }

    DatasetManifestV2 manifest;
    manifest.datasetRoot_ = QFileInfo(path).absolutePath();
    if (!requiredString(root, "dataset_id", manifest.datasetId_, error))
        return std::nullopt;

    const QJsonValue classesValue = root.value("classes");
    if (!classesValue.isArray()) {
        fail(error, "Field 'classes' must be an array.");
        return std::nullopt;
    }
    const QJsonArray classes = classesValue.toArray();
    if (!classes.isEmpty() && classes.size() != 2 && classes.size() != 3) {
        fail(error, "A v2 Dataset must have no classes or configure exactly two or three classes.");
        return std::nullopt;
    }
    const QVector<QString> expectedIds = classes.size() == 2
                                            ? QVector<QString>{"0", "1"}
                                            : classes.size() == 3
                                                  ? QVector<QString>{"0", "1", "2"}
                                                  : QVector<QString>{};
    QSet<QString> classNames;
    for (qsizetype index = 0; index < classes.size(); ++index) {
        if (!classes.at(index).isObject()) {
            fail(error, "Every class definition must be an object.");
            return std::nullopt;
        }
        const QJsonObject object = classes.at(index).toObject();
        if (!hasOnlyFields(object, {"id", "name"}, "class definition", error))
            return std::nullopt;
        DatasetClass datasetClass;
        if (!requiredString(object, "id", datasetClass.id, error) ||
            !requiredString(object, "name", datasetClass.name, error)) {
            return std::nullopt;
        }
        if (datasetClass.id != expectedIds.at(index)) {
            fail(error, "Class IDs must be the ordered stable IDs 0, 1, and optional 2.");
            return std::nullopt;
        }
        const QString normalizedName = datasetClass.name.toCaseFolded();
        if (classNames.contains(normalizedName)) {
            fail(error, "Class names must be unique.");
            return std::nullopt;
        }
        classNames.insert(normalizedName);
        manifest.classes_.push_back(std::move(datasetClass));
    }

    const QJsonValue recordsValue = root.value("records");
    if (!recordsValue.isArray()) {
        fail(error, "Field 'records' must be an array.");
        return std::nullopt;
    }
    QSet<QString> recordIds;
    for (const QJsonValue& value : recordsValue.toArray()) {
        if (!value.isObject()) {
            fail(error, "Every neutral record must be an object.");
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        if (!hasOnlyFields(object,
                           {"record_id", "crop_path", "crop_sha256", "source_frame_id",
                            "source_event_id", "timestamp", "crop_rect"},
                           "neutral record", error))
            return std::nullopt;

        DatasetRecord record;
        if (!requiredString(object, "record_id", record.recordId, error) ||
            !requiredString(object, "crop_path", record.cropPath, error) ||
            !requiredString(object, "crop_sha256", record.cropSha256, error) ||
            !requiredString(object, "source_frame_id", record.sourceFrameId, error) ||
            !requiredString(object, "source_event_id", record.sourceEventId, error) ||
            !requiredString(object, "timestamp", record.timestamp, error)) {
            return std::nullopt;
        }
        if (recordIds.contains(record.recordId)) {
            fail(error, "Duplicate record_id: " + record.recordId);
            return std::nullopt;
        }
        recordIds.insert(record.recordId);
        if (!validSha256(record.cropSha256)) {
            fail(error, "crop_sha256 must contain 64 hexadecimal characters.");
            return std::nullopt;
        }
        record.cropSha256 = record.cropSha256.toLower();
        QString resolvedPath;
        if (!resolveContainedPath(manifest.datasetRoot_, record.cropPath, resolvedPath, error))
            return std::nullopt;
        if (!QDateTime::fromString(record.timestamp, Qt::ISODate).isValid()) {
            fail(error, "Record timestamp must be a valid ISO-8601 timestamp.");
            return std::nullopt;
        }

        const QJsonValue rectValue = object.value("crop_rect");
        if (!rectValue.isObject()) {
            fail(error, "Field 'crop_rect' must be an object.");
            return std::nullopt;
        }
        const QJsonObject rect = rectValue.toObject();
        if (!hasOnlyFields(rect, {"x", "y", "width", "height"}, "crop_rect", error))
            return std::nullopt;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        if (!jsonInteger(rect, "x", x, error) || !jsonInteger(rect, "y", y, error) ||
            !jsonInteger(rect, "width", width, error) || !jsonInteger(rect, "height", height, error)) {
            return std::nullopt;
        }
        if (x < 0 || y < 0 || width <= 0 || height <= 0) {
            fail(error, "Crop rectangle requires nonnegative origin and positive size.");
            return std::nullopt;
        }
        record.cropRect = QRect(x, y, width, height);
        manifest.records_.push_back(std::move(record));
    }

    const QJsonValue labelsValue = root.value("labels");
    if (!labelsValue.isArray()) {
        fail(error, "Field 'labels' must be an array.");
        return std::nullopt;
    }
    if (classes.isEmpty() && !labelsValue.toArray().isEmpty()) {
        fail(error, "A Dataset without classes must not contain labels.");
        return std::nullopt;
    }
    QSet<QString> labelIds;
    QSet<QString> labeledRecordIds;
    for (const QJsonValue& value : labelsValue.toArray()) {
        if (!value.isObject()) {
            fail(error, "Every user label record must be an object.");
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        if (!hasOnlyFields(object, {"label_id", "record_id", "class_id", "excluded"},
                           "user label", error)) {
            return std::nullopt;
        }
        UserLabelRecord label;
        if (!requiredString(object, "label_id", label.labelId, error) ||
            !requiredString(object, "record_id", label.recordId, error)) {
            return std::nullopt;
        }
        if (labelIds.contains(label.labelId)) {
            fail(error, "Duplicate label_id: " + label.labelId);
            return std::nullopt;
        }
        labelIds.insert(label.labelId);
        if (!recordIds.contains(label.recordId)) {
            fail(error, "Label references an unknown record_id: " + label.recordId);
            return std::nullopt;
        }
        if (labeledRecordIds.contains(label.recordId)) {
            fail(error, "A record may have only one current user label.");
            return std::nullopt;
        }
        labeledRecordIds.insert(label.recordId);

        const bool hasClass = object.contains("class_id");
        const bool hasExcluded = object.contains("excluded");
        if (hasClass == hasExcluded) {
            fail(error, "A user label must either assign class_id or set excluded to true.");
            return std::nullopt;
        }
        if (hasClass) {
            if (!requiredString(object, "class_id", label.classId, error))
                return std::nullopt;
            const bool classExists =
                std::any_of(manifest.classes_.cbegin(), manifest.classes_.cend(), [&](const DatasetClass& value) {
                    return value.id == label.classId;
                });
            if (!classExists) {
                fail(error, "Label references a class outside the configured classes.");
                return std::nullopt;
            }
        } else {
            if (!object.value("excluded").isBool() || !object.value("excluded").toBool()) {
                fail(error, "Excluded user labels must set excluded to true.");
                return std::nullopt;
            }
            label.excluded = true;
        }
        manifest.labels_.push_back(std::move(label));
    }

    return manifest;
}

bool DatasetManifestV2::save(const QString& path, const QString& datasetId,
                             const QVector<DatasetClass>& classes,
                             const QVector<DatasetRecord>& records,
                             const QVector<UserLabelRecord>& labels, QString* error) {
    QJsonArray classesJson;
    for (const DatasetClass& datasetClass : classes) {
        classesJson.push_back(QJsonObject{{"id", datasetClass.id}, {"name", datasetClass.name}});
    }

    QJsonArray recordsJson;
    for (const DatasetRecord& record : records) {
        recordsJson.push_back(
            QJsonObject{{"record_id", record.recordId},
                        {"crop_path", record.cropPath},
                        {"crop_sha256", record.cropSha256},
                        {"source_frame_id", record.sourceFrameId},
                        {"source_event_id", record.sourceEventId},
                        {"timestamp", record.timestamp},
                        {"crop_rect", QJsonObject{{"x", record.cropRect.x()},
                                                  {"y", record.cropRect.y()},
                                                  {"width", record.cropRect.width()},
                                                  {"height", record.cropRect.height()}}}});
    }

    QJsonArray labelsJson;
    for (const UserLabelRecord& label : labels) {
        QJsonObject object{{"label_id", label.labelId}, {"record_id", label.recordId}};
        if (label.excluded)
            object.insert("excluded", true);
        else
            object.insert("class_id", label.classId);
        labelsJson.push_back(object);
    }

    const QJsonObject root{
        {"schema_version", SchemaVersion},
        {"dataset_id", datasetId},
        {"classes", classesJson},
        {"records", recordsJson},
        {"labels", labelsJson},
    };
    if (!fromJsonObject(root, path, error))
        return false;
    if (!desktop_app::writeJsonObjectAtomically(path, root, error))
        return false;
    return load(path, error).has_value();
}

const QString& DatasetManifestV2::datasetId() const noexcept {
    return datasetId_;
}

const QVector<DatasetClass>& DatasetManifestV2::classes() const noexcept {
    return classes_;
}

const QVector<DatasetRecord>& DatasetManifestV2::records() const noexcept {
    return records_;
}

const QVector<UserLabelRecord>& DatasetManifestV2::labels() const noexcept {
    return labels_;
}

QVector<TrainingSample> DatasetManifestV2::trainingSamples(QString* error) const {
    if (error)
        error->clear();

    QVector<TrainingSample> samples;
    for (const UserLabelRecord& label : labels_) {
        if (label.excluded)
            continue;
        const auto recordIt = std::find_if(records_.cbegin(), records_.cend(), [&](const DatasetRecord& record) {
            return record.recordId == label.recordId;
        });
        if (recordIt == records_.cend()) {
            fail(error, "User label record join failed.");
            return {};
        }

        QString cropPath;
        if (!resolveContainedPath(datasetRoot_, recordIt->cropPath, cropPath, error))
            return {};
        const QFileInfo cropInfo(cropPath);
        if (!cropInfo.isFile() || !cropInfo.isReadable()) {
            fail(error, "Training crop is missing or unreadable: " + recordIt->recordId);
            return {};
        }
        if (!canonicalPathIsContained(datasetRoot_, cropPath, error))
            return {};
        const QString actualHash = sha256File(cropPath);
        if (actualHash.isEmpty() ||
            actualHash.compare(recordIt->cropSha256, Qt::CaseInsensitive) != 0) {
            fail(error, "Training crop SHA-256 mismatch: " + recordIt->recordId);
            return {};
        }
        samples.push_back(TrainingSample{recordIt->recordId, label.classId, cropInfo.absoluteFilePath()});
    }
    return samples;
}

} // namespace desktop_app::v2::dataset
