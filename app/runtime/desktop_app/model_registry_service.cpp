#include "model_registry_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
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
constexpr auto kModelsRootOverrideEnv = "OVDS_MODELS_ROOT_PATH";

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
    blank["display_name"] = "Blank SqueezeNet starter (ImageNet-start)";
    blank["state"] = "available";
    blank["model_status"] = "Untrained";
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
    backup["display_name"] = "Cell aggregate model V1 (2026-05-14)";
    backup["state"] = "available";
    backup["model_status"] = "Trained";
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
    return makePackagedBlankModelRegistryEntry();
}

QJsonObject packagedPretrainedModelRegistryEntry() {
    return makePackagedPretrainedModelRegistryEntry();
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
    return doc.object().value("entries").toArray();
}

QString registryString(const QJsonObject& entry, const QString& key) {
    return entry.value(key).toString();
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
        const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error)
                *error = "Model registry parse failed: " + parseError.errorString();
            return false;
        }
        registry = doc.object();
    }
    if (registry.value("schema_version").toString().isEmpty())
        registry["schema_version"] = "model-registry-v1";

    QJsonArray entries = registry.value("entries").toArray();
    const QString modelId = firstNonEmpty({metadata.value("model_id").toString(), QFileInfo(runPath).fileName(),
                                           QFileInfo(modelPath).completeBaseName()});
    const QString entryId = "trained_" + registryIdToken(modelId);
    const QString createdAt = firstNonEmpty({metadata.value("created_at").toString(),
                                             QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    const QString displayName =
        firstNonEmpty({metadata.value("model_name").toString(), QString("Trained model %1").arg(createdAt)});

    QJsonObject entry;
    entry["registry_entry_id"] = entryId;
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

    const QString rootPath = modelsRootPath();
    QDir root(rootPath);
    if (!root.exists() && !QDir().mkpath(rootPath)) {
        if (error)
            *error = "Could not create model workspace folder: " + rootPath;
        return false;
    }
    const QString destinationDirPath = root.filePath(modelFolderName(displayName));
    if (QFileInfo::exists(destinationDirPath)) {
        if (error)
            *error = "A model folder already exists for this name: " + destinationDirPath;
        return false;
    }
    if (!root.mkpath(QFileInfo(destinationDirPath).fileName())) {
        if (error)
            *error = "Could not create model folder: " + destinationDirPath;
        return false;
    }

    bool createdDestination = true;
    auto cleanupOnFailure = [&]() {
        if (createdDestination) {
            QDir destination(destinationDirPath);
            destination.removeRecursively();
            createdDestination = false;
        }
    };

    const QString promotedModelPath = QDir(destinationDirPath).filePath("model.onnx");
    const QString promotedMetadataPath = QDir(destinationDirPath).filePath("metadata.json");
    const QString promotedMetricsPath = QDir(destinationDirPath).filePath("metrics.csv");
    const QString promotedMetricsJsonPath = QDir(destinationDirPath).filePath("metrics.json");
    const QString promotedClassMetricsPath = QDir(destinationDirPath).filePath("class_metrics.csv");
    const QString promotedConfusionMatrixPath = QDir(destinationDirPath).filePath("confusion_matrix.csv");
    const QString promotedConfigPath = QDir(destinationDirPath).filePath("training_config.json");

    if (!copyRequiredModelFile(modelOnnxPath, promotedModelPath, "Trained ONNX model", error) ||
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

    QJsonObject artifact = metadata.value("artifact").toObject();
    artifact["onnx_file"] = "model.onnx";
    artifact["onnx_sha256"] = sha256FileHex(promotedModelPath);
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
    metadata["model_id"] = "saved_" + registryIdToken(displayName);
    metadata["status"] = "trained";
    metadata["training_run_dir"] = absoluteCleanPath(runDir);
    metadata["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QFile metadataFile(promotedMetadataPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error)
            *error = "Trained model metadata is not writable: " + promotedMetadataPath;
        cleanupOnFailure();
        return false;
    }
    metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    metadataFile.close();

    QString entryId;
    QString registrationError;
    if (!registerTrainedModelArtifacts(registryFilePath, runDir, promotedModelPath, promotedMetadataPath, &entryId,
                                       &registrationError)) {
        if (error)
            *error = registrationError;
        cleanupOnFailure();
        return false;
    }

    createdDestination = false;
    if (registeredEntryId)
        *registeredEntryId = entryId;
    return true;
}

ActiveModelReadiness evaluateActiveModelReadiness(const QJsonObject& entry) {
    const QString configuredModelPath = registryString(entry, "model_path").trimmed();
    const QString modelPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredModelPath));
    if (!QFileInfo(modelPath).isFile()) {
        return blockedReadiness(
            "model.onnx",
            QString("This model cannot become active because the ONNX model file is missing.\n\nExpected file:\n%1")
                .arg(QDir::toNativeSeparators(modelPath.isEmpty() ? configuredModelPath : modelPath)));
    }

    const QString configuredMetadataPath = registryString(entry, "metadata_path").trimmed();
    const QString metadataPath = absoluteCleanPath(resolvePackagedPathFromRegistryPath(configuredMetadataPath));
    if (!QFileInfo(metadataPath).isFile()) {
        return blockedReadiness(
            "metadata.json",
            QString("This model cannot become active because the metadata file is missing.\n\nExpected file:\n%1")
                .arg(QDir::toNativeSeparators(metadataPath.isEmpty() ? configuredMetadataPath : metadataPath)));
    }

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
    const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = "Model registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = doc.object();
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
    QJsonDocument registryDoc = QJsonDocument::fromJson(existing.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !registryDoc.isObject()) {
        if (error)
            *error = "Model registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = registryDoc.object();
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

    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error)
            *error = "Model metadata is not writable: " + metadataPath;
        return false;
    }
    metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
    metadataFile.close();

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
    QJsonDocument doc = QJsonDocument::fromJson(existing.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = "Registry parse failed: " + parseError.errorString();
        return false;
    }

    QJsonObject registry = doc.object();
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
