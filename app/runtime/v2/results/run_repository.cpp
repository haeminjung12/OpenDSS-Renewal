#include "run_repository.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

namespace desktop_app::v2::results {
namespace {

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

bool canonicalSummaryPath(const QString& path, QString& canonical,
                          QString* error) {
    if (containsParentTraversal(path))
        return fail(error, "Run Summary path must not contain parent traversal.");

    const QFileInfo info(path);
    if (info.fileName() != QStringLiteral("run_summary.json") || !info.exists() ||
        !info.isFile() || info.isSymLink()) {
        return fail(error, "Expected a finalized regular run_summary.json file.");
    }
    const QFileInfo folder(info.absolutePath());
    if (!folder.exists() || !folder.isDir() || folder.isSymLink())
        return fail(error, "Run folder is not a regular directory.");

    canonical = QDir::fromNativeSeparators(info.canonicalFilePath());
    const QString canonicalFolder =
        QDir::fromNativeSeparators(folder.canonicalFilePath());
    if (canonical.isEmpty() || canonicalFolder.isEmpty() ||
        QFileInfo(canonical).absolutePath().compare(canonicalFolder,
                                                   pathSensitivity()) != 0) {
        return fail(error, "Run Summary is not canonically contained in its Run folder.");
    }
    return true;
}

bool resolveContained(const QString& runFolder, const QString& relative,
                      bool requireDirectory, QString& resolved, QString* error) {
    const QString canonicalRoot =
        QDir::fromNativeSeparators(QFileInfo(runFolder).canonicalFilePath());
    const QFileInfo candidate(QDir(runFolder).filePath(relative));
    const QString canonicalCandidate =
        QDir::fromNativeSeparators(candidate.canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty() ||
        (requireDirectory ? !candidate.isDir() : !candidate.exists()) ||
        candidate.isSymLink()) {
        return fail(error, QString("Missing or linked Run artifact '%1'.").arg(relative));
    }
    if (canonicalCandidate.compare(canonicalRoot, pathSensitivity()) != 0 &&
        !canonicalCandidate.startsWith(canonicalRoot + '/', pathSensitivity())) {
        return fail(error, QString("Run artifact '%1' escapes its Run folder.")
                               .arg(relative));
    }

    QString current = runFolder;
    for (const QString& part :
         QDir::fromNativeSeparators(relative).split('/', Qt::SkipEmptyParts)) {
        current = QDir(current).filePath(part);
        if (QFileInfo(current).isSymLink())
            return fail(error,
                        QString("Run artifact '%1' traverses a link.").arg(relative));
    }
    resolved = canonicalCandidate;
    return true;
}

bool validatedPaths(const QString& summaryPath, const run::RunManifestData& data,
                    RunSummary* summary, QString* error) {
    const QString runFolder = QFileInfo(summaryPath).absolutePath();
    QString eventsPath;
    QString cropsPath;
    if (!resolveContained(runFolder, data.files.eventsCsv, false, eventsPath, error) ||
        !QFileInfo(eventsPath).isFile() ||
        !resolveContained(runFolder, data.files.cropsPath, true, cropsPath, error)) {
        return false;
    }
    std::optional<QString> sequencePath;
    if (data.files.sequencePath) {
        QString resolved;
        if (!resolveContained(runFolder, *data.files.sequencePath, false, resolved,
                              error)) {
            return false;
        }
        sequencePath = resolved;
    }
    if (summary) {
        summary->summaryPath = summaryPath;
        summary->runFolderPath =
            QDir::fromNativeSeparators(QFileInfo(runFolder).canonicalFilePath());
        summary->eventsPath = eventsPath;
        summary->cropsPath = cropsPath;
        summary->sequencePath = sequencePath;
    }
    return true;
}

std::optional<run::RunManifestV2> loadSupported(const QString& path,
                                                QString* error) {
    QString canonical;
    if (!canonicalSummaryPath(path, canonical, error))
        return std::nullopt;
    auto manifest = run::RunManifestV2::load(canonical, error);
    if (!manifest)
        return std::nullopt;
    if (manifest->data().operation != run::RunOperation::SequenceTest) {
        fail(error,
             "Live Sorting Run discovery is unavailable until the Live Run contract is implemented.");
        return std::nullopt;
    }
    if (!validatedPaths(canonical, manifest->data(), nullptr, error))
        return std::nullopt;
    return manifest;
}

RunSummary makeSummary(const QString& summaryPath,
                       const run::RunManifestV2& manifest) {
    RunSummary summary;
    const auto& data = manifest.data();
    summary.runId = data.runId;
    summary.runName = data.runName;
    summary.operation = data.operation;
    summary.experimentType = data.experimentType;
    summary.status = data.status;
    summary.startedAt = data.startedAt;
    summary.endedAt = data.endedAt;
    summary.stopReason = data.stopReason;
    if (data.model)
        summary.modelName = data.model->name;
    summary.counts = manifest.derivedCounts();
    const QDateTime started = QDateTime::fromString(data.startedAt, Qt::ISODate);
    const QDateTime ended = QDateTime::fromString(data.endedAt, Qt::ISODate);
    summary.elapsedDurationSeconds = started.msecsTo(ended) / 1000.0;
    QString ignored;
    validatedPaths(summaryPath, data, &summary, &ignored);
    return summary;
}

bool readObject(const QString& path, QJsonObject& object, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, "Could not read Run Summary for notes update.");
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, "Run Summary changed to invalid JSON.");
    object = document.object();
    return true;
}

