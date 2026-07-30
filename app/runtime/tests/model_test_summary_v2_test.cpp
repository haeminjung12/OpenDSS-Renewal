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

QByteArray csv(const ModelTestSummaryData& summary,
               const QVector<ModelTestPrediction>& predictions) {
    QStringList lines;
    QStringList header{"image_path", "true_class_id", "predicted_class_id"};
    for (int index = 0; index < summary.activeModel.classes.size(); ++index)
        header.push_back(QString("score_class_%1").arg(index));
    header.push_back("correct");
    lines.push_back(header.join(','));
    for (const auto& prediction : predictions) {
        QStringList fields{prediction.imagePath, prediction.trueClassId,
                           prediction.predictedClassId};
        for (double score : prediction.scores)
            fields.push_back(QString::number(score, 'g', 17));
        fields.push_back(
            ModelTestSummaryV2::predictionCorrect(summary, prediction).value()
                ? "true"
                : "false");
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
    value.activeModel.checkpointSha256 = QString(64, 'a');
    value.activeModel.metadataSha256 = QString(64, 'b');
    for (int index = 0; index < classCount; ++index)
        value.activeModel.classes.push_back(
            {QString("model-%1").arg(index), QString("Model %1").arg(index)});
    value.dataset.id = "dataset-1";
    value.dataset.sourcePath =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(dataset).canonicalFilePath()));
    for (int index = 0; index < classCount; ++index)
        value.dataset.classes.push_back(
            {QString("dataset-%1").arg(index), QString("Dataset %1").arg(index)});
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
        {"crops/a.png", "dataset-0", "model-0", {0.8, 0.1, 0.1}},
        {"crops/b,quoted.png", "dataset-1", "model-2", {0.1, 0.2, 0.7}},
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
              csv(summary, predictions));
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
    const QJsonObject currentModel = object.value("active_model").toObject();
    require(object.value("schema").toString() ==
                    ModelTestSummaryV2::SchemaVersion &&
                currentModel.value("checkpoint_sha256").toString() ==
                    summary.activeModel.checkpointSha256 &&
                !currentModel.contains("onnx_sha256"),
            "v3 records the exact executed checkpoint");

    const QString legacyFolder =
        QDir(temporary.path()).filePath("legacy-v2-readable");
    QDir().mkpath(legacyFolder);
    QJsonObject legacy = object;
    legacy.insert("schema", ModelTestSummaryV2::LegacySchemaVersion);
    QJsonObject legacyModel = legacy.value("active_model").toObject();
    legacyModel.remove("checkpoint_sha256");
    legacyModel.insert("onnx_sha256", QString(64, 'c'));
    legacy.insert("active_model", legacyModel);
    const QString legacyJson =
        QDir(legacyFolder).filePath("model_test_summary.json");
    writeFile(legacyJson, QJsonDocument(legacy).toJson());
    writeFile(QDir(legacyFolder).filePath("predictions.csv"),
              csv(summary, predictions));
    const auto legacyLoaded = ModelTestSummaryV2::load(legacyJson, &error);
    require(legacyLoaded &&
                legacyLoaded->data().schemaVersion ==
                    ModelTestSummaryV2::LegacySchemaVersion &&
                legacyLoaded->data().activeModel.onnxSha256 ==
                    QString(64, 'c') &&
                legacyLoaded->data().activeModel.checkpointSha256.isEmpty(),
            qPrintable(error));

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
              csv(summary, predictions));
    require(ModelTestSummaryV2::load(cudaJson, &error).has_value(),
            qPrintable(error));
}

void testTwoClassArgmaxAndMismatch() {
    stage = "two-class";
    QTemporaryDir temporary;
    QString error;
    auto summary = data(temporary, 2, 1);
    ModelTestPrediction tie{"crops/a.png", "dataset-0", "model-0",
                            {0.5, 0.5}};
    require(ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            qPrintable(error));
    tie.predictedClassId = "model-1";
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "first argmax wins ties");
    tie.predictedClassId = "model-0";
    tie.scores = {0.5};
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "exact score count required");
    tie.scores = {0.5, std::numeric_limits<double>::infinity()};
    require(!ModelTestSummaryV2::validatePrediction(summary, tie, true, &error),
            "finite scores required");

    require(ModelTestSummaryV2::validateInitial(summary, &error),
            "different Dataset and model IDs/names accepted by count");
    const auto ordinalCorrect =
        ModelTestSummaryV2::predictionCorrect(summary, tie, &error);
    require(ordinalCorrect && *ordinalCorrect,
            "correctness compares ordered class position");
    tie.trueClassId = "dataset-1";
    require(ModelTestSummaryV2::predictionCorrect(summary, tie, &error) ==
                std::optional<bool>(false),
            "different ordered positions are incorrect");
    tie.trueClassId = "dataset-0";

    summary.dataset.classes.removeLast();
    require(!ModelTestSummaryV2::validateInitial(summary, &error),
            "class-count mismatch rejected");
    summary.dataset.classes.push_back({"dataset-1", "Dataset 1"});
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
    ModelTestPrediction prediction{"../outside.png", "dataset-0", "model-0",
                                   {1.0, 0.0}};
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
