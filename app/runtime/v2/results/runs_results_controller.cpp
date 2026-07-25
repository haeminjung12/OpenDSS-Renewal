#include "runs_results_controller.h"

#include "../state/application_state_store.h"
#include "run_repository.h"

#include <optional>
#include <utility>

namespace desktop_app::v2::results {
namespace {

QString operationText(run::RunOperation operation)
{
    return operation == run::RunOperation::LiveSorting
        ? QStringLiteral("Live Sorting")
        : QStringLiteral("Sequence Test");
}

QString statusText(run::RunStatus status)
{
    switch (status) {
    case run::RunStatus::Completed:
        return QStringLiteral("Completed");
    case run::RunStatus::Stopped:
        return QStringLiteral("Stopped");
    case run::RunStatus::Interrupted:
        return QStringLiteral("Interrupted");
    case run::RunStatus::Failed:
        return QStringLiteral("Failed");
    }
    return {};
}

QVariantMap entryMap(const RunEntry &entry)
{
    QVariantMap result{{QStringLiteral("id"), entry.id},
                       {QStringLiteral("runName"), entry.runName},
                       {QStringLiteral("loadable"), entry.loadable},
                       {QStringLiteral("reason"), entry.reason}};
    if (!entry.loadable)
        return result;

    result.insert(QStringLiteral("operation"), operationText(entry.operation));
    result.insert(QStringLiteral("status"), statusText(entry.status));
    result.insert(QStringLiteral("startedAt"), entry.startedAt);
    result.insert(QStringLiteral("durationSeconds"), entry.elapsedDurationSeconds);
    result.insert(QStringLiteral("totalCount"), entry.counts.total);
    result.insert(QStringLiteral("modelName"), entry.modelName.value_or(QString{}));
    result.insert(QStringLiteral("recoverable"), entry.recoverable);
    return result;
}

} // namespace

RunsResultsController::RunsResultsController(RunRepository &repository,
                                             ApplicationStateStore &stateStore,
                                             QObject *parent)
    : QObject(parent)
    , repository_(repository)
    , stateStore_(stateStore)
{
    connect(&stateStore_, &ApplicationStateStore::changed, this, [this] {
        emit selectedRunIdChanged();
        emit loadedRunChanged();
        emit errorMessageChanged();
    });
}

QVariantList RunsResultsController::runs() const
{
    QVariantList result;
    const QVector<RunEntry> entries = repository_.entries();
    result.reserve(entries.size());
    for (const RunEntry &entry : entries)
        result.append(entryMap(entry));
    return result;
}

QString RunsResultsController::selectedRunId() const
{
    return stateStore_.snapshot().results.selectedRunId;
}

QVariantMap RunsResultsController::loadedRun() const
{
    const std::optional<LoadedRun> loaded = repository_.loadedRun();
    if (!loaded)
        return {};

    QVariantMap result = entryMap(loaded->entry);
    result.insert(QStringLiteral("notes"), loaded->manifest.data().notes);
    result.insert(QStringLiteral("stopReason"), loaded->entry.stopReason);
    result.insert(QStringLiteral("runFolderPath"), loaded->entry.runFolderPath);
    result.insert(QStringLiteral("eventsPath"), loaded->entry.eventsPath);
    result.insert(QStringLiteral("cropsPath"), loaded->entry.cropsPath);
    result.insert(QStringLiteral("sequencePath"), loaded->entry.sequencePath.value_or(QString{}));
    result.insert(QStringLiteral("sequenceReason"), loaded->entry.sequenceReason);
    return result;
}

QString RunsResultsController::errorMessage() const
{
    return stateStore_.snapshot().results.loadError;
}

bool RunsResultsController::refresh()
{
    QString error;
    if (!repository_.refresh(stateStore_.snapshot().preferences.storageRoot, &error)) {
        publishError(error);
        return false;
    }
    emit runsChanged();
    return true;
}

bool RunsResultsController::selectRun(const QString &id)
{
    if (!repository_.selectRun(id)) {
        publishError(QStringLiteral("Selected Run is unavailable."));
        return false;
    }
    return true;
}

bool RunsResultsController::loadSelected()
{
    QString error;
    if (!repository_.loadSelected(&error))
        return false;
    return true;
}

bool RunsResultsController::updateLoadedNotes(const QString &notes)
{
    QString error;
    if (!repository_.updateLoadedNotes(notes, &error)) {
        publishError(error);
        return false;
    }
    return true;
}

void RunsResultsController::publishError(const QString &message)
{
    ResultsState results = stateStore_.snapshot().results;
    results.loadError = message;
    stateStore_.publishResults(std::move(results));
}

} // namespace desktop_app::v2::results
