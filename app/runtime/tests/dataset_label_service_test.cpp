#include "../v2/dataset/dataset_label_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

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
    p.sequence.frameCount = 1;
    p.sequence.imageWidth = 100;
    p.sequence.imageHeight = 80;
    p.sequence.bitDepth = 8;
    p.sequence.nominalFps = 900.0;
    p.sequence.integrity.queueRejections = {2, {{8, 9}}};
    p.cameraSettings = {{"camera", 1}};
    p.detectionSettings = {{"detector", 2}};
    p.programSettings = {{"program", 3}};
    data.records = {{"r1", "crops/one.png",
                     QString::fromLatin1(QCryptographicHash::hash(read(crop),
                                                                  QCryptographicHash::Sha256)
                                             .toHex()),
                     "frame-1", "event-1", "2026-07-24T10:00:01Z",
                     QRect(1, 2, 20, 20), 1}};
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
        !check(service.assignClass("r1", "1", &error), error) ||
        !check(service.exclude("r1", &error), error) ||
        !check(service.undo(&error), error))
        return 2;
    auto current = DatasetManifestV2::load(path, &error);
    if (!check(current && stable(current->data().provenance, original.provenance) &&
                   current->data().provenance.updatedAt != original.provenance.updatedAt,
               "Label mutations or undo lost immutable provenance"))
        return 3;

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
