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

constexpr auto kBinaryLabelSchema = "droplet-labels-target-nontarget-binary-v1";
constexpr auto kThreeClassLabelSchema = "droplet-labels-target-nontarget-3class-v1";
constexpr auto kModelRegistryOverrideEnv = "OVDS_MODEL_REGISTRY_PATH";

QString defaultDocumentsPath() {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
        documents = QDir::home().filePath("Documents");
    return documents;
}

QString existingFileOrDirectoryPath(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return QString();
    const QFileInfo info(trimmed);
    if (info.isFile() || info.isDir())
        return info.absoluteFilePath();
    const QFileInfo parentInfo(info.absolutePath());
    if (parentInfo.isDir())
        return parentInfo.absoluteFilePath();
    return QString();
}

QString existingDirectoryPath(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return QString();
    const QFileInfo info(trimmed);
    if (info.isDir())
        return info.absoluteFilePath();
    if (info.isFile())
        return info.absolutePath();
    const QFileInfo parentInfo(info.absolutePath());
    if (parentInfo.isDir())
        return parentInfo.absoluteFilePath();
    return QString();
}

QString ensureWorkspaceDirectory(const QString& path, bool fileDialog) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return QString();
    const QString root = defaultOpenDssRootPath();
    QString candidateDir = trimmed;
    if (fileDialog && QFileInfo(trimmed).suffix().size() > 0)
        candidateDir = QFileInfo(trimmed).absolutePath();
    if (!root.isEmpty() && candidateDir.startsWith(root, Qt::CaseInsensitive))
        QDir().mkpath(candidateDir);
    return fileDialog ? existingFileOrDirectoryPath(trimmed) : existingDirectoryPath(trimmed);
}

QString packagedPathCandidate(const QString& relativePath) {
    const QString trimmed = QDir::fromNativeSeparators(relativePath.trimmed());
    if (trimmed.isEmpty())
        return QString();
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString direct = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(direct))
            return direct;
        const QString internal = QDir(projectRoot).absoluteFilePath("internal-release/" + trimmed);
        if (QFileInfo::exists(internal))
            return internal;
    }
    QDir probe(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = probe.absoluteFilePath(trimmed);
        if (QFileInfo::exists(candidate))
            return candidate;
        if (!probe.cdUp())
            break;
    }
    return QString();
}

QString resolveDialogPath(const QString& currentPath, const QString& workspacePath, const QString& packagedPath,
                          bool fileDialog) {
    const QString current = fileDialog ? existingFileOrDirectoryPath(currentPath) : existingDirectoryPath(currentPath);
    if (!current.isEmpty())
        return current;

    const QString workspace = ensureWorkspaceDirectory(workspacePath, fileDialog);
    if (!workspace.isEmpty())
        return workspace;

    const QString packaged = fileDialog ? existingFileOrDirectoryPath(packagedPath) : existingDirectoryPath(packagedPath);
    if (!packaged.isEmpty())
        return packaged;

    const QString documents = existingDirectoryPath(defaultDocumentsPath());
    if (!documents.isEmpty())
        return documents;
    return QDir::homePath();
}

QJsonObject makePackagedPromotedModelRegistryEntry() {
    QJsonObject promoted;
    promoted["registry_entry_id"] = "run_20260429_221500_wsl2_binary_linuxmirror_onnx";
    promoted["display_name"] = "Promoted/current binary runtime";
    promoted["state"] = "promoted_current";
    promoted["live_use_mode"] = "normal";
    promoted["selectable_for_normal_live_sorting"] = true;
    promoted["model_path"] = "app/runtime/models/squeezenet_final_new_condition.onnx";
    promoted["model_sha256"] = "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769";
    promoted["metadata_path"] = "app/runtime/models/metadata.json";
    promoted["metadata_sha256"] = "574f48481c0f1fa2c165aaac01cc8e4245ff00cd0be5b4e384e05a3b2999a2eb";
    promoted["metadata_status"] = "Pass";
    promoted["validation_status"] = "Default hashes match / image pass / NI pass / sequence provisional";
    promoted["promotion_status"] = "Promoted/current";
    promoted["promotion_record_path"] = "docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    QJsonObject promotedDisplayLabels;
    promotedDisplayLabels["0"] = "Non-target";
    promotedDisplayLabels["1"] = "Target";
    promoted["classes"] = QJsonArray{"0", "1"};
    promoted["display_labels"] = promotedDisplayLabels;
    promoted["label_schema_version"] = kBinaryLabelSchema;
    QJsonObject promotedTarget;
    promotedTarget["target_class_id"] = "1";
    promotedTarget["target_display_label"] = "Target";
    promotedTarget["waste_class_id"] = "0";
    promotedTarget["waste_display_label"] = "Non-target";
    promoted["target_policy"] = promotedTarget;
    promoted["limitations"] = QJsonArray{"Sequence validation remains provisional by policy for public claims.",
                                         "Public model/data release approvals remain separate."};
    promoted["blockers"] = QJsonArray{};
    return promoted;
}

