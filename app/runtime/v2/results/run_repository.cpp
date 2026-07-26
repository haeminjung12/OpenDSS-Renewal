#include "run_repository.h"

#include "../sequence/sequence_manifest_v2.h"
#include "../state/application_state_store.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>

#include <algorithm>

namespace desktop_app::v2::results {
namespace {

const QString FinalSummaryName = QStringLiteral("run_summary.json");
const QString PartialSummaryName = QStringLiteral("run_summary.partial.json");
const QString PartialEventsName = QStringLiteral("events.partial.csv");
const QString MissingSequenceReason =
    QStringLiteral("No saved Image Sequence for this Run");

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

Qt::CaseSensitivity pathSensitivity() {
#ifdef Q_OS_WIN
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool containsParentTraversal(const QString& path) {
    return QDir::fromNativeSeparators(path)
        .split('/', Qt::SkipEmptyParts)
        .contains(QStringLiteral(".."));
}

bool isLinkOrJunction(const QFileInfo& info) {
    return info.isSymLink() || info.isJunction();
}

bool canonicalMatchesAbsolute(const QFileInfo& info) {
    const QString canonical = QDir::fromNativeSeparators(info.canonicalFilePath());
    const QString absolute = QDir::fromNativeSeparators(
        QDir::cleanPath(info.absoluteFilePath()));
    return !canonical.isEmpty() &&
           canonical.compare(absolute, pathSensitivity()) == 0;
}

bool canonicalManifestPath(const QString& path, bool allowPartial,
                           QString& canonical, QString* error) {
    if (containsParentTraversal(path))
        return fail(error, "Run Summary path must not contain parent traversal.");

    const QFileInfo info(path);
    const bool expectedName =
        info.fileName() == FinalSummaryName ||
        (allowPartial && info.fileName() == PartialSummaryName);
    if (!expectedName || !info.exists() || !info.isFile() ||
        isLinkOrJunction(info) || !canonicalMatchesAbsolute(info)) {
        return fail(error, allowPartial
                               ? "Expected a regular Run Summary file."
                               : "Expected a finalized regular run_summary.json file.");
    }
    const QFileInfo folder(info.absolutePath());
    if (!folder.exists() || !folder.isDir() || isLinkOrJunction(folder) ||
        !canonicalMatchesAbsolute(folder)) {
        return fail(error, "Run folder is not a regular directory.");
    }

    canonical = QDir::fromNativeSeparators(info.canonicalFilePath());
    const QString canonicalFolder =
        QDir::fromNativeSeparators(folder.canonicalFilePath());
    if (canonical.isEmpty() || canonicalFolder.isEmpty() ||
        QFileInfo(canonical).absolutePath().compare(canonicalFolder,
                                                   pathSensitivity()) != 0) {
        return fail(error,
                    "Run Summary is not canonically contained in its Run folder.");
    }
    return true;
}

bool resolveContained(const QString& runFolder, const QString& relative,
                      bool requireDirectory, QString& resolved, QString* error) {
    if (containsParentTraversal(relative) || QDir::isAbsolutePath(relative))
        return fail(error,
                    QString("Run artifact '%1' escapes its Run folder.").arg(relative));

    const QString canonicalRoot =
        QDir::fromNativeSeparators(QFileInfo(runFolder).canonicalFilePath());
    const QFileInfo candidate(QDir(runFolder).filePath(relative));
    const QString canonicalCandidate =
        QDir::fromNativeSeparators(candidate.canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty() ||
        (requireDirectory ? !candidate.isDir() : !candidate.isFile()) ||
        isLinkOrJunction(candidate) || !canonicalMatchesAbsolute(candidate)) {
        return fail(error,
                    QString("Missing or linked Run artifact '%1'.").arg(relative));
    }
    if (canonicalCandidate.compare(canonicalRoot, pathSensitivity()) != 0 &&
        !canonicalCandidate.startsWith(canonicalRoot + '/', pathSensitivity())) {
        return fail(error,
                    QString("Run artifact '%1' escapes its Run folder.").arg(relative));
    }

    QString current = runFolder;
    for (const QString& part :
         QDir::fromNativeSeparators(relative).split('/', Qt::SkipEmptyParts)) {
        current = QDir(current).filePath(part);
        if (isLinkOrJunction(QFileInfo(current)))
            return fail(
                error, QString("Run artifact '%1' traverses a link.").arg(relative));
    }
    resolved = canonicalCandidate;
    return true;
}

QString stableFolderId(const QFileInfo& folder) {
    const QString canonical =
        QDir::fromNativeSeparators(folder.canonicalFilePath());
    if (!canonical.isEmpty())
        return canonical;
    return QDir::fromNativeSeparators(QDir::cleanPath(folder.absoluteFilePath()));
}

bool populatePaths(const QString& summaryPath, bool recoverable,
                   const run::RunManifestData& data, RunEntry& entry,
                   QString* error) {
    const QString runFolder = QFileInfo(summaryPath).absolutePath();
    const QString eventsRelative =
        recoverable ? PartialEventsName : data.files.eventsCsv;
    if (!resolveContained(runFolder, eventsRelative, false, entry.eventsPath,
                          error) ||
        !resolveContained(runFolder, data.files.cropsPath, true, entry.cropsPath,
                          error)) {
        return false;
    }

    entry.sequencePath.reset();
    entry.sequenceReason.clear();
    if (data.files.sequencePath) {
        QString sequenceFolder;
        QString sequenceManifest;
        QString sequenceError;
        const QFileInfo sequenceReference(*data.files.sequencePath);
        const bool referencesManifest =
            sequenceReference.fileName() == QStringLiteral("sequence.json");
        const QString folderRelative =
            referencesManifest ? sequenceReference.path() : *data.files.sequencePath;
        const QString manifestRelative =
            referencesManifest
                ? *data.files.sequencePath
                : QDir(*data.files.sequencePath).filePath(QStringLiteral("sequence.json"));
        if (resolveContained(runFolder, folderRelative, true,
                             sequenceFolder, &sequenceError) &&
            resolveContained(runFolder, manifestRelative, false, sequenceManifest,
                             &sequenceError) &&
            sequence::SequenceManifestV2::load(sequenceManifest, &sequenceError)) {
            entry.sequencePath = sequenceManifest;
        } else {
            entry.sequenceReason = MissingSequenceReason;
        }
    }
    return true;
}

RunEntry makeInvalidEntry(const QFileInfo& folder, const QString& summaryPath,
                          const QString& reason) {
    RunEntry entry;
    entry.id = stableFolderId(folder);
    entry.summaryPath = summaryPath;
    entry.runFolderPath = stableFolderId(folder);
    entry.runName = folder.fileName();
    entry.startedAt = folder.lastModified().toUTC().toString(Qt::ISODateWithMs);
    entry.reason = reason;
    return entry;
}

std::optional<LoadedRun> loadEntry(const RunEntry& source, QString* error) {
    QString canonical;
    if (!canonicalManifestPath(source.summaryPath, true, canonical, error))
        return std::nullopt;
    auto manifest = run::RunManifestV2::load(canonical, error);
    if (!manifest)
        return std::nullopt;
    RunEntry entry = source;
    const auto& data = manifest->data();
    entry.id = data.runId.isEmpty() ? canonical : data.runId;
    entry.summaryPath = canonical;
    entry.runFolderPath = QDir::fromNativeSeparators(
        QFileInfo(canonical).absoluteDir().canonicalPath());
    entry.runName = data.runName;
    entry.operation = data.operation;
    entry.experimentType = data.experimentType;
    entry.status = data.status;
    entry.startedAt = data.startedAt;
    entry.endedAt = data.endedAt;
    entry.stopReason = data.stopReason;
    entry.modelName.reset();
    if (data.model)
        entry.modelName = data.model->name;
    entry.counts = manifest->derivedCounts();
    const QDateTime started = QDateTime::fromString(data.startedAt, Qt::ISODate);
    const QDateTime ended = QDateTime::fromString(data.endedAt, Qt::ISODate);
    entry.elapsedDurationSeconds = started.msecsTo(ended) / 1000.0;
    entry.recoverable =
        QFileInfo(canonical).fileName() == PartialSummaryName;
    if (!populatePaths(canonical, entry.recoverable, data, entry, error))
        return std::nullopt;
    entry.loadable = true;
    entry.reason.clear();
    return LoadedRun{std::move(entry), std::move(*manifest)};
}

bool samePath(const QString& left, const QString& right) {
    if (left.isEmpty() || right.isEmpty())
        return false;
    const QString leftCanonical =
        QDir::fromNativeSeparators(QFileInfo(left).canonicalFilePath());
    const QString rightCanonical =
        QDir::fromNativeSeparators(QFileInfo(right).canonicalFilePath());
    return !leftCanonical.isEmpty() && !rightCanonical.isEmpty() &&
           leftCanonical.compare(rightCanonical, pathSensitivity()) == 0;
}

} // namespace

RunRepository::RunRepository(ApplicationStateStore& stateStore)
    : stateStore_(stateStore) {
    publish();
}

bool RunRepository::refresh(const QString& dataRoot, QString* error) {
    if (error)
        error->clear();
    const QFileInfo supplied(dataRoot);
    if (!supplied.exists() || !supplied.isDir() || isLinkOrJunction(supplied) ||
        !canonicalMatchesAbsolute(supplied)) {
        return fail(error, "Run discovery root is not a regular directory.");
    }

    QString container = supplied.absoluteFilePath();
    const QFileInfo defaultRuns(QDir(container).filePath(QStringLiteral("Runs")));
    if (defaultRuns.exists()) {
        if (!defaultRuns.isDir() || isLinkOrJunction(defaultRuns) ||
            !canonicalMatchesAbsolute(defaultRuns)) {
            return fail(error, "Default Runs path is not a regular directory.");
        }
        container = defaultRuns.absoluteFilePath();
    }

    QVector<RunEntry> refreshed;
    const QFileInfoList children =
        QDir(container).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                      QDir::Name);
    for (const QFileInfo& child : children) {
        if (isLinkOrJunction(child) || !canonicalMatchesAbsolute(child)) {
            refreshed.push_back(makeInvalidEntry(
                child, {}, QStringLiteral("Run folder is linked or unreadable.")));
            continue;
        }
        const QString finalPath =
            QDir(child.absoluteFilePath()).filePath(FinalSummaryName);
        const QString partialPath =
            QDir(child.absoluteFilePath()).filePath(PartialSummaryName);
        const QString candidate = QFileInfo::exists(finalPath) ? finalPath : partialPath;
        if (!QFileInfo::exists(candidate)) {
            refreshed.push_back(makeInvalidEntry(
                child, {}, QStringLiteral("Run Summary is missing.")));
            continue;
        }

        RunEntry seed = makeInvalidEntry(child, candidate, {});
        QString loadError;
        auto loaded = loadEntry(seed, &loadError);
        if (loaded) {
            refreshed.push_back(std::move(loaded->entry));
        } else {
            seed.reason = loadError;
            refreshed.push_back(std::move(seed));
        }
    }