std::optional<QByteArray> fileHash(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail(error, QString("Could not hash Run artifact '%1'.").arg(path));
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        fail(error, QString("Could not hash Run artifact '%1'.").arg(path));
        return std::nullopt;
    }
    return hash.result();
}

bool artifactHashes(const QString& runFolder, const run::RunManifestData& data,
                    QVector<QPair<QString, QByteArray>>& hashes, QString* error) {
    QStringList paths{data.files.eventsCsv};
    for (const auto& event : data.events)
        paths.push_back(event.cropPath);
    paths.removeDuplicates();
    for (const QString& relative : paths) {
        QString resolved;
        if (!resolveContained(runFolder, relative, false, resolved, error))
            return false;
        auto hash = fileHash(resolved, error);
        if (!hash)
            return false;
        hashes.push_back({resolved, *hash});
    }
    return true;
}

bool hashesUnchanged(const QVector<QPair<QString, QByteArray>>& before,
                     QString* error) {
    for (const auto& item : before) {
        auto after = fileHash(item.first, error);
        if (!after || *after != item.second)
            return fail(error, "Run event or Droplet Crop data changed during notes update.");
    }
    return true;
}

} // namespace

QVector<RunSummary> RunRepository::discover(const QString& dataRoot,
                                            QString* error) {
    if (error)
        error->clear();
    QVector<RunSummary> result;
    QStringList diagnostics;

    const QFileInfo supplied(dataRoot);
    if (!supplied.exists() || !supplied.isDir() || supplied.isSymLink()) {
        fail(error, "Run discovery root is not a regular directory.");
        return result;
    }

    const QString explicitSummary =
        QDir(supplied.absoluteFilePath()).filePath(QStringLiteral("run_summary.json"));
    QStringList candidates;
    if (QFileInfo::exists(explicitSummary)) {
        candidates.push_back(explicitSummary);
    } else {
        QString container = supplied.absoluteFilePath();
        const QFileInfo defaultRuns(
            QDir(container).filePath(QStringLiteral("Runs")));
        if (defaultRuns.exists()) {
            if (!defaultRuns.isDir() || defaultRuns.isSymLink()) {
                fail(error, "Default Runs path is not a regular directory.");
                return result;
            }
            container = defaultRuns.absoluteFilePath();
        }
        const QFileInfoList children =
            QDir(container).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                          QDir::Name);
        for (const QFileInfo& child : children) {
            if (child.isSymLink()) {
                diagnostics.push_back(
                    QString("%1: linked Run directory was skipped.")
                        .arg(child.absoluteFilePath()));
                continue;
            }
            const QString candidate =
                QDir(child.absoluteFilePath()).filePath("run_summary.json");
            if (QFileInfo::exists(candidate))
                candidates.push_back(candidate);
        }
    }

    for (const QString& candidate : candidates) {
        QString diagnostic;
        auto manifest = loadSupported(candidate, &diagnostic);
        if (!manifest) {
            diagnostics.push_back(QString("%1: %2").arg(candidate, diagnostic));
            continue;
        }
        QString canonical;
        canonicalSummaryPath(candidate, canonical, nullptr);
        result.push_back(makeSummary(canonical, *manifest));
    }

    std::sort(result.begin(), result.end(),
              [](const RunSummary& left, const RunSummary& right) {
                  const QDateTime leftStarted =
                      QDateTime::fromString(left.startedAt, Qt::ISODate);
                  const QDateTime rightStarted =
                      QDateTime::fromString(right.startedAt, Qt::ISODate);
                  if (leftStarted != rightStarted)
                      return leftStarted > rightStarted;
                  return left.summaryPath < right.summaryPath;
              });
    if (error && !diagnostics.isEmpty())
        *error = diagnostics.join('\n');
    return result;
}

std::optional<run::RunManifestV2>
RunRepository::load(const QString& summaryPath, QString* error) {
    if (error)
        error->clear();
    return loadSupported(summaryPath, error);
}

bool RunRepository::updateNotes(const QString& summaryPath, const QString& notes,
                                QString* error) {
    if (error)
        error->clear();
    QString canonical;
    if (!canonicalSummaryPath(summaryPath, canonical, error))
        return false;
    auto manifest = loadSupported(canonical, error);
    if (!manifest)
        return false;

    QJsonObject beforeObject;
    if (!readObject(canonical, beforeObject, error))
        return false;
    const QString runFolder = QFileInfo(canonical).absolutePath();
    QVector<QPair<QString, QByteArray>> beforeHashes;
    if (!artifactHashes(runFolder, manifest->data(), beforeHashes, error))
        return false;

    run::RunManifestData updated = manifest->data();
    updated.notes = notes;
    if (!run::RunManifestV2::save(canonical, updated, error))
        return false;

    auto afterManifest = loadSupported(canonical, error);
    QJsonObject afterObject;
    if (!afterManifest || !readObject(canonical, afterObject, error))
        return false;
    beforeObject.remove(QStringLiteral("notes"));
    afterObject.remove(QStringLiteral("notes"));
    if (beforeObject != afterObject ||
        afterManifest->data().notes != notes) {
        return fail(error, "Notes update changed non-note Run Summary data.");
    }
    return hashesUnchanged(beforeHashes, error);
}

} // namespace desktop_app::v2::results