QJsonObject makePackagedBlankModelRegistryEntry() {
    QJsonObject blank;
    blank["registry_entry_id"] = "blank_squeezenet_template_seed42";
    blank["display_name"] = "Blank SqueezeNet starter (ImageNet-start)";
    blank["state"] = "available";
    blank["live_use_mode"] = "blocked";
    blank["selectable_for_normal_live_sorting"] = false;
    blank["model_id"] = "blank_squeezenet_template_seed42";
    blank["model_path"] = "app/runtime/models/blank_squeezenet_template.onnx";
    blank["model_sha256"] = "9cf87bfc56962976845f9f62a121a37ef81751e8e8932b3366784b06a0be4528";
    blank["metadata_path"] = "app/runtime/models/blank_squeezenet_template_metadata.json";
    blank["metadata_schema_version"] = "model-metadata-v1";
    blank["metadata_sha256"] = "e8dd91c038b63311bd48ca60ea2230fa5a473090913dde98c6735ede8e46c6a6";
    blank["metadata_status"] = "Bundled ImageNet-start training starter";
    blank["validation_status"] = "Bundled starter only / not validated for live sorting";
    blank["promotion_status"] = "Starter only";
    QJsonObject blankDisplayLabels;
    blankDisplayLabels["0"] = "Non-target";
    blankDisplayLabels["1"] = "Target";
    blank["classes"] = QJsonArray{"0", "1"};
    blank["display_labels"] = blankDisplayLabels;
    blank["label_schema_version"] = kBinaryLabelSchema;
    QJsonObject blankTarget;
    blankTarget["target_class_id"] = "1";
    blankTarget["target_display_label"] = "Target";
    blankTarget["trigger_rule"] = "trigger_on_target_class";
    blankTarget["waste_class_id"] = "0";
    blankTarget["waste_display_label"] = "Non-target";
    blank["target_policy"] = blankTarget;
    blank["limitations"] = QJsonArray{"Bundled OpenDSS ImageNet-start training starter; runtime loads the packaged "
                                      "ONNX and metadata.",
                                      "Not droplet-trained.",
                                      "Do not use this starter for live sorting or DAQ-triggered operation without "
                                      "separate training and validation."};
    QJsonObject blocker;
    blocker["blocker"] = "Training starter only";
    blocker["evidence"] = "Bundled package asset: app/runtime/models/blank_squeezenet_template.onnx";
    blocker["owner_type"] = "Model Manager";
    blocker["required_next_action"] = "Train and validate this bundled starter before any live sorting use.";
    blank["blockers"] = QJsonArray{blocker};
    blank["validation_evidence"] = QJsonObject{};
    blank["model_sidecars"] = QJsonArray{};
    return blank;
}

