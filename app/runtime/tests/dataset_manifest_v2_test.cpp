#include "../v2/dataset/dataset_manifest_v2.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTemporaryDir>

#include <filesystem>
#include <iostream>

namespace {

using desktop_app::v2::dataset::DatasetManifestV2;
using desktop_app::v2::dataset::DatasetClass;
using desktop_app::v2::dataset::DatasetRecord;
using desktop_app::v2::dataset::UserLabelRecord;

int fail(int code, const QString& message) {
    std::cerr << message.toStdString() << '\n';
    return code;
}

bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

QString sha256(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QJsonObject record(const QString& id, const QString& cropPath, const QString& hash) {
    return QJsonObject{
        {"record_id", id},
        {"crop_path", cropPath},
        {"crop_sha256", hash},
        {"source_frame_id", "frame-" + id},
        {"source_event_id", "event-" + id},
        {"timestamp", "2026-07-24T10:15:30Z"},
        {"crop_rect", QJsonObject{{"x", 10}, {"y", 20}, {"width", 64}, {"height", 64}}},
    };
}

QJsonArray classes(int count) {
    QJsonArray result{
        QJsonObject{{"id", "0"}, {"name", "Empty"}},
        QJsonObject{{"id", "1"}, {"name", "Single"}},
    };
    if (count == 3)
        result.push_back(QJsonObject{{"id", "2"}, {"name", "Multiple"}});
    return result;
}

QJsonObject manifest(const QJsonArray& classes, const QJsonArray& records, const QJsonArray& labels) {
    return QJsonObject{
        {"schema_version", DatasetManifestV2::SchemaVersion},
        {"dataset_id", "dataset-fixture"},
        {"classes", classes},
        {"records", records},
        {"labels", labels},
    };
}

bool writeManifest(const QString& path, const QJsonObject& object) {
    return writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

bool expectLoadFailure(const QString& path, const QJsonObject& object, const QString& expectedError) {
    if (!writeManifest(path, object))
        return false;
    QString error;
    const auto loaded = DatasetManifestV2::load(path, &error);
    return !loaded && error.contains(expectedError, Qt::CaseInsensitive);
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return fail(1, "Could not create test Dataset folder.");

    const QString cropsDir = QDir(temp.path()).filePath("crops");
    if (!QDir().mkpath(cropsDir))
        return fail(2, "Could not create crops folder.");
    const QByteArray cropBytes("reviewed crop bytes");
    const QString cropPath = QDir(cropsDir).filePath("assigned.png");
    if (!writeBytes(cropPath, cropBytes))
        return fail(3, "Could not write assigned crop.");
    const QString datasetPath = QDir(temp.path()).filePath("dataset.json");

    const QJsonArray records{
        record("assigned", "crops\\./assigned.png", sha256(cropBytes)),
        record("excluded", "crops/excluded-may-be-missing.png", QString(64, 'a')),
        record("unlabeled", "crops/unlabeled-may-be-missing.png", QString(64, 'b')),
    };
    const QJsonArray labels{
        QJsonObject{{"label_id", "label-assigned"}, {"record_id", "assigned"}, {"class_id", "1"}},
        QJsonObject{{"label_id", "label-excluded"}, {"record_id", "excluded"}, {"excluded", true}},
    };
    if (!writeManifest(datasetPath, manifest(classes(2), records, labels)))
        return fail(4, "Could not write two-class fixture.");

    QString error;
    auto twoClass = DatasetManifestV2::load(datasetPath, &error);
    if (!twoClass || twoClass->datasetId() != "dataset-fixture" || twoClass->classes().size() != 2 ||
        twoClass->classes().at(1).id != "1" || twoClass->classes().at(1).name != "Single" ||
        twoClass->records().size() != 3 || twoClass->labels().size() != 2) {
        return fail(5, "Two-class summary load failed: " + error);
    }
    const auto samples = twoClass->trainingSamples(&error);
    if (!error.isEmpty() || samples.size() != 1 || samples.front().recordId != "assigned" ||
        samples.front().classId != "1" || samples.front().cropPath != QFileInfo(cropPath).absoluteFilePath()) {
        return fail(6, "Eligible user-label join/path/hash failed: " + error);
    }

    const QJsonArray threeClassRecords{record("class-two", "crops/assigned.png", sha256(cropBytes))};
    const QJsonArray threeClassLabels{
        QJsonObject{{"label_id", "label-class-two"}, {"record_id", "class-two"}, {"class_id", "2"}},
    };
    if (!writeManifest(datasetPath,
                       manifest(classes(3), threeClassRecords, threeClassLabels))) {
        return fail(7, "Could not write three-class fixture.");
    }
    auto threeClass = DatasetManifestV2::load(datasetPath, &error);
    const auto threeClassSamples = threeClass ? threeClass->trainingSamples(&error)
                                               : QVector<desktop_app::v2::dataset::TrainingSample>{};
    if (!threeClass || threeClass->classes().size() != 3 ||
        threeClass->classes().at(2).name != "Multiple" || !error.isEmpty() || threeClassSamples.size() != 1 ||
        threeClassSamples.front().classId != "2") {
        return fail(8, "Three-class reviewed sample failed: " + error);
    }

    QJsonArray duplicateRecords = records;
    duplicateRecords.push_back(records.first());
    if (!expectLoadFailure(datasetPath, manifest(classes(2), duplicateRecords, labels),
                           "Duplicate record_id")) {
        return fail(9, "Duplicate record IDs were accepted.");
    }

    QJsonArray duplicateLabels = labels;
    duplicateLabels.push_back(
        QJsonObject{{"label_id", "label-assigned"}, {"record_id", "unlabeled"}, {"class_id", "0"}});
    if (!expectLoadFailure(datasetPath, manifest(classes(2), records, duplicateLabels),
                           "Duplicate label_id")) {
        return fail(10, "Duplicate label IDs were accepted.");
    }

    const QJsonArray badClassLabels{
        QJsonObject{{"label_id", "bad-class"}, {"record_id", "assigned"}, {"class_id", "2"}},
    };
    if (!expectLoadFailure(datasetPath, manifest(classes(2), records, badClassLabels),
                           "configured classes")) {
        return fail(11, "Out-of-schema class label was accepted.");
    }

    const QJsonArray escapeRecords{record("escape", "../dataset-fixture-sibling/outside.png", QString(64, 'a'))};
    if (!expectLoadFailure(datasetPath, manifest(classes(2), escapeRecords, {}),
                           "escapes")) {
        return fail(12, "Escaping crop path was accepted.");
    }

    QJsonObject malformed = record("bad-geometry", "crops/assigned.png", sha256(cropBytes));
    malformed["crop_rect"] = QJsonObject{{"x", 0}, {"y", 0}, {"width", 0}, {"height", 64}};
    if (!expectLoadFailure(datasetPath, manifest(classes(2), QJsonArray{malformed}, {}),
                           "positive size")) {
        return fail(13, "Malformed crop geometry was accepted.");
    }

    QJsonObject legacy = record("legacy", "crops/assigned.png", sha256(cropBytes));
    legacy["predicted_class"] = "1";
    if (!expectLoadFailure(datasetPath, manifest(classes(2), QJsonArray{legacy}, {}),
                           "Unknown field")) {
        return fail(14, "Unknown inference field was accepted in a neutral record.");
    }

    QJsonObject routing = manifest(classes(2), records, labels);
    QJsonArray routingLabels = routing.value("labels").toArray();
    QJsonObject routingLabel = routingLabels.first().toObject();
    routingLabel["decision"] = "Hit";
    routingLabels[0] = routingLabel;
    routing["labels"] = routingLabels;
    if (!expectLoadFailure(datasetPath, routing, "Unknown field"))
        return fail(15, "Unknown routing field was accepted in a user label.");

    QJsonObject rootInference = manifest(classes(2), records, labels);
    rootInference["model_id"] = "legacy-model";
    if (!expectLoadFailure(datasetPath, rootInference, "Unknown field"))
        return fail(16, "Unknown inference field was accepted at the Dataset root.");

    QJsonObject unsupported = manifest(classes(2), records, labels);
    unsupported["schema_version"] = "opendss.dataset.v3";
    if (!expectLoadFailure(datasetPath, unsupported, "Unsupported"))
        return fail(17, "Unsupported schema was accepted.");

    QJsonArray missingNameClasses = classes(2);
    QJsonObject missingName = missingNameClasses.at(1).toObject();
    missingName.remove("name");
    missingNameClasses[1] = missingName;
    if (!expectLoadFailure(datasetPath, manifest(missingNameClasses, records, labels), "name"))
        return fail(18, "Class without a name was accepted.");

    QJsonArray duplicateNameClasses = classes(2);
    QJsonObject duplicateName = duplicateNameClasses.at(1).toObject();
    duplicateName["name"] = "empty";
    duplicateNameClasses[1] = duplicateName;
    if (!expectLoadFailure(datasetPath, manifest(duplicateNameClasses, records, labels), "unique"))
        return fail(19, "Duplicate class names were accepted.");

    QJsonObject classUnknownManifest = manifest(classes(2), records, labels);
    QJsonArray classUnknownArray = classUnknownManifest.value("classes").toArray();
    QJsonObject classUnknown = classUnknownArray.first().toObject();
    classUnknown["color"] = "red";
    classUnknownArray[0] = classUnknown;
    classUnknownManifest["classes"] = classUnknownArray;
    if (!expectLoadFailure(datasetPath, classUnknownManifest, "Unknown field"))
        return fail(20, "Unknown class field was accepted.");

    QJsonObject rectUnknown = record("rect-unknown", "crops/assigned.png", sha256(cropBytes));
    QJsonObject rectUnknownObject = rectUnknown.value("crop_rect").toObject();
    rectUnknownObject["rotation"] = 0;
    rectUnknown["crop_rect"] = rectUnknownObject;
    if (!expectLoadFailure(datasetPath, manifest(classes(2), QJsonArray{rectUnknown}, {}),
                           "Unknown field")) {
        return fail(21, "Unknown crop rectangle field was accepted.");
    }

    const QStringList nonRelativePaths{
        "C:drive-relative.png",
        R"(\\server\share\crop.png)",
        R"(\\?\C:\device\crop.png)",
        "crops/assigned.png:stream",
    };
    for (const QString& invalidPath : nonRelativePaths) {
        if (!expectLoadFailure(datasetPath,
                               manifest(classes(2),
                                        QJsonArray{record("non-relative", invalidPath, QString(64, 'a'))}, {}),
                               "relative")) {
            return fail(22, "Obvious Windows non-relative crop path was accepted: " + invalidPath);
        }
    }

    const QJsonArray missingRecords{record("missing", "crops/missing.png", QString(64, 'a'))};
    const QJsonArray missingLabels{
        QJsonObject{{"label_id", "missing-label"}, {"record_id", "missing"}, {"class_id", "0"}},
    };
    if (!writeManifest(datasetPath, manifest(classes(2), missingRecords, missingLabels)))
        return fail(23, "Could not write missing-crop fixture.");
    auto missing = DatasetManifestV2::load(datasetPath, &error);
    if (!missing)
        return fail(24, "Summary load incorrectly required missing crop bytes: " + error);
    if (!missing->trainingSamples(&error).isEmpty() || !error.contains("missing", Qt::CaseInsensitive))
        return fail(25, "Training extraction did not reject a missing assigned crop.");

    const QJsonArray corruptRecords{record("corrupt", "crops/assigned.png", QString(64, '0'))};
    const QJsonArray corruptLabels{
        QJsonObject{{"label_id", "corrupt-label"}, {"record_id", "corrupt"}, {"class_id", "0"}},
    };
    if (!writeManifest(datasetPath, manifest(classes(2), corruptRecords, corruptLabels)))
        return fail(26, "Could not write corrupt-crop fixture.");
    auto corrupt = DatasetManifestV2::load(datasetPath, &error);
    if (!corrupt)
        return fail(27, "Summary load incorrectly validated assigned crop hash: " + error);
    if (!corrupt->trainingSamples(&error).isEmpty() || !error.contains("SHA-256", Qt::CaseInsensitive))
        return fail(28, "Training extraction did not reject a corrupt assigned crop.");

    QTemporaryDir outside;
    if (outside.isValid()) {
        const QString outsideCrop = QDir(outside.path()).filePath("outside.png");
        const QString linkedCrop = QDir(cropsDir).filePath("linked.png");
        if (writeBytes(outsideCrop, cropBytes)) {
            std::error_code linkError;
            std::filesystem::create_symlink(std::filesystem::path(outsideCrop.toStdString()),
                                            std::filesystem::path(linkedCrop.toStdString()), linkError);
            if (!linkError) {
                const QJsonArray linkedRecords{record("linked", "crops/linked.png", sha256(cropBytes))};
                const QJsonArray linkedLabels{
                    QJsonObject{{"label_id", "linked-label"}, {"record_id", "linked"}, {"class_id", "0"}},
                };
                if (!writeManifest(datasetPath, manifest(classes(2), linkedRecords, linkedLabels)))
                    return fail(29, "Could not write linked-crop fixture.");
                auto linked = DatasetManifestV2::load(datasetPath, &error);
                if (!linked)
                    return fail(30, "Summary load incorrectly resolved a crop symlink: " + error);
                if (!linked->trainingSamples(&error).isEmpty() ||
                    !error.contains("outside", Qt::CaseInsensitive)) {
                    return fail(31, "Training extraction accepted a crop symlink outside the Dataset.");
                }
            }
        }
    }

    const QString savedPath = QDir(temp.path()).filePath("saved-dataset.json");
    const QVector<DatasetClass> savedClasses{{"0", "Empty"}, {"1", "Single"}};
    const QVector<DatasetRecord> savedRecords{
        {"saved-record", "crops/assigned.png", sha256(cropBytes), "frame-saved",
         "event-saved", "2026-07-24T10:15:30Z", QRect(10, 20, 64, 64)},
    };
    const QVector<UserLabelRecord> savedLabels{
        {"saved-label", "saved-record", "1", false},
    };
    if (!DatasetManifestV2::save(savedPath, "saved-dataset", savedClasses, savedRecords,
                                 savedLabels, &error)) {
        return fail(32, "Dataset save failed: " + error);
    }
    auto saved = DatasetManifestV2::load(savedPath, &error);
    if (!saved || saved->datasetId() != "saved-dataset" || saved->classes().size() != 2 ||
        saved->classes().at(0).id != "0" || saved->classes().at(1).name != "Single" ||
        saved->records().size() != 1 || saved->records().front().cropRect != QRect(10, 20, 64, 64) ||
        saved->labels().size() != 1 || saved->labels().front().classId != "1") {
        return fail(33, "Dataset save/load round trip failed: " + error);
    }
    QFile savedFile(savedPath);
    if (!savedFile.open(QIODevice::ReadOnly))
        return fail(34, "Could not read saved Dataset fixture.");
    const QByteArray savedBytes = savedFile.readAll();
    savedFile.close();

    QVector<DatasetClass> invalidClasses = savedClasses;
    invalidClasses[1].id = "9";
    if (DatasetManifestV2::save(savedPath, "invalid-dataset", invalidClasses, savedRecords,
                                savedLabels, &error)) {
        return fail(35, "Invalid Dataset was saved.");
    }
    if (!savedFile.open(QIODevice::ReadOnly) || savedFile.readAll() != savedBytes)
        return fail(36, "Failed Dataset save replaced the valid manifest.");
    savedFile.close();

    const QString neutralPath = QDir(temp.path()).filePath("neutral-dataset.json");
    if (!DatasetManifestV2::save(neutralPath, "neutral-dataset", {}, savedRecords, {}, &error)) {
        return fail(37, "Zero-class neutral Dataset save failed: " + error);
    }
    auto neutral = DatasetManifestV2::load(neutralPath, &error);
    if (!neutral || !neutral->classes().isEmpty() || !neutral->labels().isEmpty() ||
        neutral->records().size() != 1) {
        return fail(38, "Zero-class neutral Dataset round trip failed: " + error);
    }
    if (!expectLoadFailure(neutralPath,
                           manifest({}, QJsonArray{record("neutral", "crops/assigned.png",
                                                          sha256(cropBytes))},
                                    QJsonArray{QJsonObject{{"label_id", "invalid"},
                                                          {"record_id", "neutral"},
                                                          {"excluded", true}}}),
                           "must not contain labels")) {
        return fail(39, "Dataset labels were accepted without configured classes.");
    }

    return 0;
}
