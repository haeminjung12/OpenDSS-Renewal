#pragma once

#include "../run/run_manifest_v2.h"

#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2::results {

struct RunSummary {
    QString summaryPath;
    QString runFolderPath;
    QString runId;
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
};

class RunRepository {
  public:
    // Invalid candidate Runs are skipped. When any are skipped, error contains
    // newline-separated diagnostics while valid Runs are still returned.
    static QVector<RunSummary> discover(const QString& dataRoot,
                                        QString* error = nullptr);

    static std::optional<run::RunManifestV2> load(const QString& summaryPath,
                                                  QString* error = nullptr);

    static bool updateNotes(const QString& summaryPath, const QString& notes,
                            QString* error = nullptr);
};

} // namespace desktop_app::v2::results
