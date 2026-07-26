#include "runs_results_controller.h"

#include "../state/application_state_store.h"
#include "run_repository.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUrl>

#include <algorithm>
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

QString jsonFactsText(const QJsonObject &facts)
{
    if (facts.isEmpty())
        return QStringLiteral("Not recorded");
    return QString::fromUtf8(QJsonDocument(facts).toJson(QJsonDocument::Compact));
}

QString classSnapshotText(const std::optional<run::ModelSnapshot> &model)
{
    if (!model || model->classes.isEmpty())
        return QStringLiteral("Not applicable");

    QStringList classes;
    classes.reserve(model->classes.size());
    for (const run::RunClassSnapshot &entry : model->classes)
        classes.append(QStringLiteral("%1 — %2").arg(entry.id, entry.name));
    return classes.join(QStringLiteral("; "));
}

QString predictedCountsText(const run::RunManifestData &data,
                            const run::RunDerivedCounts &counts)
{
    if (!data.model || data.model->classes.isEmpty())
        return QStringLiteral("Not applicable");

    QStringList values;
    const qsizetype count = std::min(data.model->classes.size(),
                                     counts.predictedByClass.size());
    values.reserve(count + (counts.unclassified > 0 ? 1 : 0));
    for (qsizetype index = 0; index < count; ++index) {
        const run::RunClassSnapshot &entry = data.model->classes.at(index);
        values.append(QStringLiteral("%1 (%2): %3")
                          .arg(entry.name, entry.id)
                          .arg(counts.predictedByClass.at(index)));
    }
    if (counts.unclassified > 0)
        values.append(QStringLiteral("Unclassified: %1").arg(counts.unclassified));
    return values.join(QLatin1Char('\n'));
}

QString hitClassText(const run::RunManifestData &data)
{
    if (data.routing.triggerMode == run::TriggerMode::EveryDroplet)
        return QStringLiteral("Not applicable");
    if (!data.routing.hitClassId)
        return QStringLiteral("Not recorded");
    if (data.model) {
        for (const run::RunClassSnapshot &entry : data.model->classes) {
            if (entry.id == *data.routing.hitClassId)
                return QStringLiteral("%1 (%2)").arg(entry.name, entry.id);
        }
    }
    return *data.routing.hitClassId;
}

QString existingPathReason(const QString &path, bool requireDirectory,
                           const QString &missingReason)
{
    if (path.trimmed().isEmpty())
        return missingReason;
    const QFileInfo info(path);
    if (!info.exists() || (requireDirectory ? !info.isDir() : !info.isFile()))
        return missingReason;
    return {};
}

} // namespace

