#include "model_registry_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

#include "app_utils.h"
#include "json_persistence.h"

QString packagedModelRegistryPath();

namespace {

bool readRegistryForPackageMutation(const QString& registryFilePath,
                                    QJsonObject* registry, QString* error);
bool pathTraversesReparsePoint(const QString& path);

constexpr auto kBinaryLabelSchema = "droplet-labels-target-nontarget-binary-v1";
constexpr auto kThreeClassLabelSchema = "droplet-labels-target-nontarget-3class-v1";
constexpr auto kModelRegistryOverrideEnv = "OVDS_MODEL_REGISTRY_PATH";
constexpr auto kModelsRootOverrideEnv = "OVDS_MODELS_ROOT_PATH";
constexpr auto kSuppressedTrainedModelsKey = "suppressed_trained_models";

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

QString absoluteCleanPath(const QString& path) {
    const QString trimmed = path.trimmed();
    return trimmed.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(trimmed).absoluteFilePath());
}

QString normalizedPathForComparison(const QString& path) {
    QString normalized = QDir::fromNativeSeparators(QDir::cleanPath(path.trimmed())).toLower();
    while (normalized.contains("//"))
        normalized.replace("//", "/");
    return normalized;
}

bool pathLooksLikeDeveloperInternalDefault(const QString& path) {
    const QString normalized = normalizedPathForComparison(path);
    if (normalized.isEmpty())
        return false;
    return normalized.contains("/appdata/local/openvisualdropletsorter/trainer_gui_unicode_verify") ||
           normalized.contains("/codex/opendss/0. codebase/datasets") ||
           normalized.contains("/build-opendss-internal-release/desktop_app/release/models");
}

QString sha256FileHex(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

QString registryIdToken(QString value) {
    value = value.trimmed();
    QString result;
    result.reserve(value.size());
    for (const QChar ch : value) {
        if (ch.isLetterOrNumber()) {
            result.append(ch.toLower());
        } else if (ch == '_' || ch == '-') {
            result.append(ch);
        } else if (!result.endsWith('_')) {
            result.append('_');
        }
    }
    while (result.startsWith('_'))
        result.remove(0, 1);
    while (result.endsWith('_'))
        result.chop(1);
    return result.isEmpty() ? QString("trained_model") : result;
}

QString modelFolderName(const QString& modelName) {
    QString folder;
    folder.reserve(modelName.size());
    for (const QChar ch : modelName.trimmed()) {
        if (ch.isLetterOrNumber() || ch == ' ' || ch == '_' || ch == '-') {
            folder.append(ch);
        } else if (!folder.endsWith('_')) {
            folder.append('_');
        }
    }
    while (folder.startsWith('.') || folder.startsWith(' ') || folder.startsWith('_'))
        folder.remove(0, 1);
    while (folder.endsWith('.') || folder.endsWith(' ') || folder.endsWith('_'))
        folder.chop(1);
    return folder.isEmpty() ? QString("trained_model") : folder;
}

QString modelsRootPath() {
    const QString overridePath = qEnvironmentVariable(kModelsRootOverrideEnv).trimmed();
    if (!overridePath.isEmpty())
        return QFileInfo(overridePath).absoluteFilePath();
    return defaultOpenDssModelsPath();
}

QJsonObject readJsonObjectFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = "JSON file not readable: " + path;
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = "JSON parse failed for " + path + ": " + parseError.errorString();
        return {};
    }
    return doc.object();
}

bool copyRequiredModelFile(const QString& sourcePath, const QString& destinationPath, const QString& label,
                           QString* error) {
    const QString source = absoluteCleanPath(sourcePath);
    const QString destination = absoluteCleanPath(destinationPath);
    if (!QFileInfo(source).isFile()) {
        if (error)
            *error = label + " is missing: " + source;
        return false;
    }
    if (QFileInfo::exists(destination)) {
        if (error)
            *error = label + " destination already exists: " + destination;
        return false;
    }
    if (!QFile::copy(source, destination)) {
        if (error)
            *error = "Could not copy " + label + " to " + destination;
        return false;
    }
    return true;
}

bool copyOptionalModelFile(const QString& sourcePath, const QString& destinationPath, const QString& label,
                           QString* error) {
    const QString source = absoluteCleanPath(sourcePath);
    if (source.isEmpty() || !QFileInfo(source).isFile())
        return true;
    const QString destination = absoluteCleanPath(destinationPath);
    if (QFileInfo::exists(destination)) {
        if (error)
            *error = label + " destination already exists: " + destination;
        return false;
    }
    if (!QFile::copy(source, destination)) {
        if (error)
            *error = "Could not copy " + label + " to " + destination;
        return false;
    }
    return true;
}

void appendUniqueSidecarName(QStringList* names, const QString& name) {
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty() && !trimmed.contains('/') && !trimmed.contains('\\') && !names->contains(trimmed))
        names->append(trimmed);
}

QStringList onnxExternalDataSidecarNames(const QString& modelOnnxPath, const QJsonObject& metadata) {
    QStringList names;
    const QFileInfo modelInfo(modelOnnxPath);
    const QDir modelDir(modelInfo.absolutePath());

    const QJsonArray declared = metadata.value("artifact").toObject().value("external_data_files").toArray();
    for (const auto& value : declared) {
        const QJsonObject entry = value.toObject();
        appendUniqueSidecarName(&names, entry.value("filename").toString());
    }

    appendUniqueSidecarName(&names, modelInfo.fileName() + ".data");
    QDirIterator sidecars(modelDir.absolutePath(), QStringList{"*.onnx.data"}, QDir::Files);
    while (sidecars.hasNext())
        appendUniqueSidecarName(&names, QFileInfo(sidecars.next()).fileName());

    QStringList present;
    for (const QString& name : names) {
        if (modelDir.exists(name))
            present.append(name);
    }
    return present;
}

QString firstNonEmpty(std::initializer_list<QString> values) {
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty())
            return trimmed;
    }
    return QString();
}

