#pragma once

#include "../run/run_manifest_v2.h"

#include <QJsonObject>
#include <QString>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

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
using HitPulseCallback = std::function<run::DaqPulseStatus(bool, QString*)>;
using DaqReadinessGate = std::function<bool(QString*)>;

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
                        ModelProvider modelProvider = {},
                        HitPulseCallback hitPulse = {},
                        DaqReadinessGate daqReadinessGate = {});

    bool run(const SequenceTestRequest& request, QString* error = nullptr);
    void requestStop() noexcept;

private:
    OperationCoordinator& operations_;
    IDropletDetector& detector_;
    ModelLoadService* modelLoader_;
    ModelProvider modelProvider_;
    HitPulseCallback hitPulse_;
    DaqReadinessGate daqReadinessGate_;
    std::mutex controlMutex_;
    std::condition_variable pulseFinished_;
    bool running_ = false;
    bool acceptingStop_ = false;
    bool stopRequested_ = false;
    bool pulseInFlight_ = false;
    std::thread::id pulseThread_;
};

} // namespace desktop_app::v2::sequence_test
