#pragma once

#include "model_test_summary_v2.h"

#include <QFile>

#include <memory>
#include <optional>

namespace desktop_app::v2::model_test {

struct ModelTestWriterTestAccess;

class ModelTestWriter {
  public:
    static std::optional<ModelTestWriter>
    start(const QString& outputFolder, ModelTestSummaryData initialData,
          QString* error = nullptr);

    ModelTestWriter(ModelTestWriter&&) noexcept;
    ModelTestWriter& operator=(ModelTestWriter&&) noexcept;
    ~ModelTestWriter();

    ModelTestWriter(const ModelTestWriter&) = delete;
    ModelTestWriter& operator=(const ModelTestWriter&) = delete;

    bool appendPrediction(const ModelTestPrediction& prediction,
                          QString* error = nullptr);
    bool flush(QString* error = nullptr);
    bool finalize(ModelTestStatus status, const QString& endedAt,
                  const QString& stopReason, QString* error = nullptr);

    const ModelTestSummaryData& data() const noexcept { return data_; }
    const QVector<ModelTestPrediction>& predictions() const noexcept {
        return predictions_;
    }

  private:
    friend struct ModelTestWriterTestAccess;

    ModelTestWriter(QString outputFolder, ModelTestSummaryData data,
                    std::unique_ptr<QFile> partialCsv);

    QString outputFolder_;
    ModelTestSummaryData data_;
    QVector<ModelTestPrediction> predictions_;
    std::unique_ptr<QFile> partialCsv_;
    bool finalized_ = false;
    bool failNextAppendForTest_ = false;
    bool failNextFinalSummaryForTest_ = false;
    bool failPartialCsvCleanupForTest_ = false;
};

} // namespace desktop_app::v2::model_test
