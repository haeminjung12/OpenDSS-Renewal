#include "model_registry_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

#include "app_utils.h"

namespace {

QString defaultOpenDssRoot() {
    const QString userProfile = qEnvironmentVariable("USERPROFILE").trimmed();
    if (!userProfile.isEmpty()) {
        return QDir(userProfile).filePath("Documents/OpenDSS");
    }
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::home().filePath("Documents");
    return QDir(documents).filePath("OpenDSS");
}

bool copyFileIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (!QFileInfo(sourcePath).isFile()) return false;
    if (QFileInfo(destinationPath).isFile()) return true;
    QDir().mkpath(QFileInfo(destinationPath).absolutePath());
    return QFile::copy(sourcePath, destinationPath);
}

bool copyDirectoryIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (sourcePath.trimmed().isEmpty()) return false;
    if (!QFileInfo(sourcePath).isDir()) return false;
    if (QFileInfo(destinationPath).isDir()) return true;
    QDir().mkpath(destinationPath);
    QDirIterator it(sourcePath, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    bool ok = true;
    while (it.hasNext()) {
        it.next();
        const QString relative = QDir(sourcePath).relativeFilePath(it.filePath());
        const QString destination = QDir(destinationPath).filePath(relative);
        if (it.fileInfo().isDir()) {
            ok = QDir().mkpath(destination) && ok;
        } else if (!QFileInfo(destination).isFile()) {
            QDir().mkpath(QFileInfo(destination).absolutePath());
            ok = QFile::copy(it.filePath(), destination) && ok;
        }
    }
    return ok;
}

}  // namespace

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath) {
    if (projectRoot.isEmpty() || relativePath.trimmed().isEmpty()) return QString();
    const QString trimmed = relativePath.trimmed();
    const QStringList candidates = {
        QDir(projectRoot).absoluteFilePath(trimmed),
        QDir(projectRoot).absoluteFilePath("internal-release/" + trimmed),
        QDir(projectRoot).absoluteFilePath("internal-release/app/runtime/models/" + QFileInfo(trimmed).fileName()),
        QDir(projectRoot).absoluteFilePath("app/runtime/models/" + QFileInfo(trimmed).fileName())
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return QString();
}

QString modelRegistryPath() {
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString registry = runtimeModelArtifactPath(projectRoot, "app/runtime/models/model_registry.json");
        if (!registry.isEmpty()) return registry;
        return QDir(projectRoot).absoluteFilePath("app/runtime/models/model_registry.json");
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        QString candidate = dir.filePath("models/model_registry.json");
        if (QFileInfo(candidate).exists()) return candidate;
        if (!dir.cdUp()) break;
    }
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("models/model_registry.json");
}

