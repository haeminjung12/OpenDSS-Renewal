#include "../v2/dataset/dataset_label_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <iostream>

using namespace desktop_app::v2::dataset;

namespace {
bool check(bool value, const QString& message) {
    if (!value)
        std::cerr << message.toStdString() << '\n';
    return value;
}
QByteArray read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
bool writeObject(const QString& path, const QJsonObject& object) {
    QFile file(path);
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}
bool stable(const DatasetCaptureProvenance& a, const DatasetCaptureProvenance& b) {
    return a.name == b.name && a.experimentType == b.experimentType &&
           a.notes == b.notes && a.opendssVersion == b.opendssVersion &&
           a.createdAt == b.createdAt && a.captureStartedAt == b.captureStartedAt &&
           a.captureEndedAt == b.captureEndedAt &&
           a.requestedDurationSeconds == b.requestedDurationSeconds &&
           a.stopReason == b.stopReason && a.status == b.status &&
           a.sequence.folder == b.sequence.folder &&
           a.sequence.frameFilenamePattern == b.sequence.frameFilenamePattern &&
           a.sequence.frameCount == b.sequence.frameCount &&
           a.sequence.imageWidth == b.sequence.imageWidth &&
           a.sequence.imageHeight == b.sequence.imageHeight &&
           a.sequence.bitDepth == b.sequence.bitDepth &&
           a.sequence.nominalFps == b.sequence.nominalFps &&
           a.sequence.integrity.queueRejections.count ==
               b.sequence.integrity.queueRejections.count &&
           a.crop.method == b.crop.method && a.cameraSettings == b.cameraSettings &&
           a.detectionSettings == b.detectionSettings &&
           a.programSettings == b.programSettings;
}
bool sameClasses(const QVector<DatasetClass>& a, const QVector<DatasetClass>& b) {
    if (a.size() != b.size())
        return false;
    for (qsizetype index = 0; index < a.size(); ++index) {
        if (a.at(index).id != b.at(index).id || a.at(index).name != b.at(index).name)
            return false;
    }
    return true;
}
DatasetManifestData fixture(const QString& root) {
    QDir().mkpath(QDir(root).filePath("crops"));
    QImage image(64, 64, QImage::Format_Grayscale8);
    image.fill(77);
    const QString crop = QDir(root).filePath("crops/one.png");
    image.save(crop, "PNG");
    image.fill(88);
    const QString secondCrop = QDir(root).filePath("crops/two.png");
    image.save(secondCrop, "PNG");
    DatasetManifestData data;
    data.datasetId = "original";
    auto& p = data.provenance;
    p.name = "Named capture";
    p.experimentType = "experiment";
    p.notes = "notes";
    p.opendssVersion = "v2";
    p.createdAt = "2026-07-24T10:00:00Z";
    p.updatedAt = "2026-07-24T10:00:01Z";
    p.captureStartedAt = "2026-07-24T10:00:00Z";
    p.captureEndedAt = "2026-07-24T10:01:00Z";
    p.requestedDurationSeconds = 60.0;
    p.stopReason = "user";
    p.status = "completed";
    p.sequence.frameCount = 2;
    p.sequence.imageWidth = 100;
    p.sequence.imageHeight = 80;
    p.sequence.bitDepth = 8;
    p.sequence.nominalFps = 900.0;
    p.sequence.integrity.queueRejections = {2, {{8, 9}}};
    p.cameraSettings = {{"camera", 1}};
    p.detectionSettings = {{"detector", 2}};
    p.programSettings = {{"program", 3}};
    data.records = {
        {"r1", "crops/one.png",
         QString::fromLatin1(
             QCryptographicHash::hash(read(crop), QCryptographicHash::Sha256).toHex()),
         "frame-1", "event-1", "2026-07-24T10:00:01Z", QRect(1, 2, 20, 20), 1},
        {"r2", "crops/two.png",
         QString::fromLatin1(
             QCryptographicHash::hash(read(secondCrop), QCryptographicHash::Sha256).toHex()),
         "frame-2", "event-2", "2026-07-24T10:00:02Z", QRect(2, 3, 20, 20), 2}};
    return data;
}
DatasetManifestData legacyFixture(const QString& root) {
    DatasetManifestData data = fixture(root);
    data.records.resize(1);
    auto& provenance = data.provenance;
    provenance.provenanceMode = "legacy_crop_only";
    provenance.opendssVersion.clear();
    provenance.captureStartedAt.clear();
    provenance.captureEndedAt.clear();
    provenance.requestedDurationSeconds.reset();
    provenance.stopReason.clear();
    provenance.cameraSettings = {};
    provenance.detectionSettings = {};
    provenance.programSettings = {};
    provenance.crop.method.clear();
    provenance.crop.interpolation.clear();
    auto& included = data.records.front();
    included.sourceFrameId.clear();
    included.sourceEventId.clear();
    included.timestamp.clear();
    included.cropRect = {};
    included.sourceFrameIndex = 0;
    data.classes = {{"0", "Empty"}, {"1", "Single"}};
    data.records.push_back({"excluded", {}, {}, {}, {}, {}, {}, 0});
    data.labels = {{"included-label", "r1", "0", false},
                   {"excluded-label", "excluded", {}, true}};
    return data;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    desktop_app::v2::OperationCoordinator operations;
    const QString source = QDir(temporary.path()).filePath("source");
    QDir().mkpath(source);
    const DatasetManifestData original = fixture(source);
    const QString path = QDir(source).filePath("dataset.json");
    QString error;
    if (!check(DatasetManifestV2::save(path, original, &error), error))
        return 1;
    DatasetLabelService service(operations);
    if (!check(service.open(path, &error), error) ||
        !check(service.configureClassCount(2, &error), error) ||
        !check(service.renameClass("0", "Empty", &error), error) ||
        !check(service.assignClass({"r1", "r2"}, "1", &error), error))
        return 2;
    const auto batchAssigned = DatasetManifestV2::load(path, &error);
    const QByteArray batchAssignedBytes = read(path);
    if (!check(batchAssigned && batchAssigned->data().labels.size() == 2 &&
                   std::all_of(batchAssigned->data().labels.cbegin(),
                               batchAssigned->data().labels.cend(),
                               [](const UserLabelRecord& label) {
                                   return !label.excluded && label.classId == "1";
                               }),
               "Full selection was not assigned in one persisted mutation") ||
        !check(!service.assignClass({"r1", "missing"}, "0", &error),
               "Atomic assignment accepted an unknown record") ||
        !check(read(path) == batchAssignedBytes,
               "Failed atomic assignment changed persisted labels") ||
        !check(service.exclude({"r1", "r2"}, &error), error))
        return 2;
    const auto batchExcluded = DatasetManifestV2::load(path, &error);
    if (!check(batchExcluded && batchExcluded->data().records.size() == 2 &&
                   batchExcluded->data().labels.size() == 2 &&
                   std::all_of(batchExcluded->data().labels.cbegin(),
                               batchExcluded->data().labels.cend(),
                               [](const UserLabelRecord& label) { return label.excluded; }),
               "Exclude did not retain both Crops in the persisted Skipped state") ||
        !check(service.undo(&error), error))
        return 2;
    auto current = DatasetManifestV2::load(path, &error);
    if (!check(current && stable(current->data().provenance, original.provenance) &&
                   current->data().provenance.updatedAt != original.provenance.updatedAt,
               "Label mutations or undo lost immutable provenance"))
        return 3;

    const QString legacySource =
        QDir(temporary.path()).filePath("legacy-source");
    QDir().mkpath(legacySource);
    const DatasetManifestData legacy = legacyFixture(legacySource);
    const QString legacyPath = QDir(legacySource).filePath("dataset.json");
    if (!check(DatasetManifestV2::save(legacyPath, legacy, &error), error))
        return 14;
    DatasetLabelService legacyService(operations);
    if (!check(legacyService.open(legacyPath, &error), error))
        return 15;
    const DatasetLabelSnapshot legacySnapshot = legacyService.snapshot();
    if (!check(legacySnapshot.records.size() == 2 &&
                   legacySnapshot.counts.classCounts.value(0) == 1 &&
                   legacySnapshot.counts.excluded == 1 &&
                   legacySnapshot.records.at(1).state ==
                       DatasetLabelState::Excluded &&
                   legacySnapshot.records.at(1).cropPath.isEmpty(),
               "Excluded null-crop record did not open as excluded"))
        return 16;
    const QString retainedCropPath = legacy.records.front().cropPath;
    const QString retainedCropSha256 = legacy.records.front().cropSha256;
    if (!check(legacyService.exclude({"r1"}, &error), error))
        return 22;
    const auto excludedIncludedCrop =
        DatasetManifestV2::load(legacyPath, &error);
    if (!check(excludedIncludedCrop &&
                   excludedIncludedCrop->data().labels.front().excluded &&
                   excludedIncludedCrop->data().records.front().cropPath ==
                       retainedCropPath &&
                   excludedIncludedCrop->data().records.front().cropSha256 ==
                       retainedCropSha256,
               "Label Exclude did not retain verified legacy crop facts"))
        return 23;

    const QString nullCropSource =
        QDir(temporary.path()).filePath("null-crop-source");
    QDir().mkpath(nullCropSource);
    const DatasetManifestData nullCropData = legacyFixture(nullCropSource);
    const QString nullCropPath =
        QDir(nullCropSource).filePath("dataset.json");
    if (!DatasetManifestV2::save(nullCropPath, nullCropData, &error))
        return 24;
    QJsonObject nullCropRoot =
        QJsonDocument::fromJson(read(nullCropPath)).object();
    QJsonObject halfPresentRoot = nullCropRoot;
    QJsonArray nullCropRecords = nullCropRoot.value("records").toArray();
    QJsonObject includedNullCrop = nullCropRecords.at(0).toObject();
    includedNullCrop["crop_path"] = QJsonValue(QJsonValue::Null);
    includedNullCrop["crop_sha256"] = QJsonValue(QJsonValue::Null);
    nullCropRecords[0] = includedNullCrop;
    nullCropRoot["records"] = nullCropRecords;
    DatasetLabelService nullCropService(operations);
    if (!check(writeObject(nullCropPath, nullCropRoot),
               "Could not write included null-crop fixture") ||
        !check(!nullCropService.open(nullCropPath, &error),
               "Included null-crop record opened successfully"))
        return 17;
    QJsonArray halfPresentRecords =
        halfPresentRoot.value("records").toArray();
    QJsonObject halfPresentRecord = halfPresentRecords.at(0).toObject();
    halfPresentRecord["crop_path"] = QJsonValue(QJsonValue::Null);
    halfPresentRecords[0] = halfPresentRecord;
    halfPresentRoot["records"] = halfPresentRecords;
    DatasetLabelService halfPresentService(operations);
    if (!check(writeObject(nullCropPath, halfPresentRoot),
               "Could not write half-present crop fixture") ||
        !check(!halfPresentService.open(nullCropPath, &error),
               "Half-present crop facts opened successfully"))
        return 25;

    const QString missingSource =
        QDir(temporary.path()).filePath("missing-source");
    QDir().mkpath(missingSource);
    const DatasetManifestData missing = fixture(missingSource);
    const QString missingPath = QDir(missingSource).filePath("dataset.json");
    if (!DatasetManifestV2::save(missingPath, missing, &error) ||
        !QFile::remove(QDir(missingSource).filePath("crops/one.png")))
        return 18;
    DatasetLabelService missingService(operations);
    if (!check(!missingService.open(missingPath, &error) &&
                   error.contains("missing", Qt::CaseInsensitive),
               "Included missing crop opened successfully"))
        return 19;

    const QString nativeExcludedMissingSource =
        QDir(temporary.path()).filePath("native-excluded-missing-source");
    QDir().mkpath(nativeExcludedMissingSource);
    DatasetManifestData nativeExcludedMissing =
        fixture(nativeExcludedMissingSource);
    nativeExcludedMissing.classes = {{"0", "Empty"}, {"1", "Single"}};
    nativeExcludedMissing.labels = {
        {"native-excluded-label", "r1", {}, true}};
    const QString nativeExcludedMissingPath =
        QDir(nativeExcludedMissingSource).filePath("dataset.json");
    if (!DatasetManifestV2::save(nativeExcludedMissingPath,
                                 nativeExcludedMissing, &error) ||
        !QFile::remove(
            QDir(nativeExcludedMissingSource).filePath("crops/one.png")))
        return 26;
    DatasetLabelService nativeExcludedMissingService(operations);
    if (!check(!nativeExcludedMissingService.open(nativeExcludedMissingPath,
                                                  &error) &&
                   error.contains("missing", Qt::CaseInsensitive),
               "Native excluded missing crop opened successfully"))
        return 27;

    const QString invalidHashSource =
        QDir(temporary.path()).filePath("invalid-hash-source");
    QDir().mkpath(invalidHashSource);
    DatasetManifestData invalidHash = fixture(invalidHashSource);
    invalidHash.records.front().cropSha256 = QString(64, QLatin1Char('0'));
    const QString invalidHashPath =
        QDir(invalidHashSource).filePath("dataset.json");
    if (!DatasetManifestV2::save(invalidHashPath, invalidHash, &error))
        return 20;
    DatasetLabelService invalidHashService(operations);
    if (!check(!invalidHashService.open(invalidHashPath, &error) &&
                   error.contains("SHA-256", Qt::CaseInsensitive),
               "Included hash-invalid crop opened successfully"))
        return 21;

    const QString sourceB = QDir(temporary.path()).filePath("source-b");
    QDir().mkpath(sourceB);
    DatasetManifestData other = fixture(sourceB);
    other.datasetId = "other";
    const QString pathB = QDir(sourceB).filePath("dataset.json");
    if (!DatasetManifestV2::save(pathB, other, &error))
        return 4;
    DatasetLabelService serviceB(operations);
    if (!serviceB.open(pathB, &error) || !serviceB.configureClassCount(2, &error))
        return 5;

    auto training = operations.acquireWithDataset(
        desktop_app::v2::OperationKind::Training,
        desktop_app::v2::ResourceLock::Training | desktop_app::v2::ResourceLock::Storage,
        path, desktop_app::v2::DatasetAccess::Read);
    const QByteArray beforeDenied = read(path);
    const DatasetLabelSnapshot beforeDeniedSnapshot = service.snapshot();
    if (!check(training.acquired(), "Could not acquire Training Dataset read fixture") ||
        !check(!service.renameClass("0", "Blocked", &error),
               "Label write was allowed during Training read") ||
        !check(read(path) == beforeDenied &&
                   service.snapshot().classes.at(0).name ==
                       beforeDeniedSnapshot.classes.at(0).name,
               "Denied Label write changed memory or disk") ||
        !check(serviceB.renameClass("0", "Other", &error),
               "Different Dataset Label write was incorrectly blocked"))
        return 6;
    training.lease.release();
    if (!check(service.renameClass("0", "Allowed", &error),
               "Label write remained blocked after Training released the Dataset"))
        return 7;

    current = DatasetManifestV2::load(path, &error);
    if (!current)
        return 8;
    const DatasetManifestData beforeFailure = current->data();
    const QString backup = path + ".backup";
    if (!QFile::rename(path, backup) || !QDir().mkdir(path))
        return 9;
    if (!check(!service.renameClass("1", "Cell", &error),
               "Persistence failure was not reported") ||
        !check(QDir(path).removeRecursively() && QFile::rename(backup, path),
               "Could not restore manifest after failure"))
        return 10;
    current = DatasetManifestV2::load(path, &error);
    if (!check(current && sameClasses(current->data().classes, beforeFailure.classes) &&
                   stable(current->data().provenance, beforeFailure.provenance),
               "Failed save changed manifest data"))
        return 11;

    const QString copy = QDir(temporary.path()).filePath("copy");
    if (!check(service.saveAs(copy, &error), error))
        return 12;
    const auto copied = DatasetManifestV2::load(QDir(copy).filePath("dataset.json"), &error);
    const auto originalAfter = DatasetManifestV2::load(path, &error);
    if (!check(copied && originalAfter && copied->data().datasetId != originalAfter->data().datasetId &&
                   stable(copied->data().provenance, originalAfter->data().provenance) &&
                   copied->data().provenance.updatedAt !=
                       originalAfter->data().provenance.updatedAt,
               "Save As changed provenance beyond ID and updated_at"))
        return 13;
    return 0;
}
