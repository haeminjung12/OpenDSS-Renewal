#include "../v2/dataset/dataset_manifest_v2.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <iostream>

using namespace desktop_app::v2::dataset;

namespace {
bool check(bool value, const QString& message) {
    if (!value)
        std::cerr << message.toStdString() << '\n';
    return value;
}

QByteArray bytes(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

DatasetManifestData fixture(const QString& root) {
    QDir().mkpath(QDir(root).filePath("crops"));
    const QString crop = QDir(root).filePath("crops/one.png");
    QFile cropFile(crop);
    cropFile.open(QIODevice::WriteOnly);
    cropFile.write("reviewed-crop");
    cropFile.close();
    DatasetManifestData data;
    data.datasetId = "dataset-id";
    auto& p = data.provenance;
    p.name = "Capture A";
    p.experimentType = "sorting";
    p.notes = "provenance";
    p.opendssVersion = "2.0";
    p.createdAt = "2026-07-24T10:00:00Z";
    p.updatedAt = "2026-07-24T10:01:00Z";
    p.captureStartedAt = "2026-07-24T10:00:00Z";
    p.captureEndedAt = "2026-07-24T10:01:00Z";
    p.requestedDurationSeconds = 60.0;
    p.stopReason = "user";
    p.status = "completed";
    p.sequence.frameCount = 3;
    p.sequence.imageWidth = 128;
    p.sequence.imageHeight = 96;
    p.sequence.bitDepth = 8;
    p.sequence.nominalFps = 500.0;
    p.sequence.integrity.sourceFrameGaps = {1, {{2, 2}}};
    p.cameraSettings = {{"exposure_us", 10}};
    p.detectionSettings = {{"threshold", 5}};
    p.programSettings = {{"profile", "A"}};
    p.crop = {};
    data.classes = {{"0", "Empty"}, {"1", "Cell"}};
    data.records = {{"record-1", "crops/one.png",
                     QString::fromLatin1(QCryptographicHash::hash(bytes(crop),
                                                                  QCryptographicHash::Sha256)
                                             .toHex()),
                     "source-3", "event-1", "2026-07-24T10:00:01Z",
                     QRect(3, 4, 20, 20), 3}};
    data.labels = {{"label-1", "record-1", "1", false}};
    return data;
}

bool overwrite(const QString& path, const QJsonObject& object) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(QJsonDocument(object).toJson()) > 0;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc > 1) {
        for (int index = 1; index < argc; ++index) {
            QString error;
            const QString path = QString::fromLocal8Bit(argv[index]);
            const auto manifest = DatasetManifestV2::load(path, &error);
            if (!check(manifest.has_value(), path + ": " + error))
                return 20;
            const auto samples = manifest->trainingSamples(&error);
            const auto counts = manifest->counts();
            if (!check(error.isEmpty() && samples.size() == counts.labeled,
                       path + ": training sample validation failed: " + error))
                return 21;
            std::cout << path.toStdString() << ": records=" << counts.total
                      << " labeled=" << counts.labeled << " removed=" << counts.removed
                      << " samples=" << samples.size() << " by_class="
                      << QJsonDocument(counts.byClass).toJson(QJsonDocument::Compact).toStdString()
                      << '\n';
        }
        return 0;
    }
    QTemporaryDir temporary;
    const QString path = QDir(temporary.path()).filePath("dataset.json");
    QString error;
    const DatasetManifestData original = fixture(temporary.path());
    if (!check(DatasetManifestV2::save(path, original, &error), error))
        return 1;
    const auto loaded = DatasetManifestV2::load(path, &error);
    if (!check(loaded && loaded->data().datasetId == original.datasetId &&
                   loaded->data().provenance.notes == "provenance" &&
                   loaded->data().provenance.sequence.integrity.sourceFrameGaps.count == 1 &&
                   loaded->data().records.front().sourceFrameIndex == 3 &&
                   loaded->counts().labeled == 1 &&
                   loaded->counts().byClass.value("1").toInt() == 1,
               "Complete Dataset provenance/count/source-index round trip failed: " + error))
        return 2;
    const QJsonObject defaultRoot = QJsonDocument::fromJson(bytes(path)).object();
    if (!check(!defaultRoot.value("capture").toObject().contains("save_full_image_sequence"),
               "Default full-sequence Dataset emitted an unnecessary persistence flag"))
        return 37;
    if (!check(loaded->trainingSamples(&error).size() == 1 && error.isEmpty(),
               "Training join rejected valid reviewed crop: " + error))
        return 3;