bool jsonArrayContainsString(const QJsonArray& values, const QString& needle) {
    for (const auto& value : values) {
        if (value.toString().compare(needle, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QString trainedValidationStatus(const QJsonObject& metadata) {
    const QJsonObject validation = metadata.value("validation_summary").toObject();
    const QJsonObject image = validation.value("image_validation").toObject();
    const QString status = image.value("status").toString().trimmed();
    QStringList parts;
    parts << (status.isEmpty() ? QString("Training completed") : QString("Training completed; image status: %1").arg(status));
    if (image.value("accuracy").isDouble())
        parts << QString("accuracy %1%").arg(image.value("accuracy").toDouble() * 100.0, 0, 'f', 1);
    if (image.value("macro_f1").isDouble())
        parts << QString("macro F1 %1").arg(image.value("macro_f1").toDouble(), 0, 'f', 3);
    const QJsonObject sequence = validation.value("sequence_validation").toObject();
    const QString sequenceStatus = sequence.value("status").toString().trimmed();
    if (!sequenceStatus.isEmpty())
        parts << QString("sequence %1").arg(sequenceStatus);
    return parts.join("; ");
}

QString packagedPathCandidate(const QString& relativePath);

QString resolvedRegistryArtifactPathForComparison(const QString& path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (QFileInfo(trimmed).isAbsolute())
        return normalizedPathForComparison(absoluteCleanPath(trimmed));
    const QString packagedPath = packagedPathCandidate(trimmed);
    if (!packagedPath.isEmpty())
        return normalizedPathForComparison(absoluteCleanPath(packagedPath));
    return normalizedPathForComparison(absoluteCleanPath(trimmed));
}

QJsonObject imageValidationSummaryFromValidatorSummary(const QJsonObject& summary, const QString& summaryPath) {
    const QJsonObject metrics = summary.value("metrics").toObject();
    QJsonObject imageValidation;
    const QString status = summary.value("status").toString().trimmed();
    imageValidation["status"] = status.isEmpty() ? QString("completed") : status;
    imageValidation["summary_path"] = QFileInfo(summaryPath).absoluteFilePath();
    imageValidation["validated_at"] = summary.value("created_at").toString(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    imageValidation["samples_total"] = summary.value("dataset").toObject().value("samples_total").toInt();
    imageValidation["samples_evaluated"] = summary.value("dataset").toObject().value("samples_evaluated").toInt();
    imageValidation["samples_failed"] = summary.value("dataset").toObject().value("samples_failed").toInt();
    imageValidation["samples_incorrect"] = metrics.value("samples_incorrect").toInt();
    if (metrics.value("accuracy").isDouble())
        imageValidation["accuracy"] = metrics.value("accuracy").toDouble();
    if (metrics.value("macro_f1").isDouble())
        imageValidation["macro_f1"] = metrics.value("macro_f1").toDouble();
    if (metrics.value("loss").isDouble())
        imageValidation["loss"] = metrics.value("loss").toDouble();
    const QJsonObject latency = summary.value("latency").toObject();
    if (latency.value("mean_ms").isDouble())
        imageValidation["mean_latency_ms"] = latency.value("mean_ms").toDouble();
    if (latency.value("max_ms").isDouble())
        imageValidation["p99_latency_ms"] = latency.value("max_ms").toDouble();
    return imageValidation;
}

bool validationSummaryHasReadableResult(const QJsonObject& summary) {
    const QString status = summary.value("status").toString().trimmed();
    if (status.isEmpty() || status.compare("not_run", Qt::CaseInsensitive) == 0 ||
        status.compare("error", Qt::CaseInsensitive) == 0 ||
        status.contains("failed", Qt::CaseInsensitive) || status.contains("canceled", Qt::CaseInsensitive) ||
        status.contains("cancelled", Qt::CaseInsensitive)) {
        return false;
    }
    const QJsonObject metrics = summary.value("metrics").toObject();
    return metrics.value("accuracy").isDouble() || metrics.value("macro_f1").isDouble();
}

QJsonObject targetPolicyFromMetadata(const QJsonObject& metadata) {
    QJsonObject targetPolicy = metadata.value("sorting_policy").toObject();
    if (!targetPolicy.isEmpty())
        return targetPolicy;

    const QJsonArray classes = metadata.value("classes").toArray();
    const QJsonObject displayLabels = metadata.value("display_labels").toObject();
    if (!jsonArrayContainsString(classes, "1"))
        return {};
    targetPolicy["target_class_id"] = "1";
    targetPolicy["target_display_label"] = displayLabels.value("1").toString("Target");
    targetPolicy["trigger_rule"] = "trigger_on_target_class";
    QJsonArray nonTargetClassIds;
    for (const QJsonValue& value : classes) {
        const QString classId = value.toString().trimmed();
        if (!classId.isEmpty() && classId != "1")
            nonTargetClassIds.append(classId);
    }
    if (!nonTargetClassIds.isEmpty())
        targetPolicy["non_target_class_ids"] = nonTargetClassIds;
    if (jsonArrayContainsString(classes, "0")) {
        targetPolicy["waste_class_id"] = "0";
        targetPolicy["waste_display_label"] = displayLabels.value("0").toString("Non-target");
    }
    return targetPolicy;
}

QJsonObject trainedModelRegistryEntry(const QString& runDir, const QString& modelOnnxPath,
                                      const QString& metadataJsonPath, const QJsonObject& metadata) {
    const QString runPath = absoluteCleanPath(runDir);
    const QString modelPath = absoluteCleanPath(modelOnnxPath);
    const QString metadataPath = absoluteCleanPath(metadataJsonPath);
    const QString modelId = firstNonEmpty({metadata.value("model_id").toString(), QFileInfo(runPath).fileName(),
                                           QFileInfo(modelPath).completeBaseName()});
    const QString createdAt = firstNonEmpty({metadata.value("created_at").toString(),
                                             QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    const QString displayName =
        firstNonEmpty({metadata.value("model_name").toString(), QString("Trained model %1").arg(createdAt)});

    QJsonObject entry;
    entry["registry_entry_id"] = "trained_" + registryIdToken(modelId);
    entry["display_name"] = displayName;
    entry["state"] = "trained";
    entry["model_status"] = "Trained";
    entry["live_use_mode"] = "normal";
    entry["selectable_for_normal_live_sorting"] = false;
    entry["model_id"] = modelId;
    entry["model_path"] = modelPath;
    entry["model_sha256"] = sha256FileHex(modelPath);
    entry["metadata_path"] = metadataPath;
    entry["metadata_schema_version"] = metadata.value("schema_version").toString("model-metadata-v1");
    entry["metadata_sha256"] = sha256FileHex(metadataPath);
    entry["metadata_status"] = "Training completed";
    entry["validation_status"] = trainedValidationStatus(metadata);
    entry["promotion_status"] = "Trained";
    entry["created_at"] = createdAt;
    entry["training_run_dir"] = runPath;
    entry["classes"] = metadata.value("classes").toArray(metadata.value("class_ids").toArray());
    entry["display_labels"] = metadata.value("display_labels").toObject();
    entry["label_schema_version"] = metadata.value("label_schema_version").toString();
    entry["target_policy"] = targetPolicyFromMetadata(metadata);
    entry["limitations"] = metadata.value("limitations").toArray(
        QJsonArray{QString("Sequence validation has not been run for this trained candidate.")});
    entry["blockers"] = QJsonArray{};
    entry["validation_evidence"] = metadata.value("validation_summary").toObject();
    return entry;
}

bool registryContainsTrainedModel(const QJsonArray& entries, const QString& entryId, const QString& modelPath,
                                  const QString& metadataPath) {
    const QString cleanModelPath = absoluteCleanPath(modelPath);
    const QString cleanMetadataPath = absoluteCleanPath(metadataPath);
    for (const auto& value : entries) {
        const QJsonObject existingEntry = value.toObject();
        const QString existingId = registryString(existingEntry, "registry_entry_id");
        const QString existingModel = absoluteCleanPath(registryString(existingEntry, "model_path"));
        const QString existingMetadata = absoluteCleanPath(registryString(existingEntry, "metadata_path"));
        if (existingId.compare(entryId, Qt::CaseInsensitive) == 0 ||
            (!existingModel.isEmpty() && existingModel.compare(cleanModelPath, Qt::CaseInsensitive) == 0) ||
            (!existingMetadata.isEmpty() && existingMetadata.compare(cleanMetadataPath, Qt::CaseInsensitive) == 0)) {
            return true;
        }
    }
    return false;
}

bool nonEmptyStringEquals(const QString& lhs, const QString& rhs) {
    return !lhs.trimmed().isEmpty() && lhs.compare(rhs, Qt::CaseInsensitive) == 0;
}

bool nonEmptyPathEquals(const QString& lhs, const QString& rhs) {
    const QString lhsPath = absoluteCleanPath(lhs);
    const QString rhsPath = absoluteCleanPath(rhs);
    return !lhsPath.isEmpty() && lhsPath.compare(rhsPath, Qt::CaseInsensitive) == 0;
}

bool suppressionMatchesTrainedEntry(const QJsonObject& suppression, const QJsonObject& entry) {
    if (nonEmptyStringEquals(registryString(suppression, "registry_entry_id"),
                             registryString(entry, "registry_entry_id"))) {
        return true;
    }
    if (nonEmptyPathEquals(registryString(suppression, "model_path"), registryString(entry, "model_path")))
        return true;
    if (nonEmptyPathEquals(registryString(suppression, "metadata_path"), registryString(entry, "metadata_path")))
        return true;
    if (nonEmptyPathEquals(registryString(suppression, "training_run_dir"), registryString(entry, "training_run_dir")))
        return true;
    return false;
}

bool registrySuppressesTrainedEntry(const QJsonObject& registry, const QJsonObject& entry) {
    const QJsonArray suppressions = registry.value(kSuppressedTrainedModelsKey).toArray();
    for (const auto& value : suppressions) {
        if (suppressionMatchesTrainedEntry(value.toObject(), entry))
            return true;
    }
    return false;
}

bool metadataLooksLikeDiscoveredTrainedModel(const QJsonObject& metadata) {
    const QString status = metadata.value("status").toString().trimmed();
    if (status.contains("transfer_start", Qt::CaseInsensitive) ||
        status.contains("untrained", Qt::CaseInsensitive) ||
        status.contains("template", Qt::CaseInsensitive) ||
        status.contains("starter", Qt::CaseInsensitive)) {
        return false;
    }
    if (status.contains("trained", Qt::CaseInsensitive) ||
        metadata.value("model_id").toString().startsWith("saved_", Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

QString defaultDisplayLabelForActivation(const QStringList& classIds, const QString& classId) {
    if (classIds == QStringList{"0", "1"}) {
        if (classId == "0")
            return "Non-target";
        if (classId == "1")
            return "Target";
    }
    if (classIds == QStringList{"0", "1", "2"}) {
        if (classId == "0")
            return "Non-target A";
        if (classId == "1")
            return "Target";
        if (classId == "2")
            return "Non-target B";
    }
    return QString();
}

QStringList classIdsForActivation(const QJsonObject& entry, const QJsonObject& metadata) {
    QStringList classIds;
    const QJsonArray metadataClasses = metadata.value("classes").toArray(metadata.value("class_ids").toArray());
    const QJsonArray entryClasses = entry.value("classes").toArray();
    const QJsonArray source = metadataClasses.isEmpty() ? entryClasses : metadataClasses;
    for (const auto& value : source) {
        const QString classId = value.toString().trimmed();
        if (!classId.isEmpty())
            classIds << classId;
    }
    return classIds;
}

QJsonObject mergedDisplayLabelsForActivation(const QJsonObject& entry, const QJsonObject& metadata) {
    QJsonObject displayLabels = entry.value("display_labels").toObject();
    const QJsonObject metadataLabels = metadata.value("display_labels").toObject();
    for (auto it = metadataLabels.constBegin(); it != metadataLabels.constEnd(); ++it)
        displayLabels[it.key()] = it.value();
    return displayLabels;
}

bool labelsReadableForActivation(const QStringList& classIds, const QJsonObject& displayLabels) {
    if (classIds.isEmpty())
        return false;
    for (const QString& classId : classIds) {
        QString displayLabel = displayLabels.value(classId).toString().trimmed();
        if (displayLabel.isEmpty())
            displayLabel = defaultDisplayLabelForActivation(classIds, classId);
        if (displayLabel.isEmpty())
            return false;
    }
    return true;
}

QJsonObject targetPolicyForActivation(const QJsonObject& entry, const QJsonObject& metadata) {
    const QJsonObject entryPolicy = entry.value("target_policy").toObject();
    return entryPolicy.isEmpty() ? targetPolicyFromMetadata(metadata) : entryPolicy;
}

bool targetPolicyReadableForActivation(const QJsonObject& targetPolicy, const QStringList& classIds) {
    const QString targetClassId = firstNonEmpty(
        {targetPolicy.value("target_class_id").toString(), targetPolicy.value("targetClassId").toString()});
    QStringList nonTargetClassIds;
    const QString legacyNonTargetClassId = firstNonEmpty({targetPolicy.value("waste_class_id").toString(),
                                                          targetPolicy.value("non_target_class_id").toString(),
                                                          targetPolicy.value("nontarget_class_id").toString()});
    if (!legacyNonTargetClassId.isEmpty())
        nonTargetClassIds << legacyNonTargetClassId;
    const QJsonArray nonTargetArray = targetPolicy.value("non_target_class_ids").toArray();
    for (const QJsonValue& value : nonTargetArray) {
        const QString classId = value.toString().trimmed();
        if (!classId.isEmpty() && !nonTargetClassIds.contains(classId))
            nonTargetClassIds << classId;
    }
    if (targetClassId.isEmpty() || nonTargetClassIds.isEmpty())
        return false;
    if (!classIds.isEmpty()) {
        if (!classIds.contains(targetClassId))
            return false;
        for (const QString& nonTargetClassId : nonTargetClassIds) {
            if (!classIds.contains(nonTargetClassId))
                return false;
        }
    }
    return true;
}

QJsonObject validationSummaryForActivation(const QJsonObject& entry, const QJsonObject& metadata) {
    const QJsonObject metadataSummary = metadata.value("validation_summary").toObject();
    if (!metadataSummary.isEmpty())
        return metadataSummary;

    const QJsonValue evidenceValue = entry.value("validation_evidence");
    if (evidenceValue.isObject())
        return evidenceValue.toObject();
    if (!evidenceValue.isString())
        return {};

    QString summaryError;
    const QString summaryPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(evidenceValue.toString()));
    return readJsonObjectFile(summaryPath, &summaryError);
}

ActiveModelReadiness blockedReadiness(const QString& missingItem, const QString& message) {
    ActiveModelReadiness readiness;
    readiness.ready = false;
    readiness.missingItem = missingItem;
    readiness.message = message;
    return readiness;
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

    // Registry paths historically used source-tree-relative
    // "app/runtime/models/..." values.  A deployed build contains the same
    // payload under <applicationDir>/models, and must not depend on either the
    // process working directory or a discoverable source checkout.
    QString modelsRelative = trimmed;
    const QString internalPrefix = "internal-release/app/runtime/";
    const QString sourcePrefix = "app/runtime/";
    if (modelsRelative.startsWith(internalPrefix, Qt::CaseInsensitive))
        modelsRelative.remove(0, internalPrefix.size());
    else if (modelsRelative.startsWith(sourcePrefix, Qt::CaseInsensitive))
        modelsRelative.remove(0, sourcePrefix.size());

    if (modelsRelative.startsWith("models/", Qt::CaseInsensitive)) {
        const QString modelsOverride = qEnvironmentVariable(kModelsRootOverrideEnv).trimmed();
        if (!modelsOverride.isEmpty()) {
            QString belowModels = modelsRelative.mid(QString("models/").size());
            const QString overridden = QDir(modelsOverride).absoluteFilePath(belowModels);
            if (QFileInfo::exists(overridden))
                return QDir::cleanPath(overridden);
        }

        const QString deployed = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(modelsRelative);
        if (QFileInfo::exists(deployed))
            return QDir::cleanPath(deployed);
    }

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
    const QString current = pathLooksLikeDeveloperInternalDefault(currentPath)
                                ? QString()
                                : (fileDialog ? existingFileOrDirectoryPath(currentPath)
                                              : existingDirectoryPath(currentPath));
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
    promoted["model_status"] = "Trained";
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
    blank["display_name"] = "Blank model";
    blank["state"] = "available";
    blank["model_status"] = "Untrained";
    blank["live_use_mode"] = "blocked";
    blank["selectable_for_normal_live_sorting"] = false;
    blank["model_id"] = "blank_squeezenet_template_seed42";
    blank["architecture_id"] = "squeezenet1_1";
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
                                      "separate training."};
    QJsonObject blocker;
    blocker["blocker"] = "Training starter only";
    blocker["evidence"] = "Bundled package asset: app/runtime/models/blank_squeezenet_template.onnx";
    blocker["owner_type"] = "Model Manager";
    blocker["required_next_action"] = "Train this bundled starter before any live sorting use.";
    blank["blockers"] = QJsonArray{blocker};
    blank["validation_evidence"] = QJsonObject{};
    blank["model_sidecars"] = QJsonArray{};
    return blank;
}

QJsonObject makePackagedPretrainedModelRegistryEntry() {
    QJsonObject backup;
    backup["registry_entry_id"] = "pre_binary_promotion_backup";
    backup["display_name"] = "Pre-trained model";
    backup["state"] = "available";
    backup["model_status"] = "Trained";
    backup["live_use_mode"] = "normal";
    backup["selectable_for_normal_live_sorting"] = false;
    backup["model_id"] = "pre_binary_promotion_backup";
    backup["architecture_id"] = "squeezenet1_1";
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

QJsonObject makePackagedModernModelRegistryEntry(const QString& architectureId, const QString& origin) {
    const bool mobile = architectureId == "mobilenet_v3_small";
    const bool blank = origin == "blank";
    const QString architecture = mobile ? QString("mobilenet_v3_small") : QString("efficientnet_b0");
    const QString registryEntryId = QString("opendss_%1_%2").arg(origin, architecture);

    // The deployed registry is the signed package manifest.  Reuse its fixed
    // hashes rather than deriving "expected" hashes from files that may
    // already have been altered on disk.
    QString manifestError;
    const QJsonObject manifest = readJsonObjectFile(packagedModelRegistryPath(), &manifestError);
    for (const QJsonValue& value : manifest.value("entries").toArray()) {
        QJsonObject packagedEntry = value.toObject();
        if (registryString(packagedEntry, "registry_entry_id").compare(registryEntryId, Qt::CaseInsensitive) == 0) {
            const QString family = mobile ? QString("MobileNetV3-Small") : QString("EfficientNet-B0");
            const QString qualifier = mobile ? QString("Faster") : QString("More Accurate");
            const QString base = QString("models/templates/%1/%2").arg(origin, architecture);
            // model-registry-v3 deliberately persists only four fields.  The
            // Add Model UI, however, needs the package metadata that was
            // intentionally removed from that compact registry.  Rehydrate
            // those runtime-only fields here so both choices have distinct,
            // readable labels and carry their actual architecture/package.
            packagedEntry["package_path"] = base;
            packagedEntry["architecture_id"] = architecture;
            packagedEntry["origin"] = origin;
            packagedEntry["user_facing_label"] = QString("%1 — %2").arg(family, qualifier);
            packagedEntry["recommended"] = mobile;
            packagedEntry["model_path"] = base + "/model.onnx";
            packagedEntry["metadata_path"] = base + "/metadata.json";
            packagedEntry.remove("model_sha256");
            packagedEntry.remove("metadata_sha256");
            packagedEntry.remove("model_sidecars");
            return packagedEntry;
        }
    }

    const QString family = mobile ? QString("MobileNetV3-Small") : QString("EfficientNet-B0");
    const QString qualifier = mobile ? QString("Faster") : QString("More Accurate");
    const QString base = QString("models/templates/%1/%2").arg(origin, architecture);
    const QString modelPath = base + "/model.onnx";
    const QString metadataPath = base + "/metadata.json";
    const QString sidecarPath = base + "/model.onnx.data";
    const QString absoluteModel = resolvePackagedPathFromRegistryPath(modelPath);
    const QString absoluteMetadata = resolvePackagedPathFromRegistryPath(metadataPath);
    const QString absoluteSidecar = resolvePackagedPathFromRegistryPath(sidecarPath);
    QJsonObject entry;
    entry["registry_entry_id"] = registryEntryId;
    entry["model_id"] = entry["registry_entry_id"];
    entry["user_facing_label"] = QString("%1 — %2").arg(family, qualifier);
    entry["display_name"] = QString("%1 %2").arg(blank ? QString("Blank") : QString("Pre-trained"),
                                                   entry["user_facing_label"].toString());
    entry["architecture_id"] = architecture;
    entry["origin"] = origin;
    entry["recommended"] = mobile;
    entry["legacy"] = false;
    entry["state"] = "available";
    entry["model_status"] = blank ? "Untrained" : "Trained";
    entry["live_use_mode"] = blank ? "blocked" : "normal";
    entry["selectable_for_normal_live_sorting"] = false;
    entry["active"] = false;
    entry["package_path"] = base;
    entry["model_path"] = modelPath;
    entry["metadata_path"] = metadataPath;
    entry["metadata_schema_version"] = "model-metadata-v2";
    entry["metadata_status"] = blank ? "ImageNet-start template" : "Verified production candidate";
    entry["validation_status"] = blank ? "Not droplet-trained" : "Two-fold cross-validation evidence accepted";
    entry["promotion_status"] = blank ? "Starter only" : "Available";
    entry["classes"] = QJsonArray{"0", "1", "2"};
    entry["display_labels"] = QJsonObject{{"0", "Empty"}, {"1", "Single"}, {"2", "MoreThanOne"}};
    entry["label_schema_version"] = "opendss-droplet-3class-v1";
    entry["target_policy"] = QJsonObject{{"target_class_id", "1"}, {"target_display_label", "Single"},
                                           {"waste_class_id", "0"}, {"waste_display_label", "Empty"},
                                           {"trigger_rule", "trigger_on_target_class"}};
    entry["limitations"] = blank
                                ? QJsonArray{"ImageNet-start three-class template; train and validate before live sorting."}
                                : QJsonArray{"All-data deployment fit; performance evidence is the accepted cross-validation report."};
    entry["blockers"] = blank ? QJsonArray{QJsonObject{{"blocker", "Training required"},
                                                         {"required_next_action", "Train and validate this template."}}}
                               : QJsonArray{};
    entry["provenance_reference"] = blank ? QString("official torchvision ImageNet weights")
                                           : QString("docs/worker-reports/production-model-training-2026-07-19/train-final-all-data-models.md");
    return entry;
}

QJsonObject entryByRegistryId(const QJsonArray& entries, const QString& registryEntryId) {
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").compare(registryEntryId, Qt::CaseInsensitive) == 0)
            return entry;
    }
    return {};
}

QJsonObject freshSeedBlankEntry(QJsonObject entry) {
    if (entry.isEmpty())
        entry = makePackagedModernModelRegistryEntry("mobilenet_v3_small", "blank");
    entry["state"] = "available";
    entry["selectable_for_normal_live_sorting"] = false;
    entry["promotion_status"] = "Starter only";
    return entry;
}

QJsonObject freshSeedPretrainedEntry(QJsonObject entry) {
    if (entry.isEmpty())
        entry = makePackagedModernModelRegistryEntry("mobilenet_v3_small", "pretrained");
    entry["state"] = "available";
    entry["selectable_for_normal_live_sorting"] = false;
    entry["promotion_status"] = "Available";
    return entry;
}

QJsonObject freshSeedRegistryFromPackaged(QJsonObject registry) {
    const QJsonArray packagedEntries = registry.value("entries").toArray();
    QJsonArray entries;
    entries.append(freshSeedBlankEntry(entryByRegistryId(packagedEntries, "opendss_blank_mobilenet_v3_small_3class")));
    entries.append(freshSeedPretrainedEntry(entryByRegistryId(packagedEntries, "opendss_pretrained_mobilenet_v3_small_3class")));
    registry["entries"] = entries;
    return registry;
}

} // namespace

QString defaultOpenDssRootPath() {
    return QDir(defaultDocumentsPath()).filePath("OpenDropletSortingSuite");
}

QString defaultOpenDssModelsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("models");
}

QString defaultOpenDssCollectionsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("collections");
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

QString defaultOpenDssReportsPath() {
    return QDir(defaultOpenDssRootPath()).filePath("reports");
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

bool isDeveloperInternalDefaultPath(const QString& path) {
    return pathLooksLikeDeveloperInternalDefault(path);
}

QJsonObject packagedPromotedModelRegistryEntry() {
    return makePackagedPromotedModelRegistryEntry();
}

QJsonObject packagedBlankModelRegistryEntry() {
    return makePackagedModernModelRegistryEntry("mobilenet_v3_small", "blank");
}

QJsonObject packagedPretrainedModelRegistryEntry() {
    return makePackagedModernModelRegistryEntry("mobilenet_v3_small", "pretrained");
}

QJsonObject packagedModernModelRegistryEntry(const QString& architectureId, const QString& origin) {
    return makePackagedModernModelRegistryEntry(architectureId, origin);
}

QJsonArray packagedModernModelRegistryEntries(const QString& origin) {
    return QJsonArray{makePackagedModernModelRegistryEntry("mobilenet_v3_small", origin),
                      makePackagedModernModelRegistryEntry("efficientnet_b0", origin)};
}

QString packagedModelEntryAvailabilityError(const QJsonObject& entry) {
    const QString configuredModel = registryString(entry, "model_path");
    const QString modelPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredModel));
    if (!QFileInfo(modelPath).isFile())
        return QString("Packaged model asset is missing: %1")
            .arg(QDir::toNativeSeparators(modelPath.isEmpty() ? configuredModel : modelPath));
    const QString expectedModelHash = registryString(entry, "model_sha256").trimmed();
    if (!expectedModelHash.isEmpty() &&
        sha256FileHex(modelPath).compare(expectedModelHash, Qt::CaseInsensitive) != 0) {
        return "Packaged model asset failed its SHA-256 integrity check.";
    }

    const QString configuredMetadata = registryString(entry, "metadata_path");
    const QString metadataPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredMetadata));
    if (!QFileInfo(metadataPath).isFile())
        return QString("Packaged model metadata is missing: %1")
            .arg(QDir::toNativeSeparators(metadataPath.isEmpty() ? configuredMetadata : metadataPath));
    const QString expectedMetadataHash = registryString(entry, "metadata_sha256").trimmed();
    if (!expectedMetadataHash.isEmpty() &&
        sha256FileHex(metadataPath).compare(expectedMetadataHash, Qt::CaseInsensitive) != 0) {
        return "Packaged model metadata failed its SHA-256 integrity check.";
    }

    for (const QJsonValue& value : entry.value("model_sidecars").toArray()) {
        const QJsonObject sidecar = value.toObject();
        if (!sidecar.value("required").toBool(true))
            continue;
        const QString configuredSidecar = sidecar.value("path").toString();
        const QString sidecarPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredSidecar));
        if (!QFileInfo(sidecarPath).isFile())
            return QString("Packaged model sidecar is missing: %1")
                .arg(QDir::toNativeSeparators(sidecarPath.isEmpty() ? configuredSidecar : sidecarPath));
        const QString expectedSidecarHash = sidecar.value("sha256").toString().trimmed();
        if (!expectedSidecarHash.isEmpty() &&
            sha256FileHex(sidecarPath).compare(expectedSidecarHash, Qt::CaseInsensitive) != 0) {
            return "Packaged model sidecar failed its SHA-256 integrity check.";
        }
    }
    return {};
}

bool copyFileIfMissing(const QString& sourcePath, const QString& destinationPath) {
    if (!QFileInfo(sourcePath).isFile())
        return false;
    const QString normalizedSource = normalizedPathForComparison(absoluteCleanPath(sourcePath));
    const QString normalizedDestination = normalizedPathForComparison(absoluteCleanPath(destinationPath));
    if (normalizedSource == normalizedDestination)
        return true;
    if (QFileInfo(destinationPath).isFile()) {
        if (sha256FileHex(sourcePath) == sha256FileHex(destinationPath))
            return true;
        if (!QFile::remove(destinationPath))
            return false;
    }
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
    QJsonObject simpleRegistry = registry;
    QJsonArray simpleEntries;
    for (const QJsonValue& value : registry.value("entries").toArray()) {
        const QJsonObject oldEntry = value.toObject();
        QString packagePath = oldEntry.value("package_path").toString().trimmed();
        if (packagePath.isEmpty()) {
            QString metadataPath = oldEntry.value("metadata_path").toString().trimmed();
            if (!metadataPath.isEmpty())
                packagePath = QFileInfo(metadataPath).path();
            else {
                const QString modelPath = oldEntry.value("model_path").toString().trimmed();
                if (!modelPath.isEmpty()) packagePath = QFileInfo(modelPath).path();
            }
        }
        if (packagePath.isEmpty())
            continue;
        QJsonObject entry;
        entry["registry_entry_id"] = oldEntry.value("registry_entry_id").toString();
        entry["display_name"] = oldEntry.value("display_name").toString(entry.value("registry_entry_id").toString());
        entry["package_path"] = QDir::cleanPath(packagePath);
        entry["active"] = oldEntry.value("active").toBool(oldEntry.value("selectable_for_normal_live_sorting").toBool(false));
        simpleEntries.append(entry);
    }
    simpleRegistry["schema_version"] = "model-registry-v3-simple";
    simpleRegistry["entries"] = simpleEntries;
    simpleRegistry.remove("package_options");
    if (QFileInfo(targetPath).isFile() && registry.value("schema_version").toString() != "model-registry-v3-simple") {
        const QStringList existingBackups = targetInfo.dir().entryList(
            {"model_registry.before_simple_migration_*.json"}, QDir::Files, QDir::Name);
        if (existingBackups.isEmpty()) {
            const QString backup = targetInfo.dir().filePath(
                QString("model_registry.before_simple_migration_%1.json")
                    .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmsszzz")));
            QFile::copy(targetPath, backup);
        }
    }
    return desktop_app::writeJsonObjectAtomically(targetPath, simpleRegistry, error);
}

bool repairRegistryFromDiscoveredTrainedModels(const QString& registryFilePath, QJsonObject* registry,
                                               QStringList* repairedEntryIds, QString* error) {
    if (repairedEntryIds)
        repairedEntryIds->clear();
    if (error)
        error->clear();
    if (!registry)
        return false;

    QJsonArray entries = registry->value("entries").toArray();
    const QDir modelsDir(QFileInfo(registryFilePath).absolutePath());
    if (!modelsDir.exists())
        return true;

    QStringList addedIds;
    const QFileInfoList folders = modelsDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& folderInfo : folders) {
        const QDir folder(folderInfo.absoluteFilePath());
        const QString modelPath = folder.filePath("model.onnx");
        const QString metadataPath = folder.filePath("metadata.json");
        if (!QFileInfo(modelPath).isFile() || !QFileInfo(metadataPath).isFile())
            continue;

        QString metadataError;
        const QJsonObject metadata = readJsonObjectFile(metadataPath, &metadataError);
        if (metadata.isEmpty())
            continue;
        if (!metadataLooksLikeDiscoveredTrainedModel(metadata))
            continue;

        const QJsonObject entry = trainedModelRegistryEntry(folderInfo.absoluteFilePath(), modelPath, metadataPath, metadata);
        const QString entryId = registryString(entry, "registry_entry_id");
        if (registrySuppressesTrainedEntry(*registry, entry))
            continue;
        if (registryContainsTrainedModel(entries, entryId, modelPath, metadataPath))
            continue;

        entries.append(entry);
        addedIds << entryId;
    }

    if (addedIds.isEmpty())
        return true;

    (*registry)["entries"] = entries;
    (*registry)["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!writeRegistryFile(registryFilePath, *registry, error))
        return false;
    if (repairedEntryIds)
        *repairedEntryIds = addedIds;
    return true;
}

bool repairRegistryBeforeWrite(const QString& registryFilePath, QJsonObject* registry, QString* error) {
    QStringList repairedEntryIds;
    return repairRegistryFromDiscoveredTrainedModels(registryFilePath, registry, &repairedEntryIds, error);
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
        return writeRegistryFile(targetPath, freshSeedRegistryFromPackaged(doc.object()), error);
    }

    return writeRegistryFile(targetPath, temporaryStaticModelRegistry(), error);
}

bool reconcilePackagedCopies(const QString& registryPath, const QString& packagedPath, QJsonObject* registry,
                             QStringList* repairedIds, QString* error) {
    if (!registry || !QFileInfo(packagedPath).isFile())
        return true;
    QFile packagedFile(packagedPath);
    if (!packagedFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return true;
    const QJsonDocument packagedDoc = QJsonDocument::fromJson(packagedFile.readAll());
    if (!packagedDoc.isObject())
        return true;
    QHash<QString, QJsonObject> options;
    for (const QJsonValue& value : packagedDoc.object().value("entries").toArray()) {
        const QJsonObject option = value.toObject();
        options.insert(option.value("registry_entry_id").toString(), option);
    }
    QJsonArray entries = registry->value("entries").toArray();
    bool changed = false;
    const QStringList trustedKeys = {"architecture_id", "classes", "display_labels", "label_schema_version",
                                     "metadata_path", "metadata_schema_version", "metadata_sha256", "metadata_status",
                                     "model_id", "model_path", "model_sha256", "model_sidecars", "model_status",
                                     "origin", "recommended", "user_facing_label"};
    for (int index = 0; index < entries.size(); ++index) {
        QJsonObject entry = entries.at(index).toObject();
        const QString sourceId = entry.value("source_registry_entry_id").toString();
        if (!options.contains(sourceId))
            continue;
        const QJsonObject option = options.value(sourceId);
        bool entryChanged = false;
        for (const QString& key : trustedKeys) {
            if (option.contains(key) && entry.value(key) != option.value(key)) {
                entry[key] = option.value(key);
                entryChanged = true;
            }
        }
        if (entryChanged) {
            entries[index] = entry;
            changed = true;
            if (repairedIds)
                repairedIds->append(entry.value("registry_entry_id").toString());
        }
    }
    if (!changed)
        return true;
    (*registry)["entries"] = entries;
    (*registry)["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return writeRegistryFile(registryPath, *registry, error);
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
    QJsonObject registry;
    registry["schema_version"] = "model-registry-v1";
    registry["registry_id"] = "temporary-static-fallback";
    registry["source"] = "temporary_static_fallback_missing_or_invalid_file";
    return freshSeedRegistryFromPackaged(registry);
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
    const QString schemaVersion = registry.value("schema_version").toString();
    if ((schemaVersion != "model-registry-v1" && schemaVersion != "model-registry-v2" &&
         schemaVersion != "model-registry-v3-simple") ||
        !registry.value("entries").isArray()) {
        if (loadWarning)
            *loadWarning = "Model registry schema missing/unsupported; using temporary static fallback.";
        return temporaryStaticModelRegistry();
    }
    QStringList repairedEntryIds;
    QString repairError;
    if (schemaVersion != "model-registry-v3-simple") {
        if (!writeRegistryFile(path, registry, &repairError)) {
            if (loadWarning) *loadWarning = repairError;
            return registry;
        }
        QFile migrated(path);
        if (migrated.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument migratedDoc = QJsonDocument::fromJson(migrated.readAll());
            if (migratedDoc.isObject()) registry = migratedDoc.object();
        }
    }
    if (schemaVersion != "model-registry-v3-simple" &&
        !reconcilePackagedCopies(path, packagedPath, &registry, &repairedEntryIds, &repairError)) {
        if (loadWarning)
            *loadWarning = repairError.isEmpty() ? "Model registry package reconciliation failed." : repairError;
        return registry;
    }
    if (!repairRegistryFromDiscoveredTrainedModels(path, &registry, &repairedEntryIds, &repairError)) {
        if (loadWarning)
            *loadWarning = repairError.isEmpty() ? "Model registry trained-model recovery failed." : repairError;
        return registry;
    }
    if (loadWarning)
        *loadWarning = repairedEntryIds.isEmpty()
                           ? QString()
                           : QString("Model registry recovered %1 trained model folder(s): %2")
                                 .arg(repairedEntryIds.size())
                                 .arg(repairedEntryIds.join(", "));
    return registry;
}

QJsonArray readModelRegistryEntriesFromPath(const QString& registryFilePath, QString* warning) {
    if (warning)
        warning->clear();
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (warning)
            *warning = "Registry file not readable: " + registryFilePath;
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (warning)
            *warning = "Registry parse failed: " + parseError.errorString();
        return {};
    }
    QJsonObject registry = doc.object();
    QStringList repairedEntryIds;
    QString repairError;
    if (!repairRegistryFromDiscoveredTrainedModels(registryFilePath, &registry, &repairedEntryIds, &repairError) &&
        warning) {
        *warning = repairError.isEmpty() ? "Model registry trained-model recovery failed." : repairError;
    }
    return registry.value("entries").toArray();
}

QString registryString(const QJsonObject& entry, const QString& key) {
    const QString direct = entry.value(key).toString();
    if (!direct.isEmpty())
        return direct;
    const QString packageFolder = entry.value("package_path").toString().trimmed();
    if (packageFolder.isEmpty())
        return {};
    if (key == "model_path")
        return QDir(packageFolder).filePath("model.onnx");
    if (key == "metadata_path")
        return QDir(packageFolder).filePath("metadata.json");
    if (key == "checkpoint_path")
        return QDir(packageFolder).filePath("checkpoint.pth");
    return {};
}

QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key) {
    return entry.value(objectKey).toObject().value(key).toString();
}

bool registerTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                                   const QString& modelOnnxPath, const QString& metadataJsonPath,
                                   QString* registeredEntryId, QString* error) {
    if (registeredEntryId)
        registeredEntryId->clear();
    if (error)
        error->clear();

    const QString path = registryFilePath.trimmed();
    const QString runPath = absoluteCleanPath(runDir);
    const QString modelPath = absoluteCleanPath(modelOnnxPath);
    const QString metadataPath = absoluteCleanPath(metadataJsonPath);
    if (path.isEmpty()) {
        if (error)
            *error = "No model registry file path is available.";
        return false;
    }
    if (!QFileInfo(modelPath).isFile()) {
        if (error)
            *error = "Trained ONNX model is missing: " + modelPath;
        return false;
    }
    if (!QFileInfo(metadataPath).isFile()) {
        if (error)
            *error = "Trained model metadata is missing: " + metadataPath;
        return false;
    }

    QString metadataError;
    const QJsonObject metadata = readJsonObjectFile(metadataPath, &metadataError);
    if (metadata.isEmpty()) {
        if (error)
            *error = metadataError.isEmpty() ? "Trained model metadata is empty." : metadataError;
        return false;
    }

    QJsonObject registry;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonParseError parseError;
        const QByteArray registryBytes = existing.readAll();
        existing.close();
        const QJsonDocument doc = QJsonDocument::fromJson(registryBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error)
                *error = "Model registry parse failed: " + parseError.errorString();
            return false;
        }
        registry = doc.object();
    }
    if (registry.value("schema_version").toString().isEmpty())
        registry["schema_version"] = "model-registry-v1";
    if (!repairRegistryBeforeWrite(path, &registry, error))
        return false;

    QJsonArray entries = registry.value("entries").toArray();
    QJsonObject entry = trainedModelRegistryEntry(runPath, modelPath, metadataPath, metadata);
    const QString entryId = registryString(entry, "registry_entry_id");

    int existingIndex = -1;
    for (int i = 0; i < entries.size(); ++i) {
        const QJsonObject existingEntry = entries.at(i).toObject();
        const QString existingId = registryString(existingEntry, "registry_entry_id");
        const QString existingModel = absoluteCleanPath(registryString(existingEntry, "model_path"));
        const QString existingMetadata = absoluteCleanPath(registryString(existingEntry, "metadata_path"));
        if (existingId.compare(entryId, Qt::CaseInsensitive) == 0 ||
            (!existingModel.isEmpty() && existingModel.compare(modelPath, Qt::CaseInsensitive) == 0) ||
            (!existingMetadata.isEmpty() && existingMetadata.compare(metadataPath, Qt::CaseInsensitive) == 0)) {
            existingIndex = i;
            if (!registryString(existingEntry, "display_name").trimmed().isEmpty())
                entry["display_name"] = registryString(existingEntry, "display_name");
            break;
        }
    }

    if (existingIndex >= 0) {
        entries[existingIndex] = entry;
    } else {
        entries.append(entry);
    }
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!writeRegistryFile(path, registry, error))
        return false;
    if (registeredEntryId)
        *registeredEntryId = entryId;
    return true;
}

