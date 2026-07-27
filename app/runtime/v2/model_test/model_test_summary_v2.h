#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2::model_test {

class ModelTestWriter;

enum class ModelTestStatus { Completed, Stopped, Failed };
enum class EffectiveDevice { Cpu, Cuda };

struct ModelTestClassSnapshot {
    QString id;
    QString name;
};

struct ActiveModelSnapshot {
    QString id;
    QString name;
    QString onnxSha256;
    QString metadataSha256;
    QVector<ModelTestClassSnapshot> classes;
};

struct ModelTestDatasetSnapshot {
    QString id;
    QString sourcePath;
    QVector<ModelTestClassSnapshot> classes;
};

struct ModelTestPrediction {
    QString imagePath;
    QString trueClassId;
    QString predictedClassId;
    QVector<double> scores;
};

struct ModelTestSummaryData {
    QString testId;
    ModelTestStatus status = ModelTestStatus::Stopped;
    QString startedAt;
    QString endedAt;
    QString stopReason;
    QString opendssVersion;
    ActiveModelSnapshot activeModel;
    ModelTestDatasetSnapshot dataset;
    EffectiveDevice effectiveDevice = EffectiveDevice::Cpu;
    std::optional<QString> fallbackWarning;
    qint64 eligibleImages = 0;
    QString predictionsCsv = QStringLiteral("predictions.csv");
};

struct ModelTestClassMetrics {
    QString classId;
    qint64 support = 0;
    qint64 correct = 0;
    std::optional<double> accuracy;
};

struct ModelTestDerivedResults {
    qint64 processedImages = 0;
    qint64 correctPredictions = 0;
    std::optional<double> overallAccuracy;
    QVector<ModelTestClassMetrics> perClass;
    QVector<QVector<qint64>> confusionMatrix;
};

class ModelTestSummaryV2 {
  public:
    static constexpr auto SchemaVersion = "opendss.model_test.v2";
    static constexpr auto DevicePolicy = "automatic_gpu_cpu_fallback";

    static std::optional<ModelTestSummaryV2> load(const QString& path,
                                                   QString* error = nullptr);
    static bool save(const QString& path, const ModelTestSummaryData& data,
                     const QVector<ModelTestPrediction>& predictions,
                     QString* error = nullptr);
    static bool savePartial(const QString& path, const ModelTestSummaryData& data,
                            const QVector<ModelTestPrediction>& predictions,
                            QString* error = nullptr);

    static bool validateInitial(const ModelTestSummaryData& data,
                                QString* error = nullptr);
    static bool validatePrediction(const ModelTestSummaryData& data,
                                   const ModelTestPrediction& prediction,
                                   bool requireReadableSource,
                                   QString* error = nullptr);
    static std::optional<bool>
    predictionCorrect(const ModelTestSummaryData& data,
                      const ModelTestPrediction& prediction,
                      QString* error = nullptr);
    static std::optional<ModelTestDerivedResults>
    derive(const ModelTestSummaryData& data,
           const QVector<ModelTestPrediction>& predictions,
           QString* error = nullptr);

    const ModelTestSummaryData& data() const noexcept { return data_; }
    const QVector<ModelTestPrediction>& predictions() const noexcept {
        return predictions_;
    }
    const ModelTestDerivedResults& derivedResults() const noexcept {
        return derived_;
    }

  private:
    friend class ModelTestWriter;

    static bool savePartialWithValidatedSources(
        const QString& path, const ModelTestSummaryData& data,
        const QVector<ModelTestPrediction>& predictions,
        QString* error = nullptr);

    ModelTestSummaryData data_;
    QVector<ModelTestPrediction> predictions_;
    ModelTestDerivedResults derived_;
};

} // namespace desktop_app::v2::model_test
