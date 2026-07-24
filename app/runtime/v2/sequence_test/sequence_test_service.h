#pragma once

#include "../run/run_manifest_v2.h"

#include <QJsonObject>
#include <QString>

#include <atomic>
#include <functional>
#include <optional>

#include <opencv2/core.hpp>

class IDropletDetector;

namespace desktop_app::v2 {
class ModelLoadService;
class OperationCoordinator;
}

namespace desktop_app::v2::sequence_test {

struct ModelInferenceResult {
    QVector<double> scores;
};

struct PreparedModel {
    run::ModelSnapshot snapshot;
    std::function<std::optional<ModelInferenceResult>(const cv::Mat&, QString*)> classify;
};

using ModelProvider = std::function<std::optional<PreparedModel>(QString*)>;

struct SequenceTestRequest {
    QString sequenceJson;
    QString outputRoot;
    QString runName;
    QString experimentType;
    QString notes;
    run::TriggerMode triggerMode = run::TriggerMode::EveryDroplet;
    std::optional<QString> hitClassId;
    run::HitBoundarySnapshot hitBoundary;
    bool physicalDaqOutputEnabled = false;
    double requestedProcessingFps = 0.0;
    bool useActiveModel = false;
    QString opendssVersion;
    QJsonObject detectorSettings;
    QJsonObject cropSettings;
    QJsonObject timingSettings;
    QJsonObject cameraSettings;
    QJsonObject daqSettings;
};

class SequenceTestService final {
public:
    SequenceTestService(OperationCoordinator& operations,
                        IDropletDetector& detector,
                        ModelLoadService* modelLoader,
                        ModelProvider modelProvider = {});

    bool run(const SequenceTestRequest& request, QString* error = nullptr);
    void requestStop() noexcept;

private:
    OperationCoordinator& operations_;
    IDropletDetector& detector_;
    ModelLoadService* modelLoader_;
    ModelProvider modelProvider_;
    std::atomic_bool stopRequested_{false};
};

} // namespace desktop_app::v2::sequence_test