QJsonObject makePackagedPretrainedModelRegistryEntry() {
    QJsonObject backup;
    backup["registry_entry_id"] = "pre_binary_promotion_backup";
    backup["display_name"] = "Cell aggregate model V1 (2026-05-14)";
    backup["state"] = "available";
    backup["live_use_mode"] = "normal";
    backup["selectable_for_normal_live_sorting"] = false;
    backup["model_id"] = "pre_binary_promotion_backup";
    backup["model_path"] = "app/runtime/models/pre_binary_promotion_backup.onnx";
    backup["model_sha256"] = "8b534dbec19d4f37e75803f6d01c9f32827f9d394c92a59c21c2ac6b23a2d1fd";
    backup["metadata_path"] = "app/runtime/models/pre_binary_promotion_backup_metadata.json";
    backup["metadata_schema_version"] = "legacy";
    backup["metadata_sha256"] = "3b4467d4a24a9182af85825e1cf28563f4bae6af60fc04ff75f348d8f5792acf";
    backup["metadata_status"] = "Legacy schema";
    backup["validation_status"] = "Legacy backup packaged as active default";
    backup["promotion_status"] = "Available";
    backup["promotion_record_path"] =
        "open-visual-droplet-sorter-suite/docs/worker-reports/2026-04-30-actual-model-promotion-execution.md";
    backup["classes"] = QJsonArray{"0", "1", "2"};
    QJsonObject backupDisplayLabels;
    backupDisplayLabels["0"] = "Non-target A";
    backupDisplayLabels["1"] = "Target";
    backupDisplayLabels["2"] = "Non-target B";
    backup["display_labels"] = backupDisplayLabels;
    backup["label_schema_version"] = kThreeClassLabelSchema;
    QJsonObject backupTarget;
    backupTarget["target_class_id"] = "1";
    backupTarget["target_display_label"] = "Target";
    backupTarget["trigger_rule"] = "trigger_on_target_class";
    backupTarget["waste_class_id"] = "0";
    backupTarget["waste_display_label"] = "Non-target A";
    backup["target_policy"] = backupTarget;
    backup["limitations"] =
        QJsonArray{"Legacy three-class runtime retained as the packaged active default for this internal release."};
    QJsonObject blocker;
    blocker["blocker"] = "Retired backup";
    blocker["evidence"] = "outputs/model_promotion_backups/pre_binary_promotion_20260430_055156/";
    blocker["owner_type"] = "Model Manager";
    blocker["required_next_action"] = "Use the promoted/current default model for normal live sorting.";
    backup["blockers"] = QJsonArray{blocker};
    backup["validation_evidence"] = QJsonObject{};
    backup["model_sidecars"] = QJsonArray{};
    return backup;
}

} // namespace

QString defaultOpenDssRootPath() {
    const QString userProfile = qEnvironmentVariable("USERPROFILE").trimmed();
    if (!userProfile.isEmpty())
        return QDir(userProfile).filePath("Documents/OpenDSS");
    return QDir(defaultDocumentsPath()).filePath("OpenDSS");
}

QString defaultOpenDssModelsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("models");
}

QString defaultOpenDssDatasetsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("datasets");
}

QString defaultOpenDssPreparedDatasetsPath() {
    return QDir(defaultOpenDssDatasetsPath()).filePath("prepared");
}

QString defaultOpenDssRunsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("runs");
}

QString defaultOpenDssTrainingRunsPath() {
    return QDir(defaultOpenDssRunsPath()).filePath("training");
}

QString defaultOpenDssValidationRunsPath() {
    return QDir(defaultOpenDssRunsPath()).filePath("validation");
}

QString findPackagedAppPath(const QString& relativePath) {
    return packagedPathCandidate(relativePath);
}

QString chooseOpenFileDialogPath(const QString& currentPath, const QString& workspacePath, const QString& packagedPath) {
    return resolveDialogPath(currentPath, workspacePath, packagedPath, true);
}

QString chooseExistingDirectoryDialogPath(const QString& currentPath, const QString& workspacePath,
                                          const QString& packagedPath) {
    return resolveDialogPath(currentPath, workspacePath, packagedPath, false);
}

QJsonObject packagedPromotedModelRegistryEntry() {
    return makePackagedPromotedModelRegistryEntry();
}

QJsonObject packagedBlankModelRegistryEntry() {
    return makePackagedBlankModelRegistryEntry();
}

QJsonObject packagedPretrainedModelRegistryEntry() {
    return makePackagedPretrainedModelRegistryEntry();
}

bool copyFileIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (!QFileInfo(sourcePath).isFile())
        return false;
    if (QFileInfo(destinationPath).isFile())
        return true;
    QDir().mkpath(QFileInfo(destinationPath).absolutePath());
    return QFile::copy(sourcePath, destinationPath);
}

bool copyDirectoryIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (sourcePath.trimmed().isEmpty())
        return false;
    if (!QFileInfo(sourcePath).isDir())
        return false;
    if (QFileInfo(destinationPath).isDir())
        return true;
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