RunsResultsController::RunsResultsController(RunRepository &repository,
                                             ApplicationStateStore &stateStore,
                                             ArtifactOpener artifactOpener,
                                             QObject *parent)
    : QObject(parent)
    , repository_(repository)
    , stateStore_(stateStore)
    , artifactOpener_(std::move(artifactOpener))
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

    const run::RunManifestData &data = loaded->manifest.data();
    const run::RunDerivedCounts &counts = loaded->manifest.derivedCounts();
    QVariantMap result = entryMap(loaded->entry);
    result.insert(QStringLiteral("notes"), data.notes);
    result.insert(QStringLiteral("experimentType"), data.experimentType);
    result.insert(QStringLiteral("endedAt"), data.endedAt);
    result.insert(QStringLiteral("requestedDuration"),
                  data.requestedDurationSeconds
                      ? QString::number(*data.requestedDurationSeconds) +
                            QStringLiteral(" s")
                      : QStringLiteral("Not set"));
    result.insert(QStringLiteral("stopReason"), loaded->entry.stopReason);
    result.insert(QStringLiteral("runFolderPath"), loaded->entry.runFolderPath);
    result.insert(QStringLiteral("eventsPath"), loaded->entry.eventsPath);
    result.insert(QStringLiteral("cropsPath"), loaded->entry.cropsPath);
    result.insert(QStringLiteral("sequencePath"), loaded->entry.sequencePath.value_or(QString{}));
    result.insert(QStringLiteral("sequenceReason"), loaded->entry.sequenceReason);
    result.insert(QStringLiteral("modelId"),
                  data.model ? data.model->id : QStringLiteral("Not applicable"));
    result.insert(QStringLiteral("modelChecksum"),
                  data.model ? data.model->sha256 : QStringLiteral("Not applicable"));
    result.insert(QStringLiteral("classSnapshot"), classSnapshotText(data.model));
    result.insert(QStringLiteral("triggerMode"),
                  data.routing.triggerMode == run::TriggerMode::EveryDroplet
                      ? QStringLiteral("Every Droplet")
                      : QStringLiteral("Class-Based"));
    result.insert(QStringLiteral("hitClass"), hitClassText(data));
    result.insert(QStringLiteral("hitBoundary"),
                  QStringLiteral("y=%1 px; %2 is Hit; image %3 × %4")
                      .arg(data.hitBoundary.boundaryY)
                      .arg(data.hitBoundary.hitSide == run::HitSide::PositiveY
                               ? QStringLiteral("positive Y")
                               : QStringLiteral("negative Y"))
                      .arg(data.hitBoundary.imageWidth)
                      .arg(data.hitBoundary.imageHeight));
    result.insert(QStringLiteral("physicalDaqOutput"),
                  data.routing.physicalDaqOutputEnabled
                      ? QStringLiteral("On")
                      : QStringLiteral("Off"));
    result.insert(QStringLiteral("cameraSettings"), jsonFactsText(data.cameraSettings));
    result.insert(QStringLiteral("daqSettings"), jsonFactsText(data.daqSettings));
    result.insert(QStringLiteral("detectorSettings"), jsonFactsText(data.detectorSettings));
    result.insert(QStringLiteral("cropSettings"), jsonFactsText(data.cropSettings));
    result.insert(QStringLiteral("timingSettings"), jsonFactsText(data.timingSettings));
    result.insert(QStringLiteral("opendssVersion"), data.opendssVersion);
    result.insert(QStringLiteral("requestedProcessingFps"), data.requestedProcessingFps);
    result.insert(QStringLiteral("achievedProcessingFps"), data.achievedProcessingFps);
    result.insert(QStringLiteral("predictedCounts"), predictedCountsText(data, counts));
    result.insert(QStringLiteral("decisionHit"), counts.decisionHit);
    result.insert(QStringLiteral("decisionWaste"), counts.decisionWaste);
    result.insert(QStringLiteral("observedHit"), counts.observedHit);
    result.insert(QStringLiteral("observedWaste"), counts.observedWaste);
    result.insert(QStringLiteral("observedUnresolved"), counts.observedUnresolved);
    result.insert(QStringLiteral("hitDecisionHitObserved"), counts.hitDecisionHitObserved);
    result.insert(QStringLiteral("hitDecisionWasteObserved"), counts.hitDecisionWasteObserved);
    result.insert(QStringLiteral("hitDecisionUnresolved"), counts.hitDecisionUnresolved);
    result.insert(QStringLiteral("wasteDecisionHitObserved"), counts.wasteDecisionHitObserved);
    result.insert(QStringLiteral("wasteDecisionWasteObserved"), counts.wasteDecisionWasteObserved);
    result.insert(QStringLiteral("wasteDecisionUnresolved"), counts.wasteDecisionUnresolved);

    const QString eventsReason = existingPathReason(
        loaded->entry.eventsPath, false, QStringLiteral("Droplet Log is unavailable."));
    const QString folderReason = existingPathReason(
        loaded->entry.runFolderPath, true, QStringLiteral("Run folder is unavailable."));
    const QString cropsReason = existingPathReason(
        loaded->entry.cropsPath, true, QStringLiteral("Droplet Crop folder is unavailable."));
    QString sequenceReason = loaded->entry.sequenceReason;
    if (loaded->entry.sequencePath) {
        sequenceReason = existingPathReason(
            *loaded->entry.sequencePath, false,
            QStringLiteral("Saved Image Sequence is unavailable."));
    } else if (sequenceReason.isEmpty()) {
        sequenceReason = QStringLiteral("No saved Image Sequence for this Run.");
    }
    result.insert(QStringLiteral("eventsAvailable"), eventsReason.isEmpty());
    result.insert(QStringLiteral("eventsReason"), eventsReason);
    result.insert(QStringLiteral("runFolderAvailable"), folderReason.isEmpty());
    result.insert(QStringLiteral("runFolderReason"), folderReason);
    result.insert(QStringLiteral("cropsAvailable"), cropsReason.isEmpty());
    result.insert(QStringLiteral("cropsReason"), cropsReason);
    result.insert(QStringLiteral("sequenceAvailable"), sequenceReason.isEmpty());
    result.insert(QStringLiteral("sequenceReason"), sequenceReason);
    result.insert(QStringLiteral("cropsFolderUrl"),
                  cropsReason.isEmpty()
                      ? QUrl::fromLocalFile(loaded->entry.cropsPath)
                      : QUrl{});
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

