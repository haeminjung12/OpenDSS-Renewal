#pragma once

#include "../run/run_manifest_v2.h"

#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2 {

class ApplicationStateStore;

namespace results {

struct RunEntry {
    QString id;
    QString summaryPath;
    QString runFolderPath;
    QString runName;
    run::RunOperation operation = run::RunOperation::SequenceTest;
    QString experimentType;
    run::RunStatus status = run::RunStatus::Completed;
    QString startedAt;
    QString endedAt;
    double elapsedDurationSeconds = 0.0;
    QString stopReason;
    std::optional<QString> modelName;
    run::RunDerivedCounts counts;
    QString eventsPath;
    QString cropsPath;
    std::optional<QString> sequencePath;
    QString sequenceReason;
    QString reason;
    bool loadable = false;
    bool recoverable = false;
};

struct LoadedRun {
    RunEntry entry;
    run::RunManifestV2 manifest;
};

class RunRepository {
  public:
    explicit RunRepository(ApplicationStateStore& stateStore);

    bool refresh(const QString& dataRoot, QString* error = nullptr);
    bool selectRun(const QString& id);
    bool loadSelected(QString* error = nullptr);
    bool updateLoadedNotes(const QString& notes, QString* error = nullptr);

    const QVector<RunEntry>& entries() const noexcept;
    const QString& selectedRunId() const noexcept;
    const std::optional<LoadedRun>& loadedRun() const noexcept;

  private:
    void publish(const QString& loadError = {});

    ApplicationStateStore& stateStore_;
    QVector<RunEntry> entries_;
    QString selectedRunId_;
    std::optional<LoadedRun> loadedRun_;
};

} // namespace results
} // namespace desktop_app::v2