QString packagedModelRegistryPath() {
    QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString registry = runtimeModelArtifactPath(projectRoot, "app/runtime/models/model_registry.json");
        if (!registry.isEmpty())
            return registry;
        return QDir(projectRoot).absoluteFilePath("app/runtime/models/model_registry.json");
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        QString candidate = dir.filePath("models/model_registry.json");
        if (QFileInfo(candidate).exists())
            return candidate;
        if (!dir.cdUp())
            break;
    }
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("models/model_registry.json");
}

QString writableModelRegistryPath() {
    return QDir(defaultOpenDssModelsPath()).filePath("model_registry.json");
}

bool writeRegistryFile(const QString& targetPath, const QJsonObject& registry, QString* error) {
    if (error)
        error->clear();
    const QFileInfo targetInfo(targetPath);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        if (error)
            *error = "Unable to create model registry directory: " + targetInfo.absolutePath();
        return false;
    }
    QFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error)
            *error = "Model registry file not writable: " + targetPath;
        return false;
    }
    target.write(QJsonDocument(registry).toJson(QJsonDocument::Indented));
    return true;
}

bool seedRegistryFile(const QString& targetPath, const QString& packagedPath, QString* error) {
    if (error)
        error->clear();
    if (targetPath.trimmed().isEmpty()) {
        if (error)
            *error = "No model registry target path is available.";
        return false;
    }
    if (QFileInfo(targetPath).isFile())
        return true;

    if (!packagedPath.trimmed().isEmpty() && QFileInfo(packagedPath).isFile()) {
        QFile source(packagedPath);
        if (!source.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (error)
                *error = "Packaged model registry is not readable: " + packagedPath;
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(source.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error)
                *error = "Packaged model registry is invalid: " + parseError.errorString();
            return false;
        }
        return writeRegistryFile(targetPath, doc.object(), error);
    }

    return writeRegistryFile(targetPath, temporaryStaticModelRegistry(), error);
}

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath) {
    if (projectRoot.isEmpty() || relativePath.trimmed().isEmpty())
        return QString();
    const QString trimmed = relativePath.trimmed();
    const QStringList candidates = {
        QDir(projectRoot).absoluteFilePath(trimmed), QDir(projectRoot).absoluteFilePath("internal-release/" + trimmed),
        QDir(projectRoot).absoluteFilePath("internal-release/app/runtime/models/" + QFileInfo(trimmed).fileName()),
        QDir(projectRoot).absoluteFilePath("app/runtime/models/" + QFileInfo(trimmed).fileName())};
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

QString modelRegistryPath() {
    const QString overridePath = qEnvironmentVariable(kModelRegistryOverrideEnv).trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();
    return writableModelRegistryPath();
}

QJsonObject temporaryStaticModelRegistry() {
    QJsonArray entries;
    entries.append(packagedPromotedModelRegistryEntry());
    entries.append(packagedBlankModelRegistryEntry());
    entries.append(packagedPretrainedModelRegistryEntry());

    QJsonObject registry;
    registry["schema_version"] = "model-registry-v1";
    registry["registry_id"] = "temporary-static-fallback";
    registry["source"] = "temporary_static_fallback_missing_or_invalid_file";
    registry["entries"] = entries;
    return registry;
}

QJsonObject loadModelRegistry(QString* loadedPath, QString* loadWarning) {
    QString path = modelRegistryPath();
    const QString packagedPath = packagedModelRegistryPath();
    QString seedError;
    if (!seedRegistryFile(path, packagedPath, &seedError) && !packagedPath.trimmed().isEmpty() &&
        QFileInfo(packagedPath).isFile()) {
        path = packagedPath;
    }
    if (loadedPath)
        *loadedPath = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (loadWarning)
            *loadWarning = seedError.isEmpty() ? "Model registry file not readable; using temporary static fallback: " + path
                                               : seedError;
        return temporaryStaticModelRegistry();
    }
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (loadWarning)
            *loadWarning = "Model registry file invalid; using temporary static fallback: " + parseError.errorString();
        return temporaryStaticModelRegistry();
    }
    QJsonObject registry = doc.object();
    if (registry.value("schema_version").toString() != "model-registry-v1" || !registry.value("entries").isArray()) {
        if (loadWarning)
            *loadWarning = "Model registry schema missing/unsupported; using temporary static fallback.";
        return temporaryStaticModelRegistry();
    }
    if (loadWarning)
        loadWarning->clear();
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
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return trimmed;
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
    if (!warning.isEmpty())
        lines << "Model warning: " + warning;
    Q_UNUSED(registryPath);
    const QString displayName = registryString(entry, "display_name");
    lines << "Model: " + (displayName.isEmpty() ? registryString(entry, "registry_entry_id") : displayName);

    QStringList classLines;
    QJsonArray classes = entry.value("classes").toArray();
    QJsonObject displayLabels = entry.value("display_labels").toObject();
    for (const auto& value : classes) {
        QString classId = value.toString();
        QString display = displayLabels.value(classId).toString(classId);
        classLines << display;
    }
    lines << "Labels: " + classLines.join(", ");
    const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label");
    if (!targetDisplay.isEmpty())
        lines << "Sort Target: " + targetDisplay;
    const QString wasteDisplay = registryNestedString(entry, "target_policy", "waste_display_label");
    if (!wasteDisplay.isEmpty())
        lines << "Sort Non-target: " + wasteDisplay;
    lines << "Validation: " + registryString(entry, "validation_status");

    QStringList limitations;
    for (const auto& value : entry.value("limitations").toArray())
        limitations << value.toString();
    if (!limitations.isEmpty())
        lines << "Notes: " + limitations.join("; ");

    QStringList blockerTexts;
    for (const auto& value : entry.value("blockers").toArray()) {
        QJsonObject blocker = value.toObject();
        QString text = blocker.value("blocker").toString();
        QString action = blocker.value("required_next_action").toString();
        if (!action.isEmpty())
            text += " - " + action;
        if (!text.trimmed().isEmpty())
            blockerTexts << text;
    }
    if (!blockerTexts.isEmpty())
        lines << "Needs attention: " + blockerTexts.join("; ");
    return lines.join("\n");
}

