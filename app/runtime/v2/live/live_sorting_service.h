#pragma once

#include "../operation/operation_coordinator.h"
#include "../run/run_manifest_v2.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class IDropletDetector;
class QImage;
struct FrameMeta;

namespace cv {
class Mat;
}

namespace desktop_app::v2 {

class ModelLoadService;

namespace live {

struct LiveInferenceResult {
    QVector<double> scores;
};

struct PreparedLiveModel {
    run::ModelSnapshot snapshot;
    std::function<std::optional<LiveInferenceResult>(const cv::Mat&, QString*)> classify;
};

using LiveModelProvider = std::function<std::optional<PreparedLiveModel>(QString*)>;
using HitPulseCallback = std::function<run::DaqPulseStatus(bool, QString*)>;
using DaqReadinessGate = std::function<bool(QString*)>;
using PersistenceGate = std::function<bool(QString*)>;
using DispatcherStartGate = std::function<bool()>;

struct LiveSortingRequest {
    QString outputRoot;
    QString runName;
    QString experimentType;
    QString notes;
    run::TriggerMode triggerMode = run::TriggerMode::EveryDroplet;
    std::optional<QString> hitClassId;
    run::HitBoundarySnapshot hitBoundary;
    std::optional<double> requestedDurationSeconds;
    bool useActiveModel = false;
    QString opendssVersion;
    QJsonObject detectorSettings;
    QJsonObject cropSettings;
    QJsonObject timingSettings;
    QJsonObject cameraSettings;
    QJsonObject daqSettings;
    bool daqOutputEnabled = false;
    bool recordFullImageSequence = false;
};

struct LiveSortingSnapshot {
    OperationLifecycle lifecycle = OperationLifecycle::Idle;
    QString runFolder;
    double elapsedSeconds = 0.0;
    qint64 persistedEvents = 0;
    run::RunIntegrity integrity;
    QString diagnostic;
    QString stopReason;
};

class LiveSortingService final {
public:
    LiveSortingService(OperationCoordinator& operations,
                       IDropletDetector& detector,
                       ModelLoadService* modelLoader,
                       HitPulseCallback pulse,
                       LiveModelProvider modelProvider = {},
                       PersistenceGate persistenceGate = {},
                       DispatcherStartGate dispatcherStartGate = {},
                       DaqReadinessGate daqReadinessGate = {});
    ~LiveSortingService();

    LiveSortingService(const LiveSortingService&) = delete;
    LiveSortingService& operator=(const LiveSortingService&) = delete;

    bool start(const LiveSortingRequest& request, QString* error = nullptr);
    bool offerFrame(const QImage& image, const FrameMeta& meta, double fps);
    bool pause(QString* error = nullptr);
    bool resume(QString* error = nullptr);
    bool stop(QString* error = nullptr);
    bool pollDuration(QString* error = nullptr);
    LiveSortingSnapshot snapshot() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
} // namespace desktop_app::v2
