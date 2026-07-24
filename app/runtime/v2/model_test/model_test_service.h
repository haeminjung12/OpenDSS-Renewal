#pragma once

#include "model_test_summary_v2.h"

#include <QString>

#include <atomic>
#include <functional>
#include <optional>

#include <opencv2/core.hpp>

namespace desktop_app::v2 {
class ModelLoadService;
class OperationCoordinator;
}

namespace desktop_app::v2::model_test {

struct ModelTestInferenceResult {
    QVector<double> scores;
};

struct PreparedModelTestModel {
    ActiveModelSnapshot snapshot;
    EffectiveDevice effectiveDevice = EffectiveDevice::Cpu;
    std::optional<QString> fallbackWarning;
    std::function<std::optional<ModelTestInferenceResult>(const cv::Mat&, QString*)>
        classify;
};

using ModelTestModelProvider =
    std::function<std::optional<PreparedModelTestModel>(QString*)>;
using ModelTestProgress = std::function<void(qint64 processed, qint64 eligible)>;

struct ModelTestRequest {
    QString datasetJsonPath;
    QString outputFolder;
    QString opendssVersion;
};

class ModelTestService final {
  public:
    ModelTestService(OperationCoordinator& operations,
                     ModelLoadService* modelLoader,
                     ModelTestModelProvider modelProvider = {},
                     ModelTestProgress progress = {});

    bool run(const ModelTestRequest& request, QString* error = nullptr);
    void requestStop() noexcept;

  private:
    OperationCoordinator& operations_;
    ModelLoadService* modelLoader_;
    ModelTestModelProvider modelProvider_;
    ModelTestProgress progress_;
    std::atomic_bool running_{false};
    std::atomic_bool stopRequested_{false};
};

} // namespace desktop_app::v2::model_test
