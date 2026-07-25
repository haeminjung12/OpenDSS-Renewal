#include "../v2/dataset/dataset_manifest_v2.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
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
    if (!check(loaded->trainingSamples(&error).size() == 1 && error.isEmpty(),
               "Training join rejected valid reviewed crop: " + error))
        return 3;

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