    std::sort(refreshed.begin(), refreshed.end(),
              [](const RunEntry& left, const RunEntry& right) {
                  const QDateTime leftStarted =
                      QDateTime::fromString(left.startedAt, Qt::ISODate);
                  const QDateTime rightStarted =
                      QDateTime::fromString(right.startedAt, Qt::ISODate);
                  if (leftStarted != rightStarted)
                      return leftStarted > rightStarted;
                  return left.id < right.id;
              });
    entries_ = std::move(refreshed);
    if (std::none_of(entries_.cbegin(), entries_.cend(),
                     [this](const RunEntry& entry) {
                         return entry.id == selectedRunId_;
                     })) {
        selectedRunId_.clear();
    }
    publish();
    return true;
}

bool RunRepository::selectRun(const QString& id) {
    const auto found = std::find_if(
        entries_.cbegin(), entries_.cend(),
        [&id](const RunEntry& entry) { return entry.id == id; });
    if (found == entries_.cend())
        return false;
    selectedRunId_ = id;
    publish();
    return true;
}

bool RunRepository::loadSelected(QString* error) {
    if (error)
        error->clear();
    const auto selected = std::find_if(
        entries_.cbegin(), entries_.cend(),
        [this](const RunEntry& entry) { return entry.id == selectedRunId_; });
    if (selected == entries_.cend()) {
        const QString reason = QStringLiteral("No Run is selected.");
        publish(reason);
        return fail(error, reason);
    }
    if (!selected->loadable) {
        publish(selected->reason);
        return fail(error, selected->reason);
    }

    QString loadError;
    auto loaded = loadEntry(*selected, &loadError);
    if (!loaded) {
        publish(loadError);
        return fail(error, loadError);
    }
    loadedRun_ = std::move(loaded);
    publish();
    return true;
}

bool RunRepository::updateLoadedNotes(const QString& notes, QString* error) {
    if (error)
        error->clear();
    if (!loadedRun_)
        return fail(error, "No Run is loaded.");
    if (loadedRun_->entry.recoverable)
        return fail(error, "Run Notes require a finalized run_summary.json.");

    const auto application = stateStore_.snapshot();
    if (application.run.status == RunStatus::Open &&
        (application.run.runId == loadedRun_->entry.id ||
         samePath(application.run.path, loadedRun_->entry.runFolderPath))) {
        return fail(error, "Run Notes cannot be changed while this Run is active.");
    }

    const QString summaryPath = loadedRun_->entry.summaryPath;
    QString canonical;
    if (!canonicalManifestPath(summaryPath, false, canonical, error))
        return false;
    QLockFile lock(canonical + QStringLiteral(".lock"));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100))
        return fail(error, "Run Notes are locked by another OpenDSS update.");

    RunEntry seed = loadedRun_->entry;
    auto current = loadEntry(seed, error);
    if (!current || current->entry.recoverable)
        return false;
    run::RunManifestData updated = current->manifest.data();
    updated.notes = notes;
    if (!run::RunManifestV2::save(canonical, updated, error))
        return false;

    auto reloaded = loadEntry(seed, error);
    if (!reloaded)
        return false;
    loadedRun_ = std::move(reloaded);
    publish();
    return true;
}

QVector<RunEntry> RunRepository::entries() const {
    return entries_;
}

QString RunRepository::selectedRunId() const {
    return selectedRunId_;
}

std::optional<LoadedRun> RunRepository::loadedRun() const {
    return loadedRun_;
}

void RunRepository::publish(const QString& loadError) {
    stateStore_.publishResults(
        {selectedRunId_,
         loadedRun_ ? loadedRun_->entry.id : QString{},
         loadError});
}

} // namespace desktop_app::v2::results