bool saveTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                               const QString& modelOnnxPath, const QString& metadataJsonPath,
                               const QString& metricsCsvPath, const QString& trainingConfigJsonPath,
                               const QString& metricsJsonPath, const QString& classMetricsCsvPath,
                               const QString& confusionMatrixCsvPath, const QString& modelName,
                               QString* registeredEntryId, QString* error) {
    return saveTrainedModelArtifacts(
        registryFilePath, runDir, modelOnnxPath, metadataJsonPath, metricsCsvPath,
        trainingConfigJsonPath, metricsJsonPath, classMetricsCsvPath, confusionMatrixCsvPath,
        modelName, modelsRootPath(), nullptr, registeredEntryId, error);
}

bool saveTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                               const QString& modelOnnxPath, const QString& metadataJsonPath,
                               const QString& metricsCsvPath, const QString& trainingConfigJsonPath,
                               const QString& metricsJsonPath, const QString& classMetricsCsvPath,
                               const QString& confusionMatrixCsvPath, const QString& modelName,
                               const QString& destinationRoot, QString* savedPackagePath,
                               QString* registeredEntryId, QString* error,
                               const QString& replaceRegistryEntryId) {
    if (savedPackagePath)
        savedPackagePath->clear();
    if (registeredEntryId)
        registeredEntryId->clear();
    if (error)
        error->clear();

    const QString displayName = modelName.trimmed();
    if (displayName.isEmpty()) {
        if (error)
            *error = "Model name cannot be empty.";
        return false;
    }

    QString sourceMetadataError;
    const QJsonObject sourceMetadata = readJsonObjectFile(absoluteCleanPath(metadataJsonPath), &sourceMetadataError);
    if (sourceMetadata.isEmpty()) {
        if (error)
            *error = sourceMetadataError.isEmpty() ? "Trained model metadata is empty." : sourceMetadataError;
        return false;
    }

    const QString rootPath = absoluteCleanPath(destinationRoot);
    if (rootPath.isEmpty()) {
        if (error)
            *error = "No model destination folder is available.";
        return false;
    }
    if (pathTraversesReparsePoint(rootPath)) {
        if (error)
            *error = "The model destination root cannot be a junction or symbolic link.";
        return false;
    }
    QDir root(rootPath);
    if (!root.exists() && !QDir().mkpath(rootPath)) {
        if (error)
            *error = "Could not create model workspace folder: " + rootPath;
        return false;
    }
    const QString finalDestinationDirPath = root.filePath(modelFolderName(displayName));
    if (QFileInfo::exists(finalDestinationDirPath)) {
        if (error)
            *error = "A model folder already exists for this name: " + finalDestinationDirPath;
        return false;
    }
    const QString destinationDirPath = root.filePath(
        "." + QFileInfo(finalDestinationDirPath).fileName() + ".staging-" +
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!root.mkpath(QFileInfo(destinationDirPath).fileName())) {
        if (error)
            *error = "Could not create model staging folder: " + destinationDirPath;
        return false;
    }

    bool createdDestination = true;
    QString cleanupPath = destinationDirPath;
    auto cleanupOnFailure = [&]() {
        if (createdDestination) {
            QDir destination(cleanupPath);
            destination.removeRecursively();
            createdDestination = false;
        }
    };

    const QString promotedModelPath = QDir(destinationDirPath).filePath("model.onnx");
    const QString promotedMetadataPath = QDir(destinationDirPath).filePath("metadata.json");
    const QString promotedCheckpointPath = QDir(destinationDirPath).filePath("checkpoint.pth");
    const QString promotedMetricsPath = QDir(destinationDirPath).filePath("metrics.csv");
    const QString promotedMetricsJsonPath = QDir(destinationDirPath).filePath("metrics.json");
    const QString promotedClassMetricsPath = QDir(destinationDirPath).filePath("class_metrics.csv");
    const QString promotedConfusionMatrixPath = QDir(destinationDirPath).filePath("confusion_matrix.csv");
    const QString promotedConfigPath = QDir(destinationDirPath).filePath("training_config.json");

    const QString sourceCheckpointPath = QDir(absoluteCleanPath(runDir)).filePath("checkpoint.pth");
    if (!copyRequiredModelFile(modelOnnxPath, promotedModelPath, "Trained ONNX model", error) ||
        !copyRequiredModelFile(sourceCheckpointPath, promotedCheckpointPath, "Trained checkpoint", error) ||
        !copyRequiredModelFile(metadataJsonPath, promotedMetadataPath, "Trained model metadata", error)) {
        cleanupOnFailure();
        return false;
    }

    const QDir sourceModelDir(QFileInfo(absoluteCleanPath(modelOnnxPath)).absolutePath());
    const QDir destinationDir(destinationDirPath);
    const QStringList sidecarNames = onnxExternalDataSidecarNames(absoluteCleanPath(modelOnnxPath), sourceMetadata);
    for (const QString& sidecarName : sidecarNames) {
        if (!copyRequiredModelFile(sourceModelDir.filePath(sidecarName), destinationDir.filePath(sidecarName),
                                   "ONNX external data sidecar " + sidecarName, error)) {
            cleanupOnFailure();
            return false;
        }
    }

    if (!copyOptionalModelFile(metricsCsvPath, promotedMetricsPath, "Training metrics CSV", error) ||
        !copyOptionalModelFile(metricsJsonPath, promotedMetricsJsonPath, "Training metrics JSON", error) ||
        !copyOptionalModelFile(classMetricsCsvPath, promotedClassMetricsPath, "Class metrics CSV", error) ||
        !copyOptionalModelFile(confusionMatrixCsvPath, promotedConfusionMatrixPath, "Confusion matrix CSV", error) ||
        !copyOptionalModelFile(trainingConfigJsonPath, promotedConfigPath, "Training configuration", error)) {
        cleanupOnFailure();
        return false;
    }

    QString metadataError;
    QJsonObject metadata = readJsonObjectFile(promotedMetadataPath, &metadataError);
    if (metadata.isEmpty()) {
        if (error)
            *error = metadataError.isEmpty() ? "Trained model metadata is empty." : metadataError;
        cleanupOnFailure();
        return false;
    }

    QString replacementModelId;
    if (!replaceRegistryEntryId.trimmed().isEmpty()) {
        QJsonObject registry;
        if (!readRegistryForPackageMutation(registryFilePath, &registry, error)) {
            cleanupOnFailure();
            return false;
        }
        for (const QJsonValue& value : registry.value("entries").toArray()) {
            const QJsonObject entry = value.toObject();
            if (registryString(entry, "registry_entry_id").trimmed().compare(
                    replaceRegistryEntryId.trimmed(), Qt::CaseInsensitive) != 0)
                continue;
            if (registryString(entry, "display_name").trimmed().compare(
                    displayName, Qt::CaseInsensitive) != 0) {
                if (error)
                    *error = "The selected Library identity Name no longer matches.";
                cleanupOnFailure();
                return false;
            }
            const QJsonObject identityMetadata =
                readJsonObjectFile(QDir(inspectModelPackage(entry).packagePath)
                                       .filePath("metadata.json"), error);
            replacementModelId =
                identityMetadata.value("model_id").toString().trimmed();
            break;
        }
        if (replacementModelId.isEmpty()) {
            if (error && error->isEmpty())
                *error = "The selected Library identity is unavailable.";
            cleanupOnFailure();
            return false;
        }
    }

    QJsonObject artifact = metadata.value("artifact").toObject();
    artifact["onnx_file"] = "model.onnx";
    artifact["checkpoint_file"] = "checkpoint.pth";
    artifact["onnx_sha256"] = sha256FileHex(promotedModelPath);
    artifact["checkpoint_sha256"] = sha256FileHex(promotedCheckpointPath);
    artifact["format"] = "onnx";
    QJsonArray externalDataFiles;
    for (const QString& sidecarName : sidecarNames) {
        const QString sidecarPath = destinationDir.filePath(sidecarName);
        const QFileInfo sidecarInfo(sidecarPath);
        externalDataFiles.append(QJsonObject{{"filename", sidecarName},
                                             {"sha256", sha256FileHex(sidecarPath)},
                                             {"byte_size", static_cast<double>(sidecarInfo.size())},
                                             {"required", true}});
    }
    artifact["external_data_files"] = externalDataFiles;
    metadata["artifact"] = artifact;
    metadata["model_name"] = displayName;
    metadata["model_id"] = replacementModelId.isEmpty()
        ? "saved_" + registryIdToken(displayName) : replacementModelId;
    metadata["status"] = "trained";
    metadata["training_run_dir"] = absoluteCleanPath(runDir);
    metadata["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QString metadataWriteError;
    if (!desktop_app::writeJsonObjectAtomically(promotedMetadataPath, metadata, &metadataWriteError)) {
        if (error)
            *error = metadataWriteError;
        cleanupOnFailure();
        return false;
    }

    if (QFileInfo::exists(finalDestinationDirPath)
        || !QDir().rename(destinationDirPath, finalDestinationDirPath)) {
        if (error)
            *error = "Could not atomically publish the complete model package at: "
                + finalDestinationDirPath;
        cleanupOnFailure();
        return false;
    }
    cleanupPath = finalDestinationDirPath;
    const QString finalModelPath =
        QDir(finalDestinationDirPath).filePath("model.onnx");
    const QString finalMetadataPath =
        QDir(finalDestinationDirPath).filePath("metadata.json");

    QString entryId;
    QString registrationError;
    if (!registerTrainedModelArtifacts(registryFilePath, runDir, finalModelPath, finalMetadataPath, &entryId,
                                       &registrationError)) {
        if (error)
            *error = registrationError;
        cleanupOnFailure();
        return false;
    }

    createdDestination = false;
    if (savedPackagePath)
        *savedPackagePath = QFileInfo(finalDestinationDirPath).absoluteFilePath();
    if (registeredEntryId)
        *registeredEntryId = entryId;
    return true;
}

ModelPackageInspection inspectModelPackage(const QJsonObject& entry) {
    ModelPackageInspection result;
    QString packagePath = registryString(entry, "package_path").trimmed();
    if (packagePath.isEmpty()) {
        const QString metadataConfigured = registryString(entry, "metadata_path").trimmed();
        if (!metadataConfigured.isEmpty())
            packagePath = QFileInfo(resolvePackagedPathFromRegistryPath(metadataConfigured)).absolutePath();
    }
    result.packagePath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(packagePath));
    result.metadataPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(
        registryString(entry, "metadata_path").isEmpty() ? QDir(packagePath).filePath("metadata.json")
                                                         : registryString(entry, "metadata_path")));
    if (!QFileInfo(result.metadataPath).isFile()) {
        result.status = "Invalid";
        result.message = "metadata.json is missing.";
        return result;
    }
    QString metadataError;
    const QJsonObject metadata = readJsonObjectFile(result.metadataPath, &metadataError);
    if (metadata.isEmpty()) {
        result.status = "Invalid";
        result.message = metadataError.isEmpty() ? "metadata.json is malformed." : metadataError;
        return result;
    }
    const QJsonObject architecture = metadata.value("architecture").toObject();
    result.architectureId = architecture.value("id").toString(metadata.value("architecture_id").toString());
    result.classCount = architecture.value("num_classes").toInt(metadata.value("classes").toArray().size());
    const QJsonObject artifact = metadata.value("artifact").toObject();
    const QString onnxName = artifact.value("onnx_file").toString("model.onnx");
    const QString checkpointName = artifact.value("checkpoint_file").toString("checkpoint.pth");
    const QDir packageDir(QFileInfo(result.metadataPath).absolutePath());
    result.onnxPath = packageDir.filePath(onnxName);
    result.checkpointPath = packageDir.filePath(checkpointName);
    const bool hasOnnx = QFileInfo(result.onnxPath).isFile() && QFileInfo(result.onnxPath).size() > 0;
    const bool hasCheckpoint = QFileInfo(result.checkpointPath).isFile() && QFileInfo(result.checkpointPath).size() > 0;
    const bool blank = metadata.value("status").toString().compare("trained", Qt::CaseInsensitive) != 0 ||
                       metadata.value("origin").toString().compare("blank", Qt::CaseInsensitive) == 0;
    if (blank) {
        result.status = "Blank starter";
        result.message = "Ready to train from ImageNet weights.";
        result.canTrain = true;
        return result;
    }
    result.canTrain = hasCheckpoint;
    result.canActivate = hasOnnx;
    if (hasCheckpoint && hasOnnx) {
        result.status = "Ready";
        result.message = "Ready for training, testing, and activation.";
    } else if (hasOnnx) {
        result.status = "Inference only";
        result.message = "The training checkpoint is missing; this model can still be tested and activated.";
    } else if (hasCheckpoint) {
        result.status = "Training only";
        result.message = "The ONNX model is missing; continue training or export before activation.";
    } else {
        result.status = "Invalid";
        result.message = "Both checkpoint.pth and model.onnx are missing.";
    }
    return result;
}

