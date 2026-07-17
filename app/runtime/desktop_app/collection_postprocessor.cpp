#include "collection_postprocessor.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace {

QString csvQuote(QString value) {
    value.replace("\"", "\"\"");
    return "\"" + value + "\"";
}

QStringList parseCsvLine(const QString& line) {
    QStringList fields;
    QString current;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (inQuotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line.at(i + 1) == '"') {
                    current.append('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.append(ch);
            }
        } else if (ch == '"') {
            inQuotes = true;
        } else if (ch == ',') {
            fields.push_back(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.push_back(current);
    return fields;
}

QString csvLine(const QStringList& fields) {
    QStringList quoted;
    quoted.reserve(fields.size());
    for (const QString& field : fields)
        quoted.push_back(csvQuote(field));
    return quoted.join(',');
}

bool detectedValue(const QString& raw) {
    const QString value = raw.trimmed().toLower();
    return value == "1" || value == "true" || value == "yes" || value == "y";
}

bool parsePositiveInt(const QStringList& row, int index, int* value) {
    if (index < 0 || index >= row.size() || !value)
        return false;
    bool ok = false;
    const int parsed = row.at(index).trimmed().toInt(&ok);
    if (!ok || parsed <= 0)
        return false;
    *value = parsed;
    return true;
}

bool parseInt(const QStringList& row, int index, int* value) {
    if (index < 0 || index >= row.size() || !value)
        return false;
    bool ok = false;
    const int parsed = row.at(index).trimmed().toInt(&ok);
    if (!ok)
        return false;
    *value = parsed;
    return true;
}

QString sanitizedCollectionName(const QString& name) {
    QString result = name.trimmed();
    result.replace('\\', '_');
    result.replace('/', '_');
    result.replace(':', '_');
    result.replace('*', '_');
    result.replace('?', '_');
    result.replace('"', '_');
    result.replace('<', '_');
    result.replace('>', '_');
    result.replace('|', '_');
    while (result.contains(".."))
        result.replace("..", ".");
    return result.trimmed();
}

bool sameFilePath(const QString& a, const QString& b) {
    return QFileInfo(a).absoluteFilePath().compare(QFileInfo(b).absoluteFilePath(), Qt::CaseInsensitive) == 0;
}

int ensureColumn(QStringList* header, const QString& name) {
    const int existing = header->indexOf(name);
    if (existing >= 0)
        return existing;
    header->push_back(name);
    return header->size() - 1;
}

void ensureRowWidth(QStringList* row, int width) {
    while (row->size() < width)
        row->push_back(QString());
}

QJsonObject defaultClassSchema() {
    QJsonArray classes;
    {
        QJsonObject cls;
        cls["id"] = "0";
        cls["index"] = 0;
        cls["display_name"] = "Non-target";
        cls["folder"] = "reviewed/class_0";
        cls["display_color"] = "#4f9cf9";
        classes.append(cls);
    }
    {
        QJsonObject cls;
        cls["id"] = "1";
        cls["index"] = 1;
        cls["display_name"] = "Target";
        cls["folder"] = "reviewed/class_1";
        cls["display_color"] = "#f59f00";
        classes.append(cls);
    }

    QJsonObject excluded;
    excluded["id"] = "exclude";
    excluded["display_name"] = "Excluded";
    excluded["folder"] = "reviewed/exclude";

    QJsonObject schema;
    schema["kind"] = "target-nontarget-binary";
    schema["mode"] = 2;
    schema["target_class_id"] = "1";
    schema["classes"] = classes;
    schema["excluded_label"] = excluded;
    return schema;
}

bool writeJsonFile(const QString& path, const QJsonObject& object, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QString("Failed to open %1 for writing").arg(path);
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = QString("Failed to commit %1").arg(path);
        return false;
    }
    return true;
}

QJsonObject loadJsonObject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

} // namespace