    DatasetManifestData legacy = original;
    auto& legacyProvenance = legacy.provenance;
    legacyProvenance.provenanceMode = "legacy_crop_only";
    legacyProvenance.updatedAt.clear();
    legacyProvenance.opendssVersion.clear();
    legacyProvenance.captureStartedAt.clear();
    legacyProvenance.captureEndedAt.clear();
    legacyProvenance.requestedDurationSeconds.reset();
    legacyProvenance.stopReason.clear();
    legacyProvenance.cameraSettings = {};
    legacyProvenance.detectionSettings = {};
    legacyProvenance.programSettings = {};
    legacyProvenance.crop.method.clear();
    legacyProvenance.crop.interpolation.clear();
    auto& legacyRecord = legacy.records.front();
    legacyRecord.sourceFrameId.clear();
    legacyRecord.sourceEventId.clear();
    legacyRecord.timestamp.clear();
    legacyRecord.cropRect = {};
    legacyRecord.sourceFrameIndex = 0;
    legacy.records.push_back({"excluded-record", {}, {}, {}, {}, {}, {}, 0});
    legacy.labels.push_back({"excluded-label", "excluded-record", {}, true});
    if (!check(DatasetManifestV2::save(path, legacy, &error), error))
        return 27;
    const auto loadedLegacy = DatasetManifestV2::load(path, &error);
    if (!check(loadedLegacy &&
                   loadedLegacy->data().provenance.provenanceMode ==
                       "legacy_crop_only" &&
                   loadedLegacy->data().provenance.updatedAt.isEmpty() &&
                   loadedLegacy->data().provenance.captureStartedAt.isEmpty() &&
                   loadedLegacy->data().records.front().sourceFrameId.isEmpty() &&
                   loadedLegacy->data().records.back().cropPath.isEmpty() &&
                   loadedLegacy->counts().labeled == 1 &&
                   loadedLegacy->counts().removed == 1 &&
                   loadedLegacy->trainingSamples(&error).size() == 1 &&
                   error.isEmpty(),
               "Legacy crop-only Dataset facts did not round trip: " + error))
        return 28;
    DatasetManifestData excludedWithCrop = legacy;
    excludedWithCrop.labels.front().classId.clear();
    excludedWithCrop.labels.front().excluded = true;
    if (!check(DatasetManifestV2::save(path, excludedWithCrop, &error), error))
        return 33;
    const auto loadedExcludedWithCrop = DatasetManifestV2::load(path, &error);
    if (!check(loadedExcludedWithCrop &&
                   loadedExcludedWithCrop->data().records.front().cropPath ==
                       legacy.records.front().cropPath &&
                   loadedExcludedWithCrop->data().records.front().cropSha256 ==
                       legacy.records.front().cropSha256 &&
                   loadedExcludedWithCrop->counts().removed == 2,
               "Excluded legacy record did not retain verified crop facts"))
        return 34;
    if (!DatasetManifestV2::save(path, legacy, &error))
        return 35;
    QJsonObject legacyRoot = QJsonDocument::fromJson(bytes(path)).object();
    if (!check(legacyRoot.value("provenance_mode") == "legacy_crop_only" &&
                   legacyRoot.value("updated_at").isNull() &&
                   legacyRoot.value("capture").toObject().value("sequence").isNull() &&
                   legacyRoot.value("records").toArray().last().toObject()
                       .value("crop_path").isNull(),
               "Legacy crop-only Dataset emitted invented provenance"))
        return 29;
    QJsonObject halfPresentRoot = legacyRoot;
    QJsonArray legacyRecords = legacyRoot.value("records").toArray();
    QJsonObject includedLegacyRecord = legacyRecords.first().toObject();
    includedLegacyRecord["crop_path"] = QJsonValue(QJsonValue::Null);
    includedLegacyRecord["crop_sha256"] = QJsonValue(QJsonValue::Null);
    legacyRecords[0] = includedLegacyRecord;
    legacyRoot["records"] = legacyRecords;
    if (!overwrite(path, legacyRoot) ||
        !check(!DatasetManifestV2::load(path, &error),
               "Legacy included record without crop facts was accepted"))
        return 30;
    QJsonArray halfPresentRecords =
        halfPresentRoot.value("records").toArray();
    QJsonObject halfPresentRecord = halfPresentRecords.first().toObject();
    halfPresentRecord["crop_path"] = QJsonValue(QJsonValue::Null);
    halfPresentRecords[0] = halfPresentRecord;
    halfPresentRoot["records"] = halfPresentRecords;
    if (!overwrite(path, halfPresentRoot) ||
        !check(!DatasetManifestV2::load(path, &error),
               "Legacy record with half-present crop facts was accepted"))
        return 36;