QString resolvePackagedPathFromRegistryPath(const QString& registryPath) {
    const QString trimmed = registryPath.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return trimmed;
    const QString projectRoot = findProjectRootFromApp();
    if (!projectRoot.isEmpty()) {
        const QString resolved = runtimeModelArtifactPath(projectRoot, trimmed);
        if (!resolved.isEmpty())
            return resolved;
        const QString direct = QDir(projectRoot).absoluteFilePath(trimmed);
        if (QFileInfo::exists(direct))
            return direct;
    }
    QDir probe(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; ++i) {
        const QString candidate = probe.absoluteFilePath(trimmed);
        if (QFileInfo::exists(candidate))
            return candidate;
        const QString modelsCandidate = probe.absoluteFilePath("models/" + QFileInfo(trimmed).fileName());
        if (QFileInfo::exists(modelsCandidate))
            return modelsCandidate;
        if (!probe.cdUp())
            break;
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
    paths.root = defaultOpenDssRootPath();
    paths.models = defaultOpenDssModelsPath();
    paths.datasets = defaultOpenDssDatasetsPath();
    paths.preparedDatasets = defaultOpenDssPreparedDatasetsPath();
    paths.runs = defaultOpenDssRunsPath();
    paths.trainingRuns = defaultOpenDssTrainingRunsPath();
    paths.validationRuns = defaultOpenDssValidationRunsPath();
    QDir().mkpath(paths.models);
    QDir().mkpath(paths.datasets);
    QDir().mkpath(paths.preparedDatasets);
    QDir().mkpath(paths.runs);
    QDir().mkpath(paths.trainingRuns);
    QDir().mkpath(paths.validationRuns);

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
    const QStringList packagedDatasets = {"droplet_target_nontarget_binary_starter",
                                          "droplet_target_nontarget_3class_starter"};
    for (const auto& datasetName : packagedDatasets) {
        QString sourceDataset;
        if (!projectRoot.isEmpty()) {
            const QStringList candidates = {
                QDir(projectRoot).absoluteFilePath("internal-release/datasets/prepared/" + datasetName),
                QDir(projectRoot).absoluteFilePath("datasets/prepared/" + datasetName)};
            for (const auto& candidate : candidates) {
                if (QFileInfo(candidate).isDir()) {
                    sourceDataset = candidate;
                    break;
                }
            }
        }
        const QString destinationDataset = QDir(paths.preparedDatasets).filePath(datasetName);
        copyDirectoryIfMissing(sourceDataset, destinationDataset);
        if (datasetName == "droplet_target_nontarget_binary_starter")
            paths.preparedDataset = destinationDataset;
    }
    return paths;
}