bool RunsResultsController::openDropletLog()
{
    const std::optional<LoadedRun> loaded = repository_.loadedRun();
    if (!loaded) {
        publishError(QStringLiteral("No Run is loaded."));
        return false;
    }
    return openExistingPath(loaded->entry.eventsPath, false,
                            QStringLiteral("Droplet Log is unavailable."));
}

bool RunsResultsController::openRunFolder()
{
    const std::optional<LoadedRun> loaded = repository_.loadedRun();
    if (!loaded) {
        publishError(QStringLiteral("No Run is loaded."));
        return false;
    }
    return openExistingPath(loaded->entry.runFolderPath, true,
                            QStringLiteral("Run folder is unavailable."));
}

bool RunsResultsController::openDropletCrop(const QUrl &cropUrl)
{
    const std::optional<LoadedRun> loaded = repository_.loadedRun();
    if (!loaded) {
        publishError(QStringLiteral("No Run is loaded."));
        return false;
    }
    if (!cropUrl.isLocalFile() || cropUrl.hasQuery() || cropUrl.hasFragment()) {
        publishError(QStringLiteral("Selected Droplet Crop must be a local file."));
        return false;
    }

    const QString cropsRoot = QDir::fromNativeSeparators(
        QFileInfo(loaded->entry.cropsPath).canonicalFilePath());
    const QString cropPath = QDir::fromNativeSeparators(
        QFileInfo(cropUrl.toLocalFile()).canonicalFilePath());
    const QString cropsPrefix = cropsRoot + QLatin1Char('/');
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity pathSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity pathSensitivity = Qt::CaseSensitive;
#endif
    if (cropsRoot.isEmpty() || cropPath.isEmpty() ||
        !cropPath.startsWith(cropsPrefix, pathSensitivity)) {
        publishError(QStringLiteral("Selected file is not a Droplet Crop from this Run."));
        return false;
    }
    return openExistingPath(cropPath, false,
                            QStringLiteral("Selected Droplet Crop is unavailable."));
}

bool RunsResultsController::openSavedSequence()
{
    const std::optional<LoadedRun> loaded = repository_.loadedRun();
    if (!loaded) {
        publishError(QStringLiteral("No Run is loaded."));
        return false;
    }
    if (!loaded->entry.sequencePath) {
        publishError(loaded->entry.sequenceReason.isEmpty()
                         ? QStringLiteral("No saved Image Sequence for this Run.")
                         : loaded->entry.sequenceReason);
        return false;
    }
    const QString reason = existingPathReason(
        *loaded->entry.sequencePath, false,
        QStringLiteral("Saved Image Sequence is unavailable."));
    if (!reason.isEmpty()) {
        publishError(reason);
        return false;
    }
    emit savedSequenceRequested(*loaded->entry.sequencePath);
    return true;
}

bool RunsResultsController::openRunSummary(const QUrl &summaryUrl)
{
    if (!summaryUrl.isLocalFile() || summaryUrl.hasQuery() ||
        summaryUrl.hasFragment()) {
        publishError(QStringLiteral("Run Summary must be a local file."));
        return false;
    }
    const QString requested =
        QFileInfo(summaryUrl.toLocalFile()).canonicalFilePath();
    if (requested.isEmpty()) {
        publishError(QStringLiteral("Run Summary is unavailable."));
        return false;
    }
    if (!refresh())
        return false;
    for (const RunEntry &entry : repository_.entries()) {
        if (QFileInfo(entry.summaryPath).canonicalFilePath().compare(
                requested, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!repository_.selectRun(entry.id)) {
            publishError(QStringLiteral("Run Summary is unavailable."));
            return false;
        }
        QString error;
        if (!repository_.loadSelected(&error)) {
            publishError(error);
            return false;
        }
        return true;
    }
    publishError(QStringLiteral("Run Summary is outside the configured Runs location."));
    return false;
}

bool RunsResultsController::openExistingPath(const QString &path, bool requireDirectory,
                                             const QString &unavailableMessage)
{
    const QString reason = existingPathReason(path, requireDirectory, unavailableMessage);
    if (!reason.isEmpty()) {
        publishError(reason);
        return false;
    }
    if (!artifactOpener_) {
        publishError(QStringLiteral("Desktop file opening service is unavailable."));
        return false;
    }
    if (!artifactOpener_(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()))) {
        publishError(QStringLiteral("Unable to request opening the selected Run artifact."));
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