QJsonObject temporaryStaticModelRegistry() {
    QJsonArray entries;

    QJsonObject promoted;
    promoted["registry_entry_id"] = "run_20260429_221500_wsl2_binary_linuxmirror_onnx";
    promoted["display_name"] = "Promoted/current binary runtime";
    promoted["state"] = "promoted_current";
    promoted["live_use_mode"] = "normal";
    promoted["selectable_for_normal_live_sorting"] = true;
    promoted["model_path"] = "app/runtime/models/squeezenet_final_new_condition.onnx";
    promoted["model_sha256"] = "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769";
    promoted["metadata_path"] = "app/runtime/models/metadata.json";
    promoted["metadata_sha256"] = "fa5321dfad900baec23fa6c239a29279e0e8c03fa2e78f0bd679dfb973888d2f";
    promoted["metadata_status"] = "Pass";
    promoted["validation_status"] = "Default hashes match / image pass / NI pass / sequence provisional";
    promoted["promotion_status"] = "Promoted/current";
    promoted["promotion_record_path"] = "docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    QJsonObject promotedDisplayLabels;
    promotedDisplayLabels["0"] = "Waste";
    promotedDisplayLabels["1"] = "Hits";
    promoted["classes"] = QJsonArray{"0", "1"};
    promoted["display_labels"] = promotedDisplayLabels;
    QJsonObject promotedTarget;
    promotedTarget["target_class_id"] = "1";
    promotedTarget["target_display_label"] = "Hits";
    promotedTarget["waste_class_id"] = "0";
    promoted["target_policy"] = promotedTarget;
    promoted["limitations"] = QJsonArray{"Sequence validation remains provisional by policy for public claims.", "Public model/data release approvals remain separate."};
    promoted["blockers"] = QJsonArray{};
    entries.append(promoted);

    QJsonObject backup;
    backup["registry_entry_id"] = "pre_binary_promotion_backup";
    backup["display_name"] = "Cell aggregate model V1 (2026-05-14)";
    backup["state"] = "promoted_current";
    backup["live_use_mode"] = "normal";
    backup["selectable_for_normal_live_sorting"] = true;
    backup["model_path"] = "app/runtime/models/pre_binary_promotion_backup.onnx";
    backup["model_sha256"] = "8b534dbec19d4f37e75803f6d01c9f32827f9d394c92a59c21c2ac6b23a2d1fd";
    backup["metadata_path"] = "app/runtime/models/pre_binary_promotion_backup_metadata.json";
    backup["metadata_sha256"] = "dd5499f4c96e3b5d9c812adc114262a20a5b56e927ef15ba06d69720d4cc9bac";
    backup["metadata_status"] = "Legacy schema";
    backup["validation_status"] = "Legacy backup packaged as active default";
    backup["promotion_status"] = "Packaged active default";
    backup["promotion_record_path"] = "open-visual-droplet-sorter-suite/docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    backup["classes"] = QJsonArray{"Empty", "Single", "MoreThanTwo"};
    QJsonObject backupDisplayLabels;
    backupDisplayLabels["Empty"] = "Empty";
    backupDisplayLabels["Single"] = "Single";
    backupDisplayLabels["MoreThanTwo"] = "More than two";
    backup["display_labels"] = backupDisplayLabels;
    QJsonObject backupTarget;
    backupTarget["target_class_id"] = "Single";
    backupTarget["target_display_label"] = "Single";
    backup["target_policy"] = backupTarget;
    backup["limitations"] = QJsonArray{"Legacy three-class runtime retained as the packaged active default for this internal release."};
    backup["blockers"] = QJsonArray{};
    entries.append(backup);

    QJsonObject registry;
    registry["schema_version"] = "model-registry-v1";
    registry["registry_id"] = "temporary-static-fallback";
    registry["source"] = "temporary_static_fallback_missing_or_invalid_file";
    registry["entries"] = entries;
    return registry;
}

QJsonObject loadModelRegistry(QString* loadedPath, QString* loadWarning) {
    const QString path = modelRegistryPath();
    if (loadedPath) *loadedPath = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (loadWarning) *loadWarning = "Model registry file not readable; using temporary static fallback: " + path;
        return temporaryStaticModelRegistry();
    }
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (loadWarning) *loadWarning = "Model registry file invalid; using temporary static fallback: " + parseError.errorString();
        return temporaryStaticModelRegistry();
    }
    QJsonObject registry = doc.object();
    if (registry.value("schema_version").toString() != "model-registry-v1" ||
        !registry.value("entries").isArray()) {
        if (loadWarning) *loadWarning = "Model registry schema missing/unsupported; using temporary static fallback.";
        return temporaryStaticModelRegistry();
    }
    if (loadWarning) loadWarning->clear();
    return registry;
}

QString registryString(const QJsonObject& entry, const QString& key) {
    return entry.value(key).toString();
}

QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key) {
    return entry.value(objectKey).toObject().value(key).toString();
}

QString runtimePathFromRegistryPath(const QString& path) {
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute()) return trimmed;
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        QString absolute = runtimeModelArtifactPath(projectRoot, trimmed);
        if (QFileInfo::exists(absolute)) {
            return QDir(QCoreApplication::applicationDirPath()).relativeFilePath(absolute);
        }
    }
    return trimmed;
}

