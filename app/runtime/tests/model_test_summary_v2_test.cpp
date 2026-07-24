#include "../v2/model_test/model_test_summary_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace desktop_app::v2::model_test;

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

QString csvField(QString value) {
    if (value.contains('"'))
        value.replace("\"", "\"\"");
    if (value.contains(',') || value.contains('"') || value.contains('\n'))
        return '"' + value + '"';
    return value;
}

QByteArray csv(const QVector<ModelTestPrediction>& predictions, int classCount) {
    QStringList lines;
    QStringList header{"image_path", "true_class_id", "predicted_class_id"};
    for (int index = 0; index < classCount; ++index)
        header.push_back(QString("score_class_%1").arg(index));
    header.push_back("correct");
    lines.push_back(header.join(','));
    for (const auto& prediction : predictions) {
        QStringList fields{prediction.imagePath,
                           QString::number(prediction.trueClassId),
                           QString::number(prediction.predictedClassId)};
        for (double score : prediction.scores)
            fields.push_back(QString::number(score, 'g', 17));
        fields.push_back(prediction.correct() ? "true" : "false");
        for (QString& field : fields)
            field = csvField(field);
        lines.push_back(fields.join(','));
    }
    return (lines.join('\n') + '\n').toUtf8();
}

void writeFile(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open fixture file");
    require(file.write(bytes) == bytes.size(), "write fixture file");
}

ModelTestSummaryData data(QTemporaryDir& temporary, int classCount,
                          qint64 eligible) {
    const QString dataset = QDir(temporary.path()).filePath("dataset");
    QDir().mkpath(QDir(dataset).filePath("crops"));
    writeFile(QDir(dataset).filePath("crops/a.png"), "a");
    writeFile(QDir(dataset).filePath("crops/b,quoted.png"), "b");
    ModelTestSummaryData value;
    value.testId = "test-1";
    value.status = ModelTestStatus::Completed;
    value.startedAt = "2026-07-24T12:00:00Z";
    value.endedAt = "2026-07-24T12:01:00Z";
    value.stopReason = "all_eligible_images_processed";
    value.opendssVersion = "2.0";
    value.activeModel.id = "model-1";
    value.activeModel.name = "Active Model";
    value.activeModel.onnxSha256 = QString(64, 'a');
    value.activeModel.metadataSha256 = QString(64, 'b');
    for (int index = 0; index < classCount; ++index)
        value.activeModel.classes.push_back({index, QString("Class %1").arg(index)});
    value.dataset.id = "dataset-1";
    value.dataset.sourcePath =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(dataset).canonicalFilePath()));
    value.dataset.classes = value.activeModel.classes;
    value.effectiveDevice = EffectiveDevice::Cpu;
    value.fallbackWarning = QStringLiteral("CUDA unavailable; CPU used.");
    value.eligibleImages = eligible;
    return value;
}

void testThreeClassMetricsAndStrictRoundTrip() {
    stage = "three-class";
    QTemporaryDir temporary;
    QString error;
    auto summary = data(temporary, 3, 2);
    QVector<ModelTestPrediction> predictions{
        {"crops/a.png", 0, 0, {0.8, 0.1, 0.1}},
        {"crops/b,quoted.png", 1, 2, {0.1, 0.2, 0.7}},
    };
    auto derived = ModelTestSummaryV2::derive(summary, predictions, &error);
    require(derived.has_value(), qPrintable(error));
    require(derived->processedImages == 2 && derived->correctPredictions == 1 &&
                derived->overallAccuracy == 0.5,
            "overall metrics");
    require(derived->perClass.at(2).support == 0 &&
                !derived->perClass.at(2).accuracy &&
                derived->confusionMatrix.at(1).at(2) == 1,
            "zero support and true-row predicted-column confusion");
    summary.eligibleImages = 3;
    require(!ModelTestSummaryV2::derive(summary, predictions, &error),
            "completed test cannot omit an eligible image");
    summary.eligibleImages = 2;

    const QString jsonPath =
        QDir(temporary.path()).filePath("model_test_summary.json");
    require(ModelTestSummaryV2::save(jsonPath, summary, predictions, &error),
            qPrintable(error));
    writeFile(QDir(temporary.path()).filePath("predictions.csv"),
              csv(predictions, 3));
    auto loaded = ModelTestSummaryV2::load(jsonPath, &error);
    require(loaded.has_value(), qPrintable(error));
    require(loaded->predictions().at(1).imagePath == "crops/b,quoted.png" &&
                loaded->derivedResults().confusionMatrix.at(1).at(2) == 1,
            "strict round trip");
    require(!ModelTestSummaryV2::save(jsonPath, summary, predictions, &error),
            "final summary is non-replacing");

    QFile jsonFile(jsonPath);
    require(jsonFile.open(QIODevice::ReadOnly), "read summary JSON");
    const QByteArray validCpuJson = jsonFile.readAll();
    jsonFile.close();
    QJsonObject object = QJsonDocument::fromJson(validCpuJson).object();
    QJsonObject contradictoryDevice = object.value("device").toObject();
    contradictoryDevice.insert("effective", "cuda");
    object.insert("device", contradictoryDevice);
    writeFile(jsonPath, QJsonDocument(object).toJson());
    require(!ModelTestSummaryV2::load(jsonPath, &error),
            "CUDA with fallback warning rejected on load");

    object = QJsonDocument::fromJson(validCpuJson).object();
    object.insert("approval_threshold", 0.9);
    writeFile(jsonPath, QJsonDocument(object).toJson());
    require(!ModelTestSummaryV2::load(jsonPath, &error),
            "unknown approval field rejected");

    summary.testId = "test-cuda";
    summary.effectiveDevice = EffectiveDevice::Cuda;
    summary.fallbackWarning.reset();
    const QString cudaFolder = QDir(temporary.path()).filePath("cuda");
    QDir().mkpath(cudaFolder);
    const QString cudaJson =
        QDir(cudaFolder).filePath("model_test_summary.json");
    require(ModelTestSummaryV2::save(cudaJson, summary, predictions, &error),
            qPrintable(error));
    writeFile(QDir(cudaFolder).filePath("predictions.csv"),
              csv(predictions, 3));
    require(ModelTestSummaryV2::load(cudaJson, &error).has_value(),
            qPrintable(error));
}