ActiveModelReadiness evaluateActiveModelReadiness(const QJsonObject& entry) {
    const ModelPackageInspection package = inspectModelPackage(entry);
    if (!package.canActivate)
        return blockedReadiness(package.status, package.message);
#if 0
    const QString configuredModelPath = registryString(entry, "model_path").trimmed();
    const QString modelPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredModelPath));
    if (!QFileInfo(modelPath).isFile()) {
        return blockedReadiness(
            "model.onnx",
            QString("This model cannot become active because the ONNX model file is missing.\n\nExpected file:\n%1")
                .arg(QDir::toNativeSeparators(modelPath.isEmpty() ? configuredModelPath : modelPath)));
    }
    const QString expectedModelHash = registryString(entry, "model_sha256").trimmed();
    if (!expectedModelHash.isEmpty() && sha256FileHex(modelPath).compare(expectedModelHash, Qt::CaseInsensitive) != 0) {
        return blockedReadiness("model.onnx hash", "This model cannot become active because the ONNX graph hash does not match its package metadata.");
    }

    for (const auto& value : entry.value("model_sidecars").toArray()) {
        const QJsonObject sidecar = value.toObject();
        if (!sidecar.value("required").toBool(true))
            continue;
        const QString configuredSidecar = sidecar.value("path").toString();
        const QString sidecarPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredSidecar));
        if (!QFileInfo(sidecarPath).isFile())
            return blockedReadiness("model.onnx.data", "This model cannot become active because its required ONNX external-data sidecar is missing.");
        const QString expectedSidecarHash = sidecar.value("sha256").toString().trimmed();
        if (!expectedSidecarHash.isEmpty() && sha256FileHex(sidecarPath).compare(expectedSidecarHash, Qt::CaseInsensitive) != 0)
            return blockedReadiness("model.onnx.data hash", "This model cannot become active because its ONNX external-data sidecar hash does not match.");
    }

    const QString configuredMetadataPath = registryString(entry, "metadata_path").trimmed();
    const QString metadataPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredMetadataPath));
    if (!QFileInfo(metadataPath).isFile()) {
        return blockedReadiness(
            "metadata.json",
            QString("This model cannot become active because the metadata file is missing.\n\nExpected file:\n%1")
                .arg(QDir::toNativeSeparators(metadataPath.isEmpty() ? configuredMetadataPath : metadataPath)));
    }
    const QString expectedMetadataHash = registryString(entry, "metadata_sha256").trimmed();
    if (!expectedMetadataHash.isEmpty() && sha256FileHex(metadataPath).compare(expectedMetadataHash, Qt::CaseInsensitive) != 0)
        return blockedReadiness("metadata.json hash", "This model cannot become active because its metadata hash does not match.");

    QString metadataError;
    const QJsonObject metadata = readJsonObjectFile(metadataPath, &metadataError);
    if (metadata.isEmpty()) {
        const QString detail = metadataError.isEmpty() ? "The metadata file could not be read as JSON."
                                                       : metadataError;
        return blockedReadiness(
            "metadata.json",
            QString("This model cannot become active because the metadata file could not be read.\n\n%1").arg(detail));
    }

    const QStringList classIds = classIdsForActivation(entry, metadata);
    const QJsonObject displayLabels = mergedDisplayLabelsForActivation(entry, metadata);
    if (!labelsReadableForActivation(classIds, displayLabels)) {
        return blockedReadiness(
            "classes/labels",
            "This model cannot become active because the class labels could not be read from the model metadata.");
    }

    const QJsonObject targetPolicy = targetPolicyForActivation(entry, metadata);
    if (!targetPolicyReadableForActivation(targetPolicy, classIds)) {
        return blockedReadiness(
            "target/non-target policy",
            "This model cannot become active because the target/non-target sorting policy could not be read.");
    }

    ActiveModelReadiness readiness;
    readiness.ready = true;
    readiness.message = "This model is ready to become active.";
    return readiness;