CollectionPostprocessResult postprocessCollectionForTraining(const CollectionPostprocessOptions& options,
                                                             CollectionPostprocessProgress progress) {
    CollectionPostprocessResult result;
    const QString requestedName = sanitizedCollectionName(options.collectionName);
    if (requestedName.isEmpty()) {
        result.errorMessage = "Collection name is empty.";
        return result;
    }

    const QFileInfo sessionInfo(options.sessionDir);
    if (!sessionInfo.isDir()) {
        result.errorMessage = QString("Collection session folder does not exist: %1").arg(options.sessionDir);
        return result;
    }

    QDir collectionsRoot(options.collectionsRoot.trimmed().isEmpty() ? sessionInfo.dir().absolutePath()
                                                                     : options.collectionsRoot.trimmed());
    if (!collectionsRoot.mkpath(".")) {
        result.errorMessage = QString("Failed to create collections root: %1").arg(collectionsRoot.absolutePath());
        return result;
    }
    const QString finalCollectionDir = collectionsRoot.filePath(requestedName);
    const bool sourceIsFinal = sameFilePath(sessionInfo.absoluteFilePath(), finalCollectionDir);
    if (QFileInfo::exists(finalCollectionDir) && !sourceIsFinal) {
        result.errorMessage = QString("Collection folder already exists: %1").arg(finalCollectionDir);
        return result;
    }

    QDir preparedRoot(options.preparedDatasetsRoot);
    const QString finalDatasetDir = preparedRoot.filePath(requestedName);
    if (options.createTrainingMetadata && QFileInfo::exists(finalDatasetDir)) {
        result.errorMessage = QString("Prepared dataset folder already exists: %1").arg(finalDatasetDir);
        return result;
    }

    if (!sourceIsFinal) {
        QDir sourceParent(sessionInfo.dir().absolutePath());
        if (!sourceParent.rename(sessionInfo.fileName(), requestedName)) {
            result.errorMessage = QString("Failed to rename collection session to: %1").arg(finalCollectionDir);
            return result;
        }
    }

    result.collectionDir = finalCollectionDir;
    const QDir collectionDir(finalCollectionDir);
    const QString csvPath = collectionDir.filePath("detections.csv");
    QFile csvFile(csvPath);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.errorMessage = QString("Failed to read detections CSV: %1").arg(csvPath);
        return result;
    }
    QTextStream csvIn(&csvFile);
    QStringList lines;
    while (!csvIn.atEnd())
        lines.push_back(csvIn.readLine());
    csvFile.close();
    if (lines.isEmpty()) {
        result.errorMessage = QString("Detections CSV is empty: %1").arg(csvPath);
        return result;
    }

    QStringList header = parseCsvLine(lines.takeFirst());
    const int imageCol = header.indexOf("image");
    const int detectedCol = header.indexOf("event_detected");
    const int xCol = header.indexOf("x");
    const int yCol = header.indexOf("y");
    const int widthCol = header.indexOf("width");
    const int heightCol = header.indexOf("height");
    const int frameCol = header.indexOf("frame_number");
    const int timestampCol = header.indexOf("timestamp_utc");
    const int cropIdCol = ensureColumn(&header, "crop_id");
    const int rawPathCol = ensureColumn(&header, "crop_raw_path");
    const int crop64PathCol = ensureColumn(&header, "crop_64_path");

    if (imageCol < 0 || detectedCol < 0 || xCol < 0 || yCol < 0 || widthCol < 0 || heightCol < 0) {
        result.errorMessage = "Detections CSV is missing required image/event/bbox columns.";
        return result;
    }

    std::vector<QStringList> rows;
    rows.reserve(lines.size());
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty())
            continue;
        QStringList row = parseCsvLine(line);
        ensureRowWidth(&row, header.size());
        if (detectedValue(row.value(detectedCol)))
            result.detectedRows++;
        rows.push_back(row);
    }

    QDir().mkpath(collectionDir.filePath("crops_raw"));
    QDir().mkpath(collectionDir.filePath("crops_64"));
    if (options.createTrainingMetadata) {
        if (!preparedRoot.mkpath(requestedName) || !QDir(finalDatasetDir).mkpath("images") ||
            !QDir(finalDatasetDir).mkpath("metadata")) {
            result.errorMessage = QString("Failed to create prepared dataset folder: %1").arg(finalDatasetDir);
            return result;
        }
        result.datasetDir = finalDatasetDir;
        result.datasetManifestPath = QDir(finalDatasetDir).filePath("metadata/dataset_manifest.json");
    }

    QJsonArray manifestItems;
    int cropIndex = 0;
    const int maximum = std::max(1, result.detectedRows);
    if (progress)
        progress(0, maximum, "Reading saved detections...");

    for (QStringList& row : rows) {
        if (!detectedValue(row.value(detectedCol)))
            continue;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        if (!parseInt(row, xCol, &x) || !parseInt(row, yCol, &y) || !parsePositiveInt(row, widthCol, &width) ||
            !parsePositiveInt(row, heightCol, &height)) {
            continue;
        }

        const QString imageRelPath = row.value(imageCol).trimmed();
        const QString imagePath = QFileInfo(imageRelPath).isAbsolute() ? imageRelPath : collectionDir.filePath(imageRelPath);
        QImage frame(imagePath);
        if (frame.isNull()) {
            result.errorMessage = QString("Failed to read collection TIFF frame: %1").arg(imagePath);
            return result;
        }
        result.framesRead++;

        QRect cropRect(x, y, width, height);
        cropRect = cropRect.intersected(frame.rect());
        if (cropRect.isEmpty())
            continue;

        ++cropIndex;
        const QString cropId = QString("crop_%1").arg(cropIndex, 6, 10, QChar('0'));
        const QString rawRelPath = QString("crops_raw/%1.tiff").arg(cropId);
        const QString crop64RelPath = QString("crops_64/%1.png").arg(cropId);
        const QString rawPath = collectionDir.filePath(rawRelPath);
        const QString crop64Path = collectionDir.filePath(crop64RelPath);

        const QImage rawCrop = frame.copy(cropRect);
        if (!rawCrop.save(rawPath, "TIFF")) {
            result.errorMessage = QString("Failed to write raw crop: %1").arg(rawPath);
            return result;
        }
        result.rawCropsWritten++;

        const QImage crop64 = rawCrop.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (!crop64.save(crop64Path, "PNG")) {
            result.errorMessage = QString("Failed to write 64x64 crop: %1").arg(crop64Path);
            return result;
        }
        result.resizedCropsWritten++;

        row[cropIdCol] = cropId;
        row[rawPathCol] = rawRelPath;
        row[crop64PathCol] = crop64RelPath;

        if (options.createTrainingMetadata) {
            const QString datasetImageRelPath = QString("images/%1.png").arg(cropId);
            const QString datasetImagePath = QDir(finalDatasetDir).filePath(datasetImageRelPath);
            if (!QFile::copy(crop64Path, datasetImagePath)) {
                result.errorMessage = QString("Failed to copy dataset image: %1").arg(datasetImagePath);
                return result;
            }

            QJsonObject item;
            item["image_id"] = cropId;
            item["path"] = datasetImageRelPath;
            item["crop_path"] = datasetImageRelPath;
            item["source_kind"] = "collection_crop_64";
            item["source_collection"] = requestedName;
            item["source_collection_path"] = finalCollectionDir;
            item["source_frame_id"] = row.value(frameCol);
            item["source_stream_path"] = imageRelPath;
            item["source_crop_raw_path"] = rawRelPath;
            item["source_crop_64_path"] = crop64RelPath;
            item["timestamp"] = row.value(timestampCol);
            item["collection_mode"] = "live_data_collection";
            item["auto_label"] = "unknown";
            item["auto_label_source"] = "none";
            item["review_state"] = "unreviewed";
            item["reviewed_label"] = QJsonValue::Null;
            item["exclude_reason"] = QJsonValue::Null;
            item["trainer_eligible"] = false;
            manifestItems.append(item);
        }

        if (progress)
            progress(cropIndex, maximum, QString("Extracted %1 of %2 detected crops...").arg(cropIndex).arg(maximum));
    }

    QSaveFile outCsv(csvPath);
    if (!outCsv.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.errorMessage = QString("Failed to rewrite detections CSV: %1").arg(csvPath);
        return result;
    }
    QTextStream csvOut(&outCsv);
    csvOut << header.join(',') << '\n';
    for (const QStringList& row : rows)
        csvOut << csvLine(row) << '\n';
    csvOut.flush();
    if (!outCsv.commit()) {
        result.errorMessage = QString("Failed to commit detections CSV: %1").arg(csvPath);
        return result;
    }

    QString jsonError;
    if (options.createTrainingMetadata) {
        QJsonObject manifest;
        manifest["schema_version"] = "dataset-builder-manifest-v1";
        manifest["dataset_id"] = requestedName;
        manifest["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        manifest["source"] = QJsonObject{{"type", "collection_postprocess"},
                                         {"collection", requestedName},
                                         {"collection_path", finalCollectionDir},
                                         {"crop_source", "crops_64"}};
        manifest["items"] = manifestItems;
        const QJsonObject schema = defaultClassSchema();
        manifest["class_schema"] = schema;
        manifest["classes"] = schema.value("classes").toArray();
        if (!writeJsonFile(result.datasetManifestPath, manifest, &jsonError)) {
            result.errorMessage = jsonError;
            return result;
        }
    }

    QJsonObject metadata = loadJsonObject(collectionDir.filePath("collection_metadata.json"));
    metadata["session_id"] = requestedName;
    metadata["collection_name"] = requestedName;
    metadata["crop_extraction_enabled"] = true;
    metadata["crop_source"] = "detections.csv";
    metadata["crops_raw_dir"] = "crops_raw";
    metadata["crops_64_dir"] = "crops_64";
    metadata["detected_rows"] = result.detectedRows;
    metadata["raw_crops_written"] = result.rawCropsWritten;
    metadata["resized_crops_written"] = result.resizedCropsWritten;
    metadata["training_metadata_created"] = options.createTrainingMetadata;
    if (options.createTrainingMetadata) {
        metadata["prepared_dataset_dir"] = result.datasetDir;
        metadata["prepared_dataset_manifest"] = result.datasetManifestPath;
    }
    if (!writeJsonFile(collectionDir.filePath("collection_metadata.json"), metadata, &jsonError)) {
        result.errorMessage = jsonError;
        return result;
    }

    if (progress)
        progress(maximum, maximum, "Dataset preparation complete.");
    result.ok = true;
    return result;
}