QString registryEntrySummary(const QJsonObject& entry, const QString& registryPath, const QString& warning) {
    QStringList lines;
    if (!warning.isEmpty()) lines << "Registry warning: " + warning;
    lines << "Registry source: " + registryPath;
    lines << "Model: " + registryString(entry, "registry_entry_id");
    lines << "State: " + registryString(entry, "state");
    lines << "Live-use mode: " + registryString(entry, "live_use_mode");
    lines << "ONNX: " + registryString(entry, "model_path");
    lines << "ONNX SHA-256: " + registryString(entry, "model_sha256");
    lines << "Metadata: " + registryString(entry, "metadata_path");
    lines << "Metadata SHA-256: " + registryString(entry, "metadata_sha256");

    QStringList classLines;
    QJsonArray classes = entry.value("classes").toArray();
    QJsonObject displayLabels = entry.value("display_labels").toObject();
    for (const auto& value : classes) {
        QString classId = value.toString();
        QString display = displayLabels.value(classId).toString(classId);
        classLines << QString("%1 %2").arg(classId, display);
    }
    lines << "Classes: " + classLines.join(" / ");
    lines << "Target policy: canonical class id " +
        registryNestedString(entry, "target_policy", "target_class_id") +
        ", displayed as " + registryNestedString(entry, "target_policy", "target_display_label");
    lines << "Validation: " + registryString(entry, "validation_status");
    lines << "Promotion record: " + registryString(entry, "promotion_record_path");

    QStringList limitations;
    for (const auto& value : entry.value("limitations").toArray()) limitations << value.toString();
    if (!limitations.isEmpty()) lines << "Limitations: " + limitations.join("; ");

    QStringList blockerTexts;
    for (const auto& value : entry.value("blockers").toArray()) {
        QJsonObject blocker = value.toObject();
        QString text = blocker.value("blocker").toString();
        QString action = blocker.value("required_next_action").toString();
        if (!action.isEmpty()) text += " - " + action;
        if (!text.trimmed().isEmpty()) blockerTexts << text;
    }
    if (!blockerTexts.isEmpty()) lines << "Blockers: " + blockerTexts.join("; ");
    return lines.join("\n");
}

QString resolvePackagedPathFromRegistryPath(const QString& registryPath) {
    const QString trimmed = registryPath.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute()) return trimmed;
    const QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString resolved = runtimeModelArtifactPath(projectRoot, trimmed);
        if (!resolved.isEmpty()) return resolved;
        const QString direct = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(direct)) return direct;
    }
    QDir probe(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = probe.absoluteFilePath(trimmed);
        if (QFileInfo::exists(candidate)) return candidate;
        const QString modelsCandidate = probe.absoluteFilePath("models/" + QFileInfo(trimmed).fileName());
        if (QFileInfo::exists(modelsCandidate)) return modelsCandidate;
        if (!probe.cdUp()) break;
    }
    return trimmed;
}

QJsonObject activeRegistryEntry(const QJsonArray& entries) {
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value("selectable_for_normal_live_sorting").toBool(false) &&
            registryString(entry, "live_use_mode") != "blocked") {
            return entry;
        }
    }
    return entries.isEmpty() ? QJsonObject{} : entries.first().toObject();
}

DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries) {
    DefaultWorkspacePaths paths;
    paths.root = defaultOpenDssRoot();
    paths.models = QDir(paths.root).filePath("models");
    paths.datasets = QDir(paths.root).filePath("datasets");
    paths.runs = QDir(paths.root).filePath("runs");
    QDir().mkpath(paths.models);
    QDir().mkpath(paths.datasets);
    QDir().mkpath(paths.runs);

    const QJsonObject activeEntry = activeRegistryEntry(registryEntries);
    const QString sourceModel = resolvePackagedPathFromRegistryPath(registryString(activeEntry, "model_path"));
    const QString sourceMetadata = resolvePackagedPathFromRegistryPath(registryString(activeEntry, "metadata_path"));
    if (QFileInfo(sourceModel).isFile()) {
        paths.activeModel = QDir(paths.models).filePath(QFileInfo(sourceModel).fileName());
        copyFileIfMissing(sourceModel, paths.activeModel);
    }
    if (QFileInfo(sourceMetadata).isFile()) {
        paths.activeMetadata = QDir(paths.models).filePath(QFileInfo(sourceMetadata).fileName());
        copyFileIfMissing(sourceMetadata, paths.activeMetadata);
    }

    const QString projectRoot = findProjectRootFromApp();
    QString sourceDataset;
    if (!projectRoot.isEmpty()) {
        const QStringList candidates = {
            QDir(projectRoot).absoluteFilePath("internal-release/datasets/prepared/droplet_binary_2026-04-30"),
            QDir(projectRoot).absoluteFilePath("datasets/prepared/droplet_binary_2026-04-30")
        };
        for (const auto& candidate : candidates) {
            if (QFileInfo(candidate).isDir()) {
                sourceDataset = candidate;
                break;
            }
        }
    }
    paths.preparedDataset = QDir(paths.datasets).filePath("droplet_binary_2026-04-30");
    copyDirectoryIfMissing(sourceDataset, paths.preparedDataset);
    return paths;
}