    DatasetManifestData neutral = original;
    neutral.classes.clear();
    neutral.labels.clear();
    if (!check(DatasetManifestV2::save(path, neutral, &error), error) ||
        !check(DatasetManifestV2::load(path, &error)->counts().unlabeled == 1,
               "Neutral zero-class Dataset failed"))
        return 4;

    DatasetManifestData invalid = original;
    invalid.records.front().sourceFrameIndex = 4;
    if (!check(!DatasetManifestV2::save(path, invalid, &error),
               "Out-of-range source frame was accepted"))
        return 5;
    DatasetManifestData cropOnly = original;
    cropOnly.provenance.sequence.folder.clear();
    if (!check(DatasetManifestV2::save(path, cropOnly, &error), error))
        return 38;
    const auto cropOnlyLoaded = DatasetManifestV2::load(path, &error);
    const QJsonObject cropOnlyCapture = QJsonDocument::fromJson(bytes(path)).object()
                                         .value("capture").toObject();
    if (!check(cropOnlyLoaded && !cropOnlyCapture.contains("save_full_image_sequence") &&
                   cropOnlyCapture.contains("sequence") && cropOnlyCapture.value("sequence").isNull() &&
                   cropOnlyLoaded->data().records.front().sourceFrameIndex == 3,
               "Crop-only Dataset manifest did not round trip truthfully: " + error))
        return 39;
    if (!check(DatasetManifestV2::save(path, cropOnlyLoaded->data(), &error), error) ||
        !check(QJsonDocument::fromJson(bytes(path)).object()
                    .value("capture").toObject().value("sequence").isNull(),
               "Crop-only Dataset load-save round trip did not retain null sequence metadata"))
        return 40;
    invalid = original;
    invalid.records.front().cropRect = QRect(120, 90, 20, 20);
    if (!check(!DatasetManifestV2::save(path, invalid, &error),
               "Out-of-source crop rectangle was accepted"))
        return 6;
    invalid = original;
    invalid.provenance.status = "failed";
    if (!check(!DatasetManifestV2::save(path, invalid, &error),
               "Noncanonical Dataset status was accepted"))
        return 7;

    if (!DatasetManifestV2::save(path, original, &error))
        return 8;
    QJsonObject root = QJsonDocument::fromJson(bytes(path)).object();
    QJsonObject missingDuration = root;
    QJsonObject missingDurationCapture =
        missingDuration.value("capture").toObject();
    missingDurationCapture.remove("requested_duration_seconds");
    missingDuration["capture"] = missingDurationCapture;
    if (!overwrite(path, missingDuration) ||
        !check(!DatasetManifestV2::load(path, &error),
               "Native Dataset without requested_duration_seconds was accepted"))
        return 31;
    if (!DatasetManifestV2::save(path, original, &error))
        return 32;
    root = QJsonDocument::fromJson(bytes(path)).object();
    root["model_id"] = "prohibited";
    if (!overwrite(path, root) || !check(!DatasetManifestV2::load(path, &error),
                                         "Prohibited inference field was accepted"))
        return 9;
    if (!DatasetManifestV2::save(path, original, &error))
        return 10;
    root = QJsonDocument::fromJson(bytes(path)).object();
    QJsonObject counts = root.value("counts").toObject();
    counts["labeled"] = 0;
    root["counts"] = counts;
    if (!overwrite(path, root) ||
        !check(!DatasetManifestV2::load(path, &error), "Mutable count drift was accepted"))
        return 11;
    return 0;
}
