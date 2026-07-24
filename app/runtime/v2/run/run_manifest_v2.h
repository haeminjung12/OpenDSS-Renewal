#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2::run {

enum class RunOperation { SequenceTest, LiveSorting };
enum class RunStatus { Completed, Stopped, Interrupted, Failed };
enum class TriggerMode { ClassBased, EveryDroplet };
enum class HitSide { PositiveY, NegativeY };
enum class Route { Hit, Waste, Unresolved };
enum class DaqPulseStatus { NotRequested, Requested, Issued, SuppressedNotIssued, Failed };

struct RunClassSnapshot {
    QString id;
    QString name;
};

struct ModelSnapshot {
    QString id;
    QString name;
    QString sha256;
    QVector<RunClassSnapshot> classes;
};

struct SourceSequenceSnapshot {
    QString id;
    QString name;
    QString manifestPath;
};

struct RoutingSnapshot {
    TriggerMode triggerMode = TriggerMode::EveryDroplet;
    std::optional<QString> hitClassId;
    bool physicalDaqOutputEnabled = false;
};

struct HitBoundarySnapshot {
    double boundaryY = 0.0;
    HitSide hitSide = HitSide::PositiveY;
    int imageWidth = 0;
    int imageHeight = 0;
};

struct RunEvent {
    QString eventId;
    QString detectionTimestamp;
    qint64 sourceFrameIndex = 0;
    QString effectiveConfigurationId = QStringLiteral("initial");
    QString cropPath;
    std::optional<QString> predictedClassId;
    QVector<double> scores;
    Route decision = Route::Waste;
    Route observedRoute = Route::Unresolved;
    DaqPulseStatus daqPulseStatus = DaqPulseStatus::NotRequested;
    std::optional<double> inferenceTimeMs;
};

struct RunFiles {
    QString eventsCsv = QStringLiteral("events.csv");
    QString cropsPath = QStringLiteral("crops");
    std::optional<QString> sequencePath;
};

struct RunIntegrityRange {
    qint64 first = 0;
    qint64 last = 0;
};

struct RunIntegritySeries {
    qint64 count = 0;
    QVector<RunIntegrityRange> ranges;
};

struct RunIntegrity {
    RunIntegritySeries sourceFrameGaps;
    RunIntegritySeries queueRejections;
    RunIntegritySeries consumerFailures;
};

struct RunManifestData {
    QString runId;
    QString runName;
    RunOperation operation = RunOperation::SequenceTest;
    QString experimentType;
    QString notes;
    RunStatus status = RunStatus::Completed;
    QString startedAt;
    QString endedAt;
    std::optional<double> requestedDurationSeconds;
    QString stopReason;
    QString opendssVersion;
    SourceSequenceSnapshot sourceSequence;
    std::optional<ModelSnapshot> model;
    RoutingSnapshot routing;
    QJsonObject cameraSettings;
    QJsonObject detectorSettings;
    QJsonObject cropSettings;
    QJsonObject daqSettings;
    QJsonObject timingSettings;
    HitBoundarySnapshot hitBoundary;
    double requestedProcessingFps = 0.0;
    double achievedProcessingFps = 0.0;
    RunIntegrity integrity;
    RunFiles files;
    QVector<RunEvent> events;
};

struct RunDerivedCounts {
    qint64 total = 0;
    QVector<qint64> predictedByClass;
    qint64 unclassified = 0;
    qint64 decisionHit = 0;
    qint64 decisionWaste = 0;
    qint64 observedHit = 0;
    qint64 observedWaste = 0;
    qint64 observedUnresolved = 0;
    qint64 hitDecisionHitObserved = 0;
    qint64 hitDecisionWasteObserved = 0;
    qint64 hitDecisionUnresolved = 0;
    qint64 wasteDecisionHitObserved = 0;
    qint64 wasteDecisionWasteObserved = 0;
    qint64 wasteDecisionUnresolved = 0;
};

class RunManifestV2 {
  public:
    static constexpr auto SchemaVersion = "opendss.run.v2";

    static std::optional<RunManifestV2> load(const QString& path, QString* error = nullptr);
    static bool save(const QString& path, const RunManifestData& data,
                     QString* error = nullptr);
    static bool savePartial(const QString& path, const RunManifestData& data,
                            QString* error = nullptr);

    const RunManifestData& data() const noexcept;
    const RunDerivedCounts& derivedCounts() const noexcept;

  private:
    RunManifestData data_;
    RunDerivedCounts derived_;
};

} // namespace desktop_app::v2::run
