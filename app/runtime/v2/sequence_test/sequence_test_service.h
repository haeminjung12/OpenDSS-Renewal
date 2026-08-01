#pragma once

#include "../run/run_manifest_v2.h"

#include <QByteArray>
#include <QImage>
#include <QJsonObject>
#include <QString>

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <opencv2/core.hpp>

class DropletFrameProcessor;

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
using DaqSettingsProvider = std::function<QJsonObject()>;

struct LoadedSequenceFrame {
    qint64 sourceFrameIndex = 0;
    QImage image;
};

struct LoadedSequence {
    QString sourceSequenceJson;
    QString sequenceId;
    QVector<LoadedSequenceFrame> frames;
};

struct SequenceTestProgress {
    qint64 processedFrames = 0;
    qint64 totalFrames = 0;
    double elapsedSeconds = 0.0;
    double achievedProcessingFps = 0.0;
};

using ProgressCallback = std::function<void(const SequenceTestProgress&)>;

struct SequenceTestRequest {
    QString sequenceJson;
    QByteArray frozenManifestBytes;
    std::shared_ptr<const LoadedSequence> loadedSequence;
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
    ProgressCallback progressCallback;
};

class SequenceTestService final {
public:
    SequenceTestService(OperationCoordinator& operations,
                        DropletFrameProcessor& processor,
                        ModelLoadService* modelLoader,
                        ModelProvider modelProvider = {},
                        HitPulseCallback hitPulse = {},
                        DaqReadinessGate daqReadinessGate = {},
                        DaqSettingsProvider daqSettingsProvider = {});

    bool run(const SequenceTestRequest& request, QString* error = nullptr,
             QString* runFolder = nullptr);
    bool updateActiveConfiguration(const run::RoutingSnapshot& routing,
                                   QString* error = nullptr);
    bool updateDecisionBoundary(const run::HitBoundarySnapshot& boundary);
    bool resetDecisionBoundary();
    void requestStop() noexcept;

private:
    OperationCoordinator& operations_;
    DropletFrameProcessor& processor_;
    ModelLoadService* modelLoader_;
    ModelProvider modelProvider_;
    HitPulseCallback hitPulse_;
    DaqReadinessGate daqReadinessGate_;
    DaqSettingsProvider daqSettingsProvider_;
    std::mutex controlMutex_;
    std::condition_variable stopChanged_;
    std::condition_variable pulseFinished_;
    bool running_ = false;
    bool acceptingStop_ = false;
    bool stopRequested_ = false;
    bool pulseInFlight_ = false;
    std::thread::id pulseThread_;
    std::mutex configurationMutex_;
    run::RoutingSnapshot currentRouting_;
    QVector<QString> activeModelClassIds_;
    bool configurationReady_ = false;
    std::mutex boundaryMutex_;
    run::HitBoundarySnapshot currentBoundary_;
};

} // namespace desktop_app::v2::sequence_test
