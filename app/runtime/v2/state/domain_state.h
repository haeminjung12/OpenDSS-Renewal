#pragma once

#include <QString>

namespace desktop_app::v2 {

enum class CameraStatus {
    Unavailable,
    Ready,
    Streaming,
    Faulted,
};

struct CameraState {
    CameraStatus status = CameraStatus::Unavailable;
    QString deviceId;
    QString fault;
};

enum class DaqStatus {
    Disabled,
    Ready,
    Busy,
    Faulted,
};

struct DaqAppliedSettings {
    QString outputChannel = QStringLiteral("Dev1/ao0");
    double amplitudeVpp = 5.0;
    double frequencyHz = 10000.0;
    double durationMs = 5.0;
    double delayMs = 0.0;
};

struct DaqState {
    DaqStatus status = DaqStatus::Disabled;
    QString deviceId;
    QString fault;
    DaqAppliedSettings appliedSettings;
};

struct ActiveModelState {
    QString packageId;
    QString displayName;
    bool ready = false;
    QString fault;
};

struct DatasetState {
    QString datasetId;
    QString path;
    bool ready = false;
    QString fault;
};

struct SequenceState {
    QString sequenceId;
    QString path;
    bool ready = false;
    QString fault;
};

enum class TrainingStatus {
    Idle,
    Running,
    Completed,
    Interrupted,
    Failed,
};

struct TrainingState {
    QString executionId;
    TrainingStatus status = TrainingStatus::Idle;
    QString fault;
};

enum class RunStatus {
    None,
    Open,
    Finalized,
    Faulted,
};

struct RunState {
    QString runId;
    QString path;
    RunStatus status = RunStatus::None;
    QString fault;
};

struct ResultsState {
    QString selectedRunId;
    QString loadedRunId;
    QString loadError;
};

struct PreferencesState {
    QString storageRoot;
    int textSizePercent = 100;
};

struct ApplicationSnapshot {
    CameraState camera;
    DaqState daq;
    ActiveModelState activeModel;
    DatasetState dataset;
    SequenceState sequence;
    TrainingState training;
    RunState run;
    ResultsState results;
    PreferencesState preferences;
};

} // namespace desktop_app::v2