void testTwoClassArgmaxAndMismatch() {
    stage = "two-class";
    QTemporaryDir temporary;
    QString error;
    auto summary = data(temporary, 2, 1);
    ModelTestPrediction tie{"crops/a.png", 0, 0, {0.5, 0.5}};
    require(ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            qPrintable(error));
    tie.predictedClassId = 1;
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "first argmax wins ties");
    tie.predictedClassId = 0;
    tie.scores = {0.5};
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "exact score count required");
    tie.scores = {0.5, std::numeric_limits<double>::infinity()};
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "finite scores required");

    summary.dataset.classes[1].name = "Different";
    require(!ModelTestSummaryV2::validateInitial(summary, &error),
            "Dataset class snapshot must exactly match Active Model");

    summary.dataset.classes = summary.activeModel.classes;
    summary.fallbackWarning.reset();
    require(!ModelTestSummaryV2::validateInitial(summary, &error),
            "CPU requires a factual fallback warning");
    tie.scores = {0.6, 0.4};
    require(!ModelTestSummaryV2::save(
                QDir(temporary.path()).filePath("invalid-cpu.json"),
                summary, {tie}, &error),
            "strict save rejects CPU without fallback warning");
    summary.effectiveDevice = EffectiveDevice::Cuda;
    summary.fallbackWarning = QStringLiteral("contradictory fallback");
    require(!ModelTestSummaryV2::validateInitial(summary, &error),
            "CUDA forbids a fallback warning");
    require(!ModelTestSummaryV2::save(
                QDir(temporary.path()).filePath("invalid-cuda.json"),
                summary, {tie}, &error),
            "strict save rejects CUDA with fallback warning");
}

void testPathContainmentAndLinks() {
    stage = "paths";
    QTemporaryDir temporary;
    QString error;
    auto summary = data(temporary, 2, 1);
    ModelTestPrediction prediction{"../outside.png", 0, 0, {1.0, 0.0}};
    require(!ModelTestSummaryV2::validatePrediction(summary, prediction, true, &error),
            "parent traversal rejected");
    prediction.imagePath = QDir(summary.dataset.sourcePath).filePath("crops/a.png");
    require(!ModelTestSummaryV2::validatePrediction(summary, prediction, true, &error),
            "absolute path rejected");

    const QString outside = QDir(temporary.path()).filePath("outside.png");
    writeFile(outside, "outside");
    const QString linked =
        QDir(summary.dataset.sourcePath).filePath("crops/linked.png");
    if (QFile::link(outside, linked) && QFileInfo(linked).isSymLink()) {
        prediction.imagePath = "crops/linked.png";
        require(!ModelTestSummaryV2::validatePrediction(summary, prediction, true,
                                                         &error),
                "linked image rejected");
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testThreeClassMetricsAndStrictRoundTrip();
    testTwoClassArgmaxAndMismatch();
    testPathContainmentAndLinks();
    return 0;
}