#endif
    ActiveModelReadiness readiness;
    readiness.ready = true;
    readiness.message = "This model is ready to become active.";
    return readiness;
}

bool activateModelRegistryEntry(const QString& registryFilePath, const QString& registryEntryId, QString* error) {
    if (error)
        error->clear();

    const QString path = registryFilePath.trimmed();
    const QString entryId = registryEntryId.trimmed();
    if (path.isEmpty()) {
        if (error)
            *error = "No registry file path is available.";
        return false;
    }
    if (entryId.isEmpty()) {
        if (error)
            *error = "No model is selected.";
        return false;
    }

    QFile existing(path);
    if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = "Registry file not readable: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QByteArray registryBytes = existing.readAll();
    existing.close();
    const QJsonDocument doc = QJsonDocument::fromJson(registryBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = "Model registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = doc.object();
    if (!repairRegistryBeforeWrite(path, &registry, error))
        return false;
    QJsonArray entries = registry.value("entries").toArray();
    int selectedIndex = -1;
    for (int i = 0; i < entries.size(); ++i) {
        if (registryString(entries.at(i).toObject(), "registry_entry_id").trimmed().compare(entryId, Qt::CaseInsensitive) ==
            0) {
            selectedIndex = i;
            break;
        }
    }
    if (selectedIndex < 0) {
        if (error)
            *error = "Selected model is not present in the registry.";
        return false;
    }

    const ActiveModelReadiness readiness = evaluateActiveModelReadiness(entries.at(selectedIndex).toObject());
    if (!readiness.ready) {
        if (error)
            *error = readiness.message;
        return false;
    }

    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject entry = entries.at(i).toObject();
        const bool active = i == selectedIndex;
        entry["active"] = active;
        entry["selectable_for_normal_live_sorting"] = active;
        if (active) {
            entry["state"] = "promoted_current";
            entry["promotion_status"] = "Active in workspace";
        } else if (registryString(entry, "state") == "promoted_current") {
            entry["state"] = "available";
            entry["promotion_status"] = "Available";
        }
        entries[i] = entry;
    }

    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return writeRegistryFile(path, registry, error);
}

