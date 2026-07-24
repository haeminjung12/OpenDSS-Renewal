#include "../v2/dataset/dataset_label_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

using namespace desktop_app::v2::dataset;

namespace {

bool check(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

QString hash(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

DatasetLabelRecordState record(const DatasetLabelSnapshot& snapshot, const QString& id) {
    for (const auto& value : snapshot.records) {
        if (value.recordId == id)
            return value;
    }
    return {};
}

bool sameRecords(const QVector<DatasetRecord>& first, const QVector<DatasetRecord>& second) {
    if (first.size() != second.size())
        return false;
    for (qsizetype index = 0; index < first.size(); ++index) {
        const DatasetRecord& left = first.at(index);
        const DatasetRecord& right = second.at(index);
        if (left.recordId != right.recordId || left.cropPath != right.cropPath ||
            left.cropSha256 != right.cropSha256 || left.sourceFrameId != right.sourceFrameId ||
            left.sourceEventId != right.sourceEventId || left.timestamp != right.timestamp ||
            left.cropRect != right.cropRect) {
            return false;
        }
    }
    return true;
}

bool run() {
    QTemporaryDir temporary;
    if (!check(temporary.isValid(), "temporary directory"))
        return false;

    const QString source = QDir(temporary.path()).filePath("source");
    const QString crops = QDir(source).filePath("crops");
    if (!check(QDir().mkpath(crops), "create crop directory"))
        return false;
    const QByteArray firstBytes("png-one");
    const QByteArray secondBytes("png-two");
    const QString firstPath = QDir(crops).filePath("one.png");
    const QString secondPath = QDir(crops).filePath("two.png");
    if (!check(writeFile(firstPath, firstBytes) && writeFile(secondPath, secondBytes), "write crops"))
        return false;
    if (!check(writeFile(QDir(source).filePath("source.tiff"), "source-tiff"), "write source"))
        return false;

    const QVector<DatasetRecord> records{
        {"r1", "crops/one.png", hash(firstBytes), "frame-1", "event-1",
         "2026-07-24T12:00:00Z", QRect(0, 0, 64, 64)},
        {"r2", "crops/two.png", hash(secondBytes), "frame-2", "event-2",
         "2026-07-24T12:00:01Z", QRect(1, 2, 64, 64)},
    };
    const QString manifestPath = QDir(source).filePath("dataset.json");
    QString error;
    if (!check(DatasetManifestV2::save(manifestPath, "original-id", {}, records, {}, &error),
               qPrintable(error)))
        return false;

    DatasetLabelService service;
    if (!check(service.open(manifestPath, &error), qPrintable(error)))
        return false;
    auto view = service.snapshot();
    if (!check(view.datasetId == "original-id" && view.records.size() == 2 &&
                   view.counts.unreviewed == 2 && !view.canUndo,
               "initial snapshot"))
        return false;

    if (!check(service.configureClassCount(2, &error), qPrintable(error)))
        return false;
    if (!check(service.renameClass("0", "Empty", &error), qPrintable(error)))
        return false;
    if (!check(!service.renameClass("1", " empty ", &error), "duplicate class name rejected"))
        return false;
    if (!check(service.assignClass("r1", "0", &error), qPrintable(error)))
        return false;
    if (!check(service.exclude("r2", &error), qPrintable(error)))
        return false;
    view = service.snapshot();
    if (!check(view.counts.classCounts == QVector<int>({1, 0}) &&
                   view.counts.unreviewed == 0 && view.counts.excluded == 1 &&
                   record(view, "r2").state == DatasetLabelState::Excluded && view.canUndo,
               "label counts and states"))
        return false;
    if (!check(QFileInfo(secondPath).isFile(), "exclude retains PNG"))
        return false;

    auto persisted = DatasetManifestV2::load(manifestPath, &error);
    if (!check(persisted.has_value(), qPrintable(error)))
        return false;
    const QVector<TrainingSample> samples = persisted->trainingSamples(&error);
    if (!check(error.isEmpty() && samples.size() == 1 && samples.front().recordId == "r1",
               "training samples are labeled-only"))
        return false;
    if (!check(sameRecords(persisted->records(), records),
               "record provenance and crop hashes unchanged"))
        return false;

    if (!check(service.undo(&error), qPrintable(error)))
        return false;
    view = service.snapshot();
    if (!check(record(view, "r2").state == DatasetLabelState::Unlabeled && !view.canUndo,
               "one-level undo"))
        return false;
    if (!check(!service.undo(&error), "second undo rejected"))
        return false;

    if (!check(service.configureClassCount(3, &error), qPrintable(error)) ||
        !check(service.assignClass("r2", "2", &error), qPrintable(error)))
        return false;
    if (!check(!service.configureClassCount(2, &error), "3-to-2 blocked by class 2 label"))
        return false;
    if (!check(service.exclude("r2", &error), qPrintable(error)) ||
        !check(service.configureClassCount(2, &error), qPrintable(error)))
        return false;

    const DatasetLabelSnapshot beforeFailure = service.snapshot();
    const QString backupPath = manifestPath + ".backup";
    if (!check(QFile::rename(manifestPath, backupPath) && QDir().mkdir(manifestPath),
               "prepare persistence failure"))
        return false;
    if (!check(!service.renameClass("1", "Single", &error), "persistence failure reported"))
        return false;
    view = service.snapshot();
    if (!check(view.classes.at(1).name == beforeFailure.classes.at(1).name &&
                   view.canUndo == beforeFailure.canUndo,
               "failed persistence leaves memory unchanged"))
        return false;
    if (!check(QDir(manifestPath).removeRecursively() && QFile::rename(backupPath, manifestPath),
               "restore manifest"))
        return false;

    const QString copy = QDir(temporary.path()).filePath("copy");
    if (!check(service.saveAs(copy, &error), qPrintable(error)))
        return false;
    view = service.snapshot();
    if (!check(view.manifestPath == QDir(copy).filePath("dataset.json") &&
                   view.datasetId != "original-id" && !view.canUndo,
               "Save As switches to new Dataset"))
        return false;
    if (!check(QFileInfo(QDir(copy).filePath("crops/one.png")).isFile() &&
                   QFileInfo(QDir(copy).filePath("source.tiff")).isFile(),
               "Save As copies complete folder"))
        return false;

    const auto original = DatasetManifestV2::load(manifestPath, &error);
    const auto copied = DatasetManifestV2::load(QDir(copy).filePath("dataset.json"), &error);
    if (!check(original && copied && original->datasetId() != copied->datasetId() &&
                   sameRecords(original->records(), copied->records()),
               "Save As preserves records with independent ID"))
        return false;
    if (!check(service.renameClass("1", "Single", &error), qPrintable(error)))
        return false;
    const auto originalAfter = DatasetManifestV2::load(manifestPath, &error);
    if (!check(originalAfter && originalAfter->classes().at(1).name == "Class 1",
               "subsequent mutation changes only copied Dataset"))
        return false;
    QFile originalCrop(firstPath);
    QFile copiedCrop(QDir(copy).filePath("crops/one.png"));
    if (!check(originalCrop.open(QIODevice::ReadOnly) && copiedCrop.open(QIODevice::ReadOnly) &&
                   originalCrop.readAll() == firstBytes && copiedCrop.readAll() == firstBytes,
               "PNG bytes unchanged"))
        return false;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    return run() ? 0 : 1;
}