bool updateModelRegistryImageValidationSummary(const QString& registryFilePath, const QString& validationSummaryPath,
                                               QString* updatedEntryId, QString* error) {
    if (updatedEntryId)
        updatedEntryId->clear();
    if (error)
        error->clear();

    const QString path = registryFilePath.trimmed();
    const QString summaryPath = absoluteCleanPath(validationSummaryPath);
    if (path.isEmpty()) {
        if (error)
            *error = "No model registry file path is available.";
        return false;
    }
    if (!QFileInfo(summaryPath).isFile()) {
        if (error)
            *error = "Validation summary is missing: " + summaryPath;
        return false;
    }

    QString summaryError;
    const QJsonObject summary = readJsonObjectFile(summaryPath, &summaryError);
    if (summary.isEmpty() || !validationSummaryHasReadableResult(summary)) {
        if (error)
            *error = summaryError.isEmpty() ? "Validation summary did not contain readable image metrics." : summaryError;
        return false;
    }

    const QJsonObject summaryModel = summary.value("model").toObject();
    const QString validatedModelPath = summaryModel.value("model_path").toString();
    const QString validatedMetadataPath = summaryModel.value("metadata_path").toString();
    const QString normalizedValidatedModel = resolvedRegistryArtifactPathForComparison(validatedModelPath);
    const QString normalizedValidatedMetadata = resolvedRegistryArtifactPathForComparison(validatedMetadataPath);
    if (normalizedValidatedModel.isEmpty() && normalizedValidatedMetadata.isEmpty()) {
        if (error)
            *error = "Validation summary does not identify a model or metadata file.";
        return false;
    }

    QFile existing(path);
    if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = "Registry file not readable: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QByteArray registryBytes = existing.readAll();
    existing.close();
    QJsonDocument registryDoc = QJsonDocument::fromJson(registryBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !registryDoc.isObject()) {
        if (error)
            *error = "Model registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = registryDoc.object();
    if (!repairRegistryBeforeWrite(path, &registry, error))
        return false;
    QJsonArray entries = registry.value("entries").toArray();
    int matchedIndex = -1;
    for (int i = 0; i < entries.size(); ++i) {
        const QJsonObject entry = entries.at(i).toObject();
        const QString entryModel = resolvedRegistryArtifactPathForComparison(registryString(entry, "model_path"));
        const QString entryMetadata = resolvedRegistryArtifactPathForComparison(registryString(entry, "metadata_path"));
        const bool modelMatches =
            !entryModel.isEmpty() && !normalizedValidatedModel.isEmpty() &&
            entryModel.compare(normalizedValidatedModel, Qt::CaseInsensitive) == 0;
        const bool metadataMatches =
            !entryMetadata.isEmpty() && !normalizedValidatedMetadata.isEmpty() &&
            entryMetadata.compare(normalizedValidatedMetadata, Qt::CaseInsensitive) == 0;
        if (modelMatches || metadataMatches) {
            matchedIndex = i;
            break;
        }
    }
    if (matchedIndex < 0) {
        if (error)
        *error = "Model from the validation summary is not present in the model registry.";
        return false;
    }

    QJsonObject entry = entries.at(matchedIndex).toObject();
    QString metadataPath = validatedMetadataPath.trimmed();
    if (metadataPath.isEmpty())
        metadataPath = registryString(entry, "metadata_path");
    if (!QFileInfo(metadataPath).isAbsolute()) {
        const QString resolved = packagedPathCandidate(metadataPath);
        if (!resolved.isEmpty())
            metadataPath = resolved;
    }
    metadataPath = absoluteCleanPath(metadataPath);
    if (!QFileInfo(metadataPath).isFile()) {
        if (error)
            *error = "Model metadata is missing: " + metadataPath;
        return false;
    }

    QString metadataError;
    QJsonObject metadata = readJsonObjectFile(metadataPath, &metadataError);
    if (metadata.isEmpty()) {
        if (error)
            *error = metadataError.isEmpty() ? "Model metadata is empty." : metadataError;
        return false;
    }

    QJsonObject validation = metadata.value("validation_summary").toObject();
    validation["image_validation"] = imageValidationSummaryFromValidatorSummary(summary, summaryPath);
    if (!validation.contains("sequence_validation")) {
        validation["sequence_validation"] = QJsonObject{{"status", "not_run"}};
    }
    metadata["validation_summary"] = validation;
    metadata["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QString metadataWriteError;
    if (!desktop_app::writeJsonObjectAtomically(metadataPath, metadata, &metadataWriteError)) {
        if (error)
            *error = metadataWriteError;
        return false;
    }

    entry["metadata_path"] = metadataPath;
    entry["metadata_sha256"] = sha256FileHex(metadataPath);
    entry["validation_status"] = trainedValidationStatus(metadata);
    entry["validation_evidence"] = validation;
    entry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    entries[matchedIndex] = entry;
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    if (!writeRegistryFile(path, registry, error))
        return false;
    if (updatedEntryId)
        *updatedEntryId = registryString(entry, "registry_entry_id");
    return true;
}

bool renameRegistryEntryDisplayName(const QString& registryFilePath, const QString& registryEntryId,
                                    const QString& displayName, QString* error) {
    if (error)
        error->clear();

    const QString path = registryFilePath.trimmed();
    const QString entryId = registryEntryId.trimmed();
    const QString newDisplayName = displayName.trimmed();
    if (path.isEmpty()) {
        if (error)
            *error = "No registry file path is available.";
        return false;
    }
    if (entryId.isEmpty()) {
        if (error)
            *error = "No model is selected.";
        return false;
    }
    if (newDisplayName.isEmpty()) {
        if (error)
            *error = "Model name cannot be empty.";
        return false;
    }

    QFile existing(path);
    if (!existing.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = "Registry file not readable: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QByteArray registryBytes = existing.readAll();
    existing.close();
    QJsonDocument doc = QJsonDocument::fromJson(registryBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = "Registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = doc.object();
    if (!repairRegistryBeforeWrite(path, &registry, error))
        return false;
    QJsonArray entries = registry.value("entries").toArray();
    bool renamed = false;
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject entry = entries.at(i).toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(entryId, Qt::CaseInsensitive) != 0)
            continue;
        entry["display_name"] = newDisplayName;
        entries[i] = entry;
        renamed = true;
        break;
    }
    if (!renamed) {
        if (error)
            *error = "Selected model is not present in the registry.";
        return false;
    }

    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    return writeRegistryFile(path, registry, error);
}

namespace {

bool readRegistryForPackageMutation(const QString& registryFilePath, QJsonObject* registry, QString* error) {
    QFile file(registryFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = "Registry file not readable: " + registryFilePath;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = "Model registry parse failed: " + parseError.errorString();
        return false;
    }
    *registry = document.object();
    return repairRegistryBeforeWrite(registryFilePath, registry, error);
}

bool safePackageFileName(const QString& name) {
    return !name.trimmed().isEmpty() && name != "." && name != ".." &&
           QFileInfo(name).fileName() == name;
}

QString resolvedPathForComparison(const QString& path) {
    const QString absolutePath = absoluteCleanPath(path);
    if (absolutePath.isEmpty())
        return {};

    QString existingPath = absolutePath;
    QStringList missingSegments;
    while (!QFileInfo::exists(existingPath)) {
        const QFileInfo missing(existingPath);
        const QString segment = missing.fileName();
        const QString parent = missing.absolutePath();
        if (segment.isEmpty() || parent == existingPath)
            return normalizedPathForComparison(absolutePath);
        missingSegments.prepend(segment);
        existingPath = parent;
    }

    const QFileInfo existing(existingPath);
    QString resolved;
    if (existing.isJunction())
        resolved = QFileInfo(existing.junctionTarget()).canonicalFilePath();
    else if (existing.isSymLink() || existing.isAlias())
        resolved = QFileInfo(existing.symLinkTarget()).canonicalFilePath();
    else
        resolved = existing.canonicalFilePath();
    if (resolved.isEmpty())
        resolved = absoluteCleanPath(existingPath);
    for (const QString& segment : missingSegments)
        resolved = QDir(resolved).filePath(segment);
    return normalizedPathForComparison(resolved);
}

bool pathTraversesReparsePoint(const QString& path) {
    QString current = absoluteCleanPath(path);
    while (!current.isEmpty()) {
        const QFileInfo info(current);
        if (info.exists() && (info.isJunction() || info.isSymLink() || info.isAlias()))
            return true;
        const QString parent = info.absolutePath();
        if (parent == current)
            break;
        current = parent;
    }
    return false;
}

bool pathsOverlap(const QString& first, const QString& second) {
    const QString firstResolved = resolvedPathForComparison(first);
    const QString secondResolved = resolvedPathForComparison(second);
    if (firstResolved.isEmpty() || secondResolved.isEmpty())
        return false;
    return firstResolved == secondResolved ||
           firstResolved.startsWith(secondResolved + '/') ||
           secondResolved.startsWith(firstResolved + '/');
}

bool registryEntriesHaveUniqueIdsAndPaths(const QJsonArray& entries, QString* error) {
    QSet<QString> ids;
    QSet<QString> paths;
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        const QString id =
            registryString(entry, "registry_entry_id").trimmed().toLower();
        const QString path = resolvedPathForComparison(
            resolvePackagedPathFromRegistryPath(registryString(entry, "package_path")));
        if (id.isEmpty() || ids.contains(id)) {
            if (error)
                *error = "The Model registry contains duplicate or empty Model IDs.";
            return false;
        }
        if (path.isEmpty() || paths.contains(path)) {
            if (error)
                *error = "The Model registry contains duplicate or empty package paths.";
            return false;
        }
        ids.insert(id);
        paths.insert(path);
    }
    return true;
}

bool copyPackageTree(const QString& sourcePath, const QString& destinationPath, QString* error) {
    const QDir source(sourcePath);
    if (!source.exists()) {
        if (error)
            *error = "Model package folder does not exist: " + sourcePath;
        return false;
    }
    if (!QDir().mkpath(destinationPath)) {
        if (error)
            *error = "Could not create package staging folder: " + destinationPath;
        return false;
    }

    QDirIterator iterator(sourcePath, QDir::AllEntries | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString sourceItemPath = iterator.next();
        const QFileInfo sourceItem(sourceItemPath);
        if (sourceItem.isSymLink()) {
            if (error)
                *error = "Model packages cannot contain symbolic links: " + sourceItemPath;
            return false;
        }
        const QString relativePath = source.relativeFilePath(sourceItemPath);
        const QString destinationItemPath = QDir(destinationPath).filePath(relativePath);
        if (sourceItem.isDir()) {
            if (!QDir().mkpath(destinationItemPath)) {
                if (error)
                    *error = "Could not create package folder: " + destinationItemPath;
                return false;
            }
        } else if (!QFile::copy(sourceItemPath, destinationItemPath)) {
            if (error)
                *error = "Could not copy package file: " + sourceItemPath;
            return false;
        }
    }
    return true;
}

bool createPackageStagingCopy(const QString& sourcePackagePath, const QString& destinationRoot,
                              const QString& finalFolderName, QString* stagingPath,
                              QString* finalPath, QString* error) {
    const QString rootPath = absoluteCleanPath(destinationRoot);
    if (rootPath.isEmpty() || !safePackageFileName(finalFolderName)) {
        if (error)
            *error = "The model destination is invalid.";
        return false;
    }
    if (pathTraversesReparsePoint(rootPath)) {
        if (error)
            *error = "The model destination root cannot be a junction or symbolic link.";
        return false;
    }

    *finalPath = QDir(rootPath).filePath(finalFolderName);
    if (pathsOverlap(sourcePackagePath, *finalPath)) {
        if (error)
            *error = "The source and destination Model Package paths overlap.";
        return false;
    }
    if (QFileInfo::exists(*finalPath)) {
        if (error)
            *error = "A model package already exists at: " + *finalPath;
        return false;
    }
    if (!QDir().mkpath(rootPath)) {
        if (error)
            *error = "Could not create the model destination folder: " + rootPath;
        return false;
    }
    *stagingPath = QDir(rootPath).filePath(
        ".opendss-model-staging-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!copyPackageTree(absoluteCleanPath(sourcePackagePath), *stagingPath, error)) {
        QDir(*stagingPath).removeRecursively();
        return false;
    }
    return true;
}

bool commitPackageStagingCopy(const QString& stagingPath, const QString& finalPath, QString* error) {
    if (QDir().rename(stagingPath, finalPath))
        return true;
    if (error)
        *error = "Could not commit the complete model package at: " + finalPath;
    QDir(stagingPath).removeRecursively();
    return false;
}

QString packageEntryId(const QJsonObject& metadata) {
    return "trained_" + registryIdToken(metadata.value("model_id").toString());
}

bool registryHasEntryOrPath(const QJsonArray& entries, const QString& entryId,
                            const QString& packagePath) {
    const QString normalizedPackage = resolvedPathForComparison(packagePath);
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(
                entryId, Qt::CaseInsensitive) == 0)
            return true;
        if (resolvedPathForComparison(
                resolvePackagedPathFromRegistryPath(registryString(entry, "package_path")))
                .compare(normalizedPackage, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QJsonObject simplePackageEntry(const QString& entryId, const QString& displayName,
                               const QString& packagePath) {
    return {{"registry_entry_id", entryId},
            {"display_name", displayName},
            {"package_path", absoluteCleanPath(packagePath)},
            {"active", false}};
}

bool removeUnregisteredPackage(const QString& packagePath, QString* recoveryPath,
                               QString* error) {
    if (QDir(packagePath).removeRecursively())
        return true;
    if (recoveryPath)
        *recoveryPath = packagePath;
    if (error)
        *error += " The unregistered complete package was retained for recovery at: " +
                  packagePath;
    return false;
}

} // namespace

bool validateCompleteV2ModelPackage(const QString& packagePath, QString* error) {
    if (error)
        error->clear();
    const QString cleanPackagePath = absoluteCleanPath(packagePath);
    if (!QFileInfo(cleanPackagePath).isDir()) {
        if (error)
            *error = "Choose an OpenDSS v2 Model Package folder.";
        return false;
    }

    QString metadataError;
    const QJsonObject metadata =
        readJsonObjectFile(QDir(cleanPackagePath).filePath("metadata.json"), &metadataError);
    if (metadata.isEmpty()) {
        if (error)
            *error = metadataError.isEmpty() ? "metadata.json is malformed." : metadataError;
        return false;
    }
    if (metadata.value("schema_version").toString() != "model-metadata-v2") {
        if (error)
            *error = "Only the current OpenDSS v2 Model Package contract is supported.";
        return false;
    }
    if (metadata.value("model_id").toString().trimmed().isEmpty() ||
        metadata.value("model_name").toString().trimmed().isEmpty()) {
        if (error)
            *error = "Model metadata must contain Model ID and Model Name.";
        return false;
    }
    if (metadata.value("status").toString().compare("trained", Qt::CaseInsensitive) != 0) {
        if (error)
            *error = "Only complete trained OpenDSS v2 Model Packages can be imported.";
        return false;
    }

    const QJsonArray classes = metadata.value("classes").toArray();
    QSet<QString> classIds;
    for (const QJsonValue& value : classes) {
        const QString id = value.toString().trimmed();
        if (id.isEmpty() || classIds.contains(id)) {
            if (error)
                *error = "Model metadata contains invalid or duplicate Class IDs.";
            return false;
        }
        classIds.insert(id);
    }
    if (classes.size() != 2 && classes.size() != 3) {
        if (error)
            *error = "A v2 Model Package must contain two or three classes.";
        return false;
    }

    const QJsonObject architecture = metadata.value("architecture").toObject();
    if (architecture.value("id").toString().trimmed().isEmpty() ||
        architecture.value("num_classes").toInt(-1) != classes.size()) {
        if (error)
            *error = "Model architecture and metadata class count do not agree.";
        return false;
    }
    const QJsonArray inputSize = metadata.value("input_size").toArray();
    const QJsonObject normalization = metadata.value("normalization").toObject();
    if (inputSize.size() < 2 || inputSize.at(0).toInt() <= 0 || inputSize.at(1).toInt() <= 0 ||
        normalization.value("mean").toArray().isEmpty() ||
        normalization.value("std").toArray().isEmpty()) {
        if (error)
            *error = "Required preprocessing and input dimensions are missing.";
        return false;
    }

    const QJsonObject artifact = metadata.value("artifact").toObject();
    const QString onnxName = artifact.value("onnx_file").toString();
    const QString checkpointName = artifact.value("checkpoint_file").toString();
    if (!safePackageFileName(onnxName) || !safePackageFileName(checkpointName)) {
        if (error)
            *error = "Model artifact filenames are invalid.";
        return false;
    }
    const QFileInfo onnx(QDir(cleanPackagePath).filePath(onnxName));
    const QFileInfo checkpoint(QDir(cleanPackagePath).filePath(checkpointName));
    if (!onnx.isFile() || onnx.size() <= 0 || !checkpoint.isFile() ||
        checkpoint.size() <= 0) {
        if (error)
            *error = "A complete v2 Model Package requires metadata.json, model.onnx, and checkpoint.pth.";
        return false;
    }
    const QJsonArray outputShape =
        artifact.value("output_tensor").toObject().value("shape").toArray();
    if (outputShape.isEmpty() ||
        outputShape.at(outputShape.size() - 1).toInt(-1) != classes.size()) {
        if (error)
            *error = "Metadata class count and declared ONNX output dimension do not agree.";
        return false;
    }
    return true;
}

bool importCompleteModelPackage(const QString& registryFilePath, const QString& sourcePackagePath,
                                QString* importedEntryId, QString* importedPackagePath,
                                QString* recoveryPath, QString* error) {
    if (importedEntryId)
        importedEntryId->clear();
    if (importedPackagePath)
        importedPackagePath->clear();
    if (recoveryPath)
        recoveryPath->clear();
    if (!validateCompleteV2ModelPackage(sourcePackagePath, error))
        return false;

    QJsonObject registry;
    if (!readRegistryForPackageMutation(registryFilePath, &registry, error))
        return false;
    if (!registryEntriesHaveUniqueIdsAndPaths(registry.value("entries").toArray(), error))
        return false;
    QString metadataError;
    const QJsonObject metadata = readJsonObjectFile(
        QDir(absoluteCleanPath(sourcePackagePath)).filePath("metadata.json"), &metadataError);
    const QString entryId = packageEntryId(metadata);
    const QString displayName = metadata.value("model_name").toString().trimmed();
    const QString destinationRoot = QFileInfo(registryFilePath).absolutePath();
    QString stagingPath;
    QString finalPath;
    const QString folderName = modelFolderName(displayName);
    finalPath = QDir(destinationRoot).filePath(folderName);
    if (registryHasEntryOrPath(registry.value("entries").toArray(), entryId, finalPath)) {
        if (error)
            *error = "The Model ID or destination package is already registered.";
        return false;
    }
    if (!createPackageStagingCopy(sourcePackagePath, destinationRoot, folderName,
                                  &stagingPath, &finalPath, error) ||
        !commitPackageStagingCopy(stagingPath, finalPath, error))
        return false;

    QJsonArray entries = registry.value("entries").toArray();
    entries.append(simplePackageEntry(entryId, displayName, finalPath));
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!writeRegistryFile(registryFilePath, registry, error)) {
        removeUnregisteredPackage(finalPath, recoveryPath, error);
        return false;
    }
    if (importedEntryId)
        *importedEntryId = entryId;
    if (importedPackagePath)
        *importedPackagePath = finalPath;
    return true;
}

bool exportCompleteModelPackage(const QString& sourcePackagePath, const QString& destinationRoot,
                                QString* exportedPackagePath, QString* error) {
    if (exportedPackagePath)
        exportedPackagePath->clear();
    if (!validateCompleteV2ModelPackage(sourcePackagePath, error))
        return false;
    QString stagingPath;
    QString finalPath;
    if (!createPackageStagingCopy(sourcePackagePath, destinationRoot,
                                  QFileInfo(absoluteCleanPath(sourcePackagePath)).fileName(),
                                  &stagingPath, &finalPath, error) ||
        !commitPackageStagingCopy(stagingPath, finalPath, error))
        return false;
    if (exportedPackagePath)
        *exportedPackagePath = finalPath;
    return true;
}

bool duplicateCompleteModelPackage(const QString& registryFilePath, const QString& sourcePackagePath,
                                   const QString& displayName, const QString& destinationRoot,
                                   QString* duplicatedEntryId, QString* duplicatedPackagePath,
                                   QString* recoveryPath, QString* error) {
    if (duplicatedEntryId)
        duplicatedEntryId->clear();
    if (duplicatedPackagePath)
        duplicatedPackagePath->clear();
    if (recoveryPath)
        recoveryPath->clear();
    const QString newDisplayName = displayName.trimmed();
    if (newDisplayName.isEmpty()) {
        if (error)
            *error = "Model name cannot be empty.";
        return false;
    }
    if (!validateCompleteV2ModelPackage(sourcePackagePath, error))
        return false;

    QJsonObject registry;
    if (!readRegistryForPackageMutation(registryFilePath, &registry, error))
        return false;
    if (!registryEntriesHaveUniqueIdsAndPaths(registry.value("entries").toArray(), error))
        return false;
    const QString newModelId =
        "model_" + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    const QString newEntryId = "trained_" + registryIdToken(newModelId);
    QString stagingPath;
    QString finalPath;
    if (!createPackageStagingCopy(sourcePackagePath, destinationRoot,
                                  modelFolderName(newDisplayName), &stagingPath, &finalPath, error))
        return false;

    const QString stagedMetadataPath = QDir(stagingPath).filePath("metadata.json");
    QString metadataError;
    QJsonObject metadata = readJsonObjectFile(stagedMetadataPath, &metadataError);
    metadata["model_id"] = newModelId;
    metadata["model_name"] = newDisplayName;
    metadata["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    metadata["updated_at"] = metadata.value("created_at");
    if (!desktop_app::writeJsonObjectAtomically(stagedMetadataPath, metadata, error) ||
        !validateCompleteV2ModelPackage(stagingPath, error)) {
        QDir(stagingPath).removeRecursively();
        return false;
    }
    if (registryHasEntryOrPath(registry.value("entries").toArray(), newEntryId, finalPath)) {
        QDir(stagingPath).removeRecursively();
        if (error)
            *error = "The duplicate Model ID or destination package already exists.";
        return false;
    }
    if (!commitPackageStagingCopy(stagingPath, finalPath, error))
        return false;

    QJsonArray entries = registry.value("entries").toArray();
    entries.append(simplePackageEntry(newEntryId, newDisplayName, finalPath));
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!writeRegistryFile(registryFilePath, registry, error)) {
        removeUnregisteredPackage(finalPath, recoveryPath, error);
        return false;
    }
    if (duplicatedEntryId)
        *duplicatedEntryId = newEntryId;
    if (duplicatedPackagePath)
        *duplicatedPackagePath = finalPath;
    return true;
}

bool createLibraryModelIdentity(const QString& registryFilePath,
                                const QString& sourcePackagePath,
                                const QString& displayName,
                                const QString& architectureId,
                                const QString& initializationMode,
                                const QString& destinationRoot,
                                QString* createdEntryId,
                                QString* createdPackagePath,
                                QString* recoveryPath,
                                QString* error) {
    if (createdEntryId)
        createdEntryId->clear();
    if (createdPackagePath)
        createdPackagePath->clear();
    if (recoveryPath)
        recoveryPath->clear();
    if (error)
        error->clear();

    const QString name = displayName.trimmed();
    const QString architecture = architectureId.trimmed();
    const QString mode = initializationMode.trimmed();
    if (name.isEmpty()) {
        if (error)
            *error = "Model name cannot be empty.";
        return false;
    }
    if (architecture != "mobilenet_v3_small" && architecture != "efficientnet_b0") {
        if (error)
            *error = "Architecture must be MobileNetV3-Small or EfficientNet-B0.";
        return false;
    }
    if (mode != "imagenet" && mode != "checkpoint") {
        if (error)
            *error = "Starting Weights are not supported.";
        return false;
    }

    const QString sourcePath = absoluteCleanPath(sourcePackagePath);
    QString metadataError;
    const QJsonObject sourceMetadata =
        readJsonObjectFile(QDir(sourcePath).filePath("metadata.json"), &metadataError);
    if (sourceMetadata.isEmpty()) {
        if (error)
            *error = metadataError.isEmpty() ? "Starting Weights metadata is unavailable."
                                             : metadataError;
        return false;
    }
    if (sourceMetadata.value("schema_version").toString() != "model-metadata-v2"
        || sourceMetadata.value("architecture").toObject().value("id").toString()
               != architecture) {
        if (error)
            *error = "Starting Weights do not match the selected Architecture.";
        return false;
    }

    QString weightFile;
    QString weightHash;
    if (mode == "checkpoint") {
        if (!validateCompleteV2ModelPackage(sourcePath, error))
            return false;
        const QJsonObject artifact = sourceMetadata.value("artifact").toObject();
        weightFile = artifact.value("checkpoint_file").toString().trimmed();
        weightHash = artifact.value("checkpoint_sha256").toString().trimmed();
        const QFileInfo weight(QDir(sourcePath).filePath(weightFile));
        if (weightHash.isEmpty() || !weight.isFile()
            || sha256FileHex(weight.absoluteFilePath()).compare(
                   weightHash, Qt::CaseInsensitive) != 0) {
            if (error)
                *error = "The selected checkpoint Starting Weights failed integrity validation.";
            return false;
        }
    } else {
        weightFile = "imagenet_weights.pth";
        const QFileInfo weight(QDir(sourcePath).filePath(weightFile));
        weightHash =
            sourceMetadata.value("initialization").toObject()
                .value("source_checkpoint_sha256").toString().trimmed();
        if (sourceMetadata.value("status").toString() != "imagenet_transfer_start"
            || !weight.isFile() || !weight.isReadable() || weightHash.isEmpty()
            || sha256FileHex(weight.absoluteFilePath())
                   .compare(weightHash, Qt::CaseInsensitive) != 0) {
            if (error)
                *error = "The approved local ImageNet Starting Weights failed integrity validation.";
            return false;
        }
    }

    QJsonObject registry;
    if (!readRegistryForPackageMutation(registryFilePath, &registry, error))
        return false;
    QJsonArray entries = registry.value("entries").toArray();
    if (!registryEntriesHaveUniqueIdsAndPaths(entries, error))
        return false;
    for (const QJsonValue& value : entries) {
        if (registryString(value.toObject(), "display_name").trimmed()
                .compare(name, Qt::CaseInsensitive) == 0) {
            if (error)
                *error = "A Library model already uses this Name.";
            return false;
        }
    }

    const QString modelId =
        "model_" + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    const QString entryId = "trained_" + registryIdToken(modelId);
    const QString identitiesRoot = destinationRoot.trimmed().isEmpty()
        ? QDir(QFileInfo(registryFilePath).absolutePath())
              .filePath(".opendss-model-identities")
        : absoluteCleanPath(destinationRoot);
    QString stagingPath;
    QString finalPath;
    if (!createPackageStagingCopy(sourcePath, identitiesRoot, entryId,
                                  &stagingPath, &finalPath, error))
        return false;

    const QString stagedMetadataPath = QDir(stagingPath).filePath("metadata.json");
    QJsonObject metadata = sourceMetadata;
    metadata["model_id"] = modelId;
    metadata["model_name"] = name;
    metadata["status"] = "library_identity";
    metadata["origin"] = "blank";
    metadata["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    metadata["updated_at"] = metadata.value("created_at");
    QJsonObject initialization = metadata.value("initialization").toObject();
    initialization["mode"] = mode;
    initialization["weight_file"] = weightFile;
    initialization["weight_sha256"] = weightHash;
    if (mode == "checkpoint") {
        initialization["source_model_id"] =
            sourceMetadata.value("model_id").toString();
        initialization["source_model_name"] =
            sourceMetadata.value("model_name").toString();
    }
    metadata["initialization"] = initialization;

    if (!desktop_app::writeJsonObjectAtomically(stagedMetadataPath, metadata, error)
        || !QFileInfo(QDir(stagingPath).filePath(weightFile)).isFile()) {
        QDir(stagingPath).removeRecursively();
        if (error && error->isEmpty())
            *error = "The Library identity Starting Weights are unavailable.";
        return false;
    }
    if (!commitPackageStagingCopy(stagingPath, finalPath, error))
        return false;

    entries.append(simplePackageEntry(entryId, name, finalPath));
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!writeRegistryFile(registryFilePath, registry, error)) {
        removeUnregisteredPackage(finalPath, recoveryPath, error);
        return false;
    }
    if (createdEntryId)
        *createdEntryId = entryId;
    if (createdPackagePath)
        *createdPackagePath = finalPath;
    return true;
}

bool deleteRegisteredModelPackage(const QString& registryFilePath, const QString& registryEntryId,
                                  bool* registryCommitted, bool* deletedActive,
                                  QString* recoveryPath, QString* error) {
    if (registryCommitted)
        *registryCommitted = false;
    if (deletedActive)
        *deletedActive = false;
    if (recoveryPath)
        recoveryPath->clear();
    if (error)
        error->clear();

    QJsonObject registry;
    if (!readRegistryForPackageMutation(registryFilePath, &registry, error))
        return false;
    QJsonArray entries = registry.value("entries").toArray();
    int selectedIndex = -1;
    QJsonObject selectedEntry;
    for (int index = 0; index < entries.size(); ++index) {
        const QJsonObject entry = entries.at(index).toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(
                registryEntryId.trimmed(), Qt::CaseInsensitive) == 0) {
            if (selectedIndex >= 0) {
                if (error)
                    *error = "The selected Model ID is duplicated in the registry.";
                return false;
            }
            selectedIndex = index;
            selectedEntry = entry;
        }
    }
    if (selectedIndex < 0) {
        if (error)
            *error = "Selected model is not present in the registry.";
        return false;
    }
    if (registry.value("schema_version").toString() != "model-registry-v3-simple" ||
        registryString(selectedEntry, "registry_entry_id").trimmed().isEmpty() ||
        registryString(selectedEntry, "display_name").trimmed().isEmpty() ||
        registryString(selectedEntry, "package_path").trimmed().isEmpty() ||
        !selectedEntry.value("active").isBool()) {
        if (error)
            *error = "The selected Model registry record does not satisfy the current registry schema.";
        return false;
    }
    if (selectedEntry.value("active").toBool()) {
        if (error)
            *error = "The Active Model cannot be removed. Set another model Active first.";
        return false;
    }

    const QString registryPackagePath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(
        registryString(selectedEntry, "package_path")));
    const QFileInfo packageInfo(registryPackagePath);
    const QString canonicalPackagePath = packageInfo.canonicalFilePath();
    if (!packageInfo.isDir()) {
        if (error)
            *error = "Selected Model Package folder is unavailable: " + registryPackagePath;
        return false;
    }
    if (canonicalPackagePath.isEmpty() || packageInfo.isSymLink() ||
        packageInfo.isJunction() || packageInfo.isAlias() ||
        normalizedPathForComparison(registryPackagePath) !=
            normalizedPathForComparison(canonicalPackagePath)) {
        if (error)
            *error = "The selected Model Package path is an alias or reparse point.";
        return false;
    }
    if (!validateCompleteV2ModelPackage(canonicalPackagePath, error))
        return false;
    const ModelPackageInspection inspection = inspectModelPackage(selectedEntry);
    if (resolvedPathForComparison(inspection.packagePath) !=
            normalizedPathForComparison(canonicalPackagePath) ||
        QFileInfo(inspection.metadataPath).canonicalPath().compare(
            canonicalPackagePath, Qt::CaseInsensitive) != 0 ||
        QFileInfo(inspection.onnxPath).canonicalPath().compare(
            canonicalPackagePath, Qt::CaseInsensitive) != 0 ||
        QFileInfo(inspection.checkpointPath).canonicalPath().compare(
            canonicalPackagePath, Qt::CaseInsensitive) != 0) {
        if (error)
            *error = "The selected registry record does not resolve to one exact Model Package.";
        return false;
    }
    const QString selectedCanonical =
        normalizedPathForComparison(canonicalPackagePath);
    for (int index = 0; index < entries.size(); ++index) {
        if (index == selectedIndex)
            continue;
        const QString otherPath = resolvedPathForComparison(
            resolvePackagedPathFromRegistryPath(
                registryString(entries.at(index).toObject(), "package_path")));
        if (!otherPath.isEmpty() && otherPath == selectedCanonical) {
            if (error)
                *error = "The selected Model Package path is registered to another Model ID.";
            return false;
        }
    }
    const QString recoveryRoot =
        QDir(QFileInfo(canonicalPackagePath).absolutePath()).filePath(".opendss-model-recovery");
    if (pathTraversesReparsePoint(recoveryRoot)) {
        if (error)
            *error = "The Model recovery root cannot be a junction or symbolic link.";
        return false;
    }
    if (!QDir().mkpath(recoveryRoot)) {
        if (error)
            *error = "Could not create the model deletion recovery folder.";
        return false;
    }
    const QString stagedRecoveryPath = QDir(recoveryRoot).filePath(
        QFileInfo(canonicalPackagePath).fileName() + "-" +
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().rename(canonicalPackagePath, stagedRecoveryPath)) {
        if (error)
            *error = "Could not stage the Model Package for recoverable deletion.";
        return false;
    }

    entries.removeAt(selectedIndex);
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!writeRegistryFile(registryFilePath, registry, error)) {
        if (!QDir().rename(stagedRecoveryPath, canonicalPackagePath)) {
            QString copyError;
            if (!copyPackageTree(stagedRecoveryPath, canonicalPackagePath, &copyError)) {
                if (recoveryPath)
                    *recoveryPath = stagedRecoveryPath;
                if (error)
                    *error += " Registry rollback left the complete package at: " +
                              stagedRecoveryPath + ". " + copyError;
            }
        }
        return false;
    }
    if (registryCommitted)
        *registryCommitted = true;

    QString pathInTrash;
    if (!QFile::moveToTrash(stagedRecoveryPath, &pathInTrash)) {
        if (recoveryPath)
            *recoveryPath = stagedRecoveryPath;
        if (error)
            *error = "Model removal committed, but its recovery copy could not be moved to "
                     "the Recycle Bin. "
                     "The retained recovery path is: " + stagedRecoveryPath;
        return false;
    }
    QDir().rmdir(recoveryRoot);
    return true;
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
    const QString packaged = packagedPathCandidate(trimmed);
    if (!packaged.isEmpty())
        return packaged;

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
        if (entry.value("active").toBool(false) && registryString(entry, "live_use_mode") != "blocked") {
            return entry;
        }
    }
    for (const auto& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value("selectable_for_normal_live_sorting").toBool(false) &&
            registryString(entry, "live_use_mode") != "blocked") {
            return entry;
        }
    }
    return entries.isEmpty() ? QJsonObject{} : entries.first().toObject();
}

bool repairTrustedPretrainedMetadataHash(const QString& installedPackagePath,
                                         const QString& trustedPackagePath,
                                         QString* error) {
    if (error)
        error->clear();

    const QDir installed(installedPackagePath);
    const QDir trusted(trustedPackagePath);
    const QString installedMetadataPath = installed.filePath("metadata.json");
    const QString installedModelPath = installed.filePath("model.onnx");
    const QString trustedMetadataPath = trusted.filePath("metadata.json");
    const QString trustedModelPath = trusted.filePath("model.onnx");

    QString readError;
    QJsonObject installedMetadata =
        readJsonObjectFile(installedMetadataPath, &readError);
    if (installedMetadata.isEmpty()) {
        if (error)
            *error = readError;
        return false;
    }
    QJsonObject installedArtifact =
        installedMetadata.value("artifact").toObject();
    if (!installedArtifact.value("onnx_sha256").toString().trimmed().isEmpty())
        return true;

    const QJsonObject trustedMetadata =
        readJsonObjectFile(trustedMetadataPath, &readError);
    const QString trustedHash =
        trustedMetadata.value("artifact")
            .toObject()
            .value("onnx_sha256")
            .toString()
            .trimmed()
            .toLower();
    if (trustedMetadata.isEmpty() || trustedHash.size() != 64) {
        if (error) {
            *error = readError.isEmpty()
                         ? QStringLiteral(
                               "The bundled pretrained model has no trusted ONNX SHA-256.")
                         : readError;
        }
        return false;
    }

    QJsonObject normalizedInstalledMetadata = installedMetadata;
    QJsonObject normalizedInstalledArtifact =
        normalizedInstalledMetadata.value("artifact").toObject();
    normalizedInstalledArtifact.remove("onnx_sha256");
    normalizedInstalledMetadata["artifact"] = normalizedInstalledArtifact;

    QJsonObject normalizedTrustedMetadata = trustedMetadata;
    QJsonObject normalizedTrustedArtifact =
        normalizedTrustedMetadata.value("artifact").toObject();
    normalizedTrustedArtifact.remove("onnx_sha256");
    normalizedTrustedMetadata["artifact"] = normalizedTrustedArtifact;
    if (normalizedInstalledMetadata != normalizedTrustedMetadata) {
        if (error)
            *error = QStringLiteral(
                "The installed pretrained metadata does not match the bundled trusted metadata.");
        return false;
    }

    if (sha256FileHex(trustedModelPath).compare(
            trustedHash, Qt::CaseInsensitive) != 0) {
        if (error)
            *error = QStringLiteral(
                "The bundled pretrained ONNX file does not match its trusted SHA-256.");
        return false;
    }
    if (sha256FileHex(installedModelPath).compare(
            trustedHash, Qt::CaseInsensitive) != 0) {
        if (error)
            *error = QStringLiteral(
                "The installed pretrained ONNX file does not match the bundled trusted model.");
        return false;
    }

    installedArtifact["onnx_sha256"] = trustedHash;
    installedMetadata["artifact"] = installedArtifact;
    QString writeError;
    if (!desktop_app::writeJsonObjectAtomically(
            installedMetadataPath, installedMetadata, &writeError)) {
        if (error)
            *error = writeError;
        return false;
    }
    return true;
}

DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries) {
    DefaultWorkspacePaths paths;
    paths.root = defaultOpenDssRootPath();
    paths.collections = defaultOpenDssCollectionsPath();
    paths.models = defaultOpenDssModelsPath();
    paths.datasets = defaultOpenDssDatasetsPath();
    paths.preparedDatasets = defaultOpenDssPreparedDatasetsPath();
    paths.runs = defaultOpenDssRunsPath();
    paths.trainingRuns = defaultOpenDssTrainingRunsPath();
    paths.validationRuns = defaultOpenDssValidationRunsPath();
    paths.reports = defaultOpenDssReportsPath();
    QDir().mkpath(paths.collections);
    QDir().mkpath(paths.models);
    QDir().mkpath(paths.datasets);
    QDir().mkpath(paths.preparedDatasets);
    QDir().mkpath(paths.runs);
    QDir().mkpath(paths.trainingRuns);
    QDir().mkpath(paths.validationRuns);
    QDir().mkpath(paths.reports);

    for (const QString& architecture :
         {QStringLiteral("mobilenet_v3_small"),
          QStringLiteral("efficientnet_b0")}) {
        const QString installedPackage =
            QDir(paths.models)
                .filePath(QStringLiteral("packages/pretrained/%1")
                              .arg(architecture));
        const QString trustedPackage = findPackagedAppPath(
            QStringLiteral("models/templates/pretrained/%1")
                .arg(architecture));
        if (QFileInfo(installedPackage).isDir() &&
            QFileInfo(trustedPackage).isDir()) {
            QString repairError;
            if (!repairTrustedPretrainedMetadataHash(
                    installedPackage, trustedPackage, &repairError)) {
                qWarning().noquote()
                    << "Could not repair trusted pretrained metadata:"
                    << repairError;
            }
        }
    }

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
        if (datasetName == "droplet_target_nontarget_binary_starter") {
            paths.preparedDataset = destinationDataset;
            paths.preparedDatasetManifest = QDir(destinationDataset).filePath("metadata/dataset_manifest.json");
        }
    }
    return paths;
}
