#include "model_load_service.h"

#include "../../desktop_app/json_persistence.h"
#include "../../desktop_app/model_registry_service.h"
#ifndef OPENDSS_MODEL_LOAD_NO_PIPELINE
#include "../../desktop_app/pipeline_runner.h"
#endif

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

#include <cmath>

namespace {

QJsonObject readJsonObject(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "File is not readable: " + path;
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = "JSON parse failed for " + path + ": " + parseError.errorString();
        return {};
    }
    return document.object();
}

QString sha256File(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

bool validateExternalFiles(const QJsonArray& declarations, const QDir& packageDir, QString* error) {
    for (const QJsonValue& value : declarations) {
        QString name;
        QString expectedHash;
        bool required = true;
        if (value.isString()) {
            name = value.toString();
        } else if (value.isObject()) {
            const QJsonObject declaration = value.toObject();
            name = declaration.value("filename").toString(declaration.value("file").toString());
            expectedHash = declaration.value("sha256").toString().trimmed();
            required = declaration.value("required").toBool(true);
        }
        if (!required)
            continue;
        if (name.trimmed().isEmpty()) {
            if (error)
                *error = "A required ONNX external-data file has no declared name.";
            return false;
        }
        const QString path = packageDir.filePath(name);
        const QFileInfo info(path);
        if (!info.isFile() || info.size() <= 0) {
            if (error)
                *error = "Required ONNX external-data file is missing or empty: " + path;
            return false;
        }
        if (!expectedHash.isEmpty() && sha256File(path).compare(expectedHash, Qt::CaseInsensitive) != 0) {
            if (error)
                *error = "Required ONNX external-data file hash does not match: " + path;
            return false;
        }
    }
    return true;
}

QJsonObject registryObject(const QString& path, QString* error) {
    return readJsonObject(path, error);
}

QJsonObject registryEntryById(const QJsonArray& entries, const QString& id) {
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(id.trimmed(), Qt::CaseInsensitive) == 0)
            return entry;
    }
    return {};
}

bool samePath(const QString& left, const QString& right) {
    const QString normalizedLeft = QDir::cleanPath(QFileInfo(left).absoluteFilePath());
    const QString normalizedRight = QDir::cleanPath(QFileInfo(right).absoluteFilePath());
#ifdef Q_OS_WIN
    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0;
#else
    return normalizedLeft == normalizedRight;
#endif
}

struct RegistrySnapshot {
    bool existed = false;
    QByteArray bytes;
};

bool readRegistrySnapshot(const QString& path, RegistrySnapshot& snapshot, QString* error) {
    snapshot = {};
    if (!QFileInfo::exists(path))
        return true;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = "Could not read the model registry before saving: " + file.errorString();
        return false;
    }
    snapshot.bytes = file.readAll();
    if (file.error() != QFile::NoError) {
        if (error)
            *error = "Could not read the complete model registry before saving: " + file.errorString();
        return false;
    }
    snapshot.existed = true;
    return true;
}

bool restoreRegistrySnapshot(const QString& path, const RegistrySnapshot& snapshot, QString* error) {
    if (!snapshot.existed) {
        if (!QFileInfo::exists(path) || QFile::remove(path))
            return true;
        if (error)
            *error = "Could not remove the newly created model registry during rollback.";
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = "Could not open the model registry for rollback: " + file.errorString();
        return false;
    }
    if (file.write(snapshot.bytes) != snapshot.bytes.size() || !file.commit()) {
        if (error)
            *error = "Could not restore the model registry during rollback: " + file.errorString();
        return false;
    }
    return true;
}

std::unique_ptr<OnnxInferenceAdapter>
prepareRegistryEntry(const QJsonObject& entry, const QString& requestedDevice,
                     QString* warning, QString* error) {
    const ModelPackageInspection inspection = inspectModelPackage(entry);
    if (!inspection.canActivate) {
        if (error)
            *error = inspection.message.isEmpty()
                         ? "Selected model package is not loadable."
                         : inspection.message;
        return {};
    }

    QString metadataError;
    const QJsonObject metadataJson =
        readJsonObject(inspection.metadataPath, &metadataError);
    if (metadataJson.isEmpty()) {
        if (error)
            *error = metadataError;
        return {};
    }

    Metadata metadata;
    std::string loadError;
    if (!LoadMetadata(inspection.metadataPath.toStdString(), metadata,
                      loadError)) {
        if (error)
            *error = QString::fromStdString(loadError);
        return {};
    }

    const QJsonArray declaredClasses = metadataJson.value("classes").toArray();
    if (declaredClasses.size() != static_cast<int>(metadata.classes.size())) {
        if (error)
            *error =
                "Declared metadata classes do not match the supported metadata projection.";
        return {};
    }
    for (int index = 0; index < declaredClasses.size(); ++index) {
        if (!declaredClasses.at(index).isString() ||
            declaredClasses.at(index).toString().toStdString() !=
                metadata.classes[static_cast<std::size_t>(index)]) {
            if (error)
                *error = "Declared metadata class order is invalid.";
            return {};
        }
    }

    const QFileInfo onnxInfo(inspection.onnxPath);
    if (!onnxInfo.isFile() || onnxInfo.size() <= 0) {
        if (error)
            *error =
                "Declared ONNX model is missing or empty: " + inspection.onnxPath;
        return {};
    }
    const QDir packageDir(inspection.packagePath);
    const QJsonObject metadataArtifact =
        metadataJson.value("artifact").toObject();
    if (!validateExternalFiles(
            metadataArtifact.value("external_data_files").toArray(),
            packageDir, error)) {
        return {};
    }

    const QString expectedHash =
        metadataArtifact.value("onnx_sha256").toString().trimmed();
    if (expectedHash.isEmpty()) {
        if (error)
            *error = "Selected package has no trusted declared ONNX SHA-256.";
        return {};
    }
    if (sha256File(inspection.onnxPath)
            .compare(expectedHash, Qt::CaseInsensitive) != 0) {
        if (error)
            *error = "Declared ONNX SHA-256 does not match the model file.";
        return {};
    }
    const QString metadataHash = sha256File(inspection.metadataPath);
    if (metadataHash.isEmpty()) {
        if (error)
            *error = "Could not hash the validated metadata file.";
        return {};
    }

    auto candidate = std::make_unique<OnnxInferenceAdapter>();
    std::string adapterMessage;
    if (!candidate->load(
            registryString(entry, "registry_entry_id").toStdString(),
            inspection.onnxPath.toStdString(),
            inspection.metadataPath.toStdString(), metadata,
            requestedDevice.toStdString(), adapterMessage)) {
        if (error)
            *error = QString::fromStdString(adapterMessage);
        return {};
    }
    const QJsonObject sortingPolicy =
        metadataJson.value("sorting_policy").toObject();
    if (!candidate->setSortingPolicy(
            sortingPolicy.value("target_class_id")
                .toString()
                .trimmed()
                .toStdString(),
            sortingPolicy.value("target_display_label")
                .toString()
                .trimmed()
                .toStdString(),
            sortingPolicy.value("trigger_rule")
                .toString()
                .trimmed()
                .toStdString(),
            adapterMessage)) {
        if (error)
            *error = QString::fromStdString(adapterMessage);
        return {};
    }
    candidate->setArtifactIdentity(expectedHash.toStdString(),
                                   metadataHash.toStdString());
    if (warning)
        *warning = QString::fromStdString(adapterMessage);
    return candidate;
}

} // namespace

namespace desktop_app::v2 {

ModelLoadService::ModelLoadService(QString registryFilePath)
    : registryFilePath_(std::move(registryFilePath)) {}

PersistedActiveModelInspection ModelLoadService::inspectPersistedActive() const {
    PersistedActiveModelInspection result;
    QString readError;
    const QJsonObject registry = registryObject(registryFilePath_, &readError);
    if (registry.isEmpty()) {
        result.error = readError;
        return result;
    }

    QJsonObject activeEntry;
    int activeCount = 0;
    for (const QJsonValue& value : registry.value("entries").toArray()) {
        const QJsonObject entry = value.toObject();
        if (entry.value("active").toBool(false)) {
            activeEntry = entry;
            ++activeCount;
        }
    }
    if (activeCount != 1) {
        result.error =
            QStringLiteral("Model registry must contain exactly one active entry; found %1.")
                .arg(activeCount);
        return result;
    }

    result.id = registryString(activeEntry, "registry_entry_id").trimmed();
    result.displayName = registryString(activeEntry, "display_name").trimmed();
    const ModelPackageInspection package = inspectModelPackage(activeEntry);
    result.classCount = package.classCount;
    if (result.id.isEmpty()) {
        result.error = QStringLiteral("The Active Model registry ID is invalid.");
    } else if (result.displayName.isEmpty()) {
        result.error =
            QStringLiteral("The Active Model registry display name is invalid.");
    } else if (!package.canActivate) {
        result.error = package.message;
    } else if (result.classCount != 2 && result.classCount != 3) {
        result.error =
            QStringLiteral("The Active Model must define two or three classes.");
    } else {
        QString metadataError;
        const QJsonObject metadataJson =
            readJsonObject(package.metadataPath, &metadataError);
        Metadata metadata;
        std::string loadError;
        if (metadataJson.isEmpty()) {
            result.error = metadataError;
            return result;
        }
        if (!LoadMetadata(package.metadataPath.toStdString(), metadata,
                          loadError)) {
            result.error = QString::fromStdString(loadError);
            return result;
        }

        const QJsonArray declaredClasses = metadataJson.value("classes").toArray();
        const QJsonObject displayLabels =
            metadataJson.value("display_labels").toObject();
        if (declaredClasses.size() != result.classCount ||
            declaredClasses.size() != static_cast<int>(metadata.classes.size())) {
            result.error =
                QStringLiteral("The Active Model class metadata is inconsistent.");
            return result;
        }
        for (int index = 0; index < declaredClasses.size(); ++index) {
            const QJsonValue classValue = declaredClasses.at(index);
            const QString classId =
                classValue.isString() ? classValue.toString().trimmed() : QString{};
            const QString displayLabel =
                displayLabels.value(classId).toString().trimmed();
            if (classId.isEmpty() || displayLabel.isEmpty() ||
                classId.toStdString() !=
                    metadata.classes[static_cast<std::size_t>(index)]) {
                result.error =
                    QStringLiteral("The Active Model class labels are incomplete.");
                return result;
            }
            result.classes.push_back({classId, displayLabel});
        }

        const QJsonObject artifact = metadataJson.value("artifact").toObject();
        if (!validateExternalFiles(
                artifact.value("external_data_files").toArray(),
                QDir(package.packagePath), &result.error)) {
            return result;
        }
        const QString declaredModelSha256 =
            artifact.value("onnx_sha256").toString().trimmed().toLower();
        const QString actualModelSha256 = sha256File(package.onnxPath).toLower();
        if (actualModelSha256.size() != 64 ||
            (!declaredModelSha256.isEmpty() &&
             (declaredModelSha256.size() != 64 ||
              actualModelSha256 != declaredModelSha256))) {
            result.error = QStringLiteral(
                "The Active Model ONNX SHA-256 does not match its package metadata.");
            return result;
        }
        result.modelSha256 = actualModelSha256;
        result.plannedDevice =
            QString::fromStdString(OnnxClassifier::plannedAutomaticDevice());
        result.loadable = true;
    }
    return result;
}

std::unique_ptr<OnnxInferenceAdapter> ModelLoadService::prepare(const QString& registryEntryId,
                                                                const QString& requestedDevice,
                                                                QString* warning,
                                                                QString* error) const {
    if (warning)
        warning->clear();
    if (error)
        error->clear();

    QString readError;
    const QJsonObject registry = registryObject(registryFilePath_, &readError);
    if (registry.isEmpty()) {
        if (error)
            *error = readError;
        return {};
    }
    const QJsonObject entry = registryEntryById(registry.value("entries").toArray(), registryEntryId);
    if (entry.isEmpty()) {
        if (error)
            *error = "Selected model is not present in the registry.";
        return {};
    }

    return prepareRegistryEntry(entry, requestedDevice, warning, error);
}

std::unique_ptr<OnnxInferenceAdapter> ModelLoadService::preparePersistedActive(const QString& requestedDevice,
                                                                               QString* warning,
                                                                               QString* error) const {
    return preparePersistedActive(requestedDevice, warning, error, nullptr);
}

std::unique_ptr<OnnxInferenceAdapter> ModelLoadService::preparePersistedActive(const QString& requestedDevice,
                                                                               QString* warning,
                                                                               QString* error,
                                                                               QString* activeDisplayName) const {
    if (warning)
        warning->clear();
    if (error)
        error->clear();
    if (activeDisplayName)
        activeDisplayName->clear();
    QString readError;
    const QJsonObject registry = registryObject(registryFilePath_, &readError);
    if (registry.isEmpty()) {
        if (error)
            *error = readError;
        return {};
    }
    QJsonObject activeEntry;
    int activeCount = 0;
    for (const QJsonValue& value : registry.value("entries").toArray()) {
        const QJsonObject entry = value.toObject();
        if (entry.value("active").toBool(false)) {
            activeEntry = entry;
            ++activeCount;
        }
    }
    if (activeCount != 1) {
        if (error)
            *error = QString("Model registry must contain exactly one active entry; found %1.").arg(activeCount);
        return {};
    }
    auto candidate =
        prepareRegistryEntry(activeEntry, requestedDevice, warning, error);
    if (candidate && activeDisplayName)
        *activeDisplayName = registryString(activeEntry, "display_name");
    return candidate;
}

#ifndef OPENDSS_MODEL_LOAD_NO_PIPELINE
bool ModelLoadService::activateAndInstall(std::unique_ptr<OnnxInferenceAdapter> candidate,
                                          PipelineRunner& pipeline,
                                          QString* error) const {
    if (error)
        error->clear();
    if (!candidate) {
        if (error)
            *error = "No prepared model candidate is available.";
        return false;
    }
    if (pipeline.isReady()) {
        if (error)
            *error = "Stop the active pipeline before changing the model.";
        return false;
    }

    const QString entryId = QString::fromStdString(candidate->modelId());
    QString readError;
    QJsonObject registry = registryObject(registryFilePath_, &readError);
    if (registry.isEmpty()) {
        if (error)
            *error = readError;
        return false;
    }

    QJsonArray entries = registry.value("entries").toArray();
    int selectedIndex = -1;
    for (int index = 0; index < entries.size(); ++index) {
        const QJsonObject entry = entries.at(index).toObject();
        if (registryString(entry, "registry_entry_id").trimmed().compare(entryId.trimmed(), Qt::CaseInsensitive) == 0) {
            if (selectedIndex >= 0) {
                if (error)
                    *error = "Selected model ID is duplicated in the registry.";
                return false;
            }
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0) {
        if (error)
            *error = "Selected model is not present in the registry.";
        return false;
    }

    const ModelPackageInspection inspection = inspectModelPackage(entries.at(selectedIndex).toObject());
    if (!inspection.canActivate ||
        !samePath(inspection.onnxPath, QString::fromStdString(candidate->modelPath())) ||
        !samePath(inspection.metadataPath, QString::fromStdString(candidate->metadataPath()))) {
        if (error)
            *error = "Selected model package changed after it was prepared.";
        return false;
    }

    for (int index = 0; index < entries.size(); ++index) {
        QJsonObject entry = entries.at(index).toObject();
        entry["active"] = index == selectedIndex;
        entries[index] = entry;
    }
    registry["entries"] = entries;
    registry["updated_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!desktop_app::writeJsonObjectAtomically(registryFilePath_, registry, error))
        return false;

    pipeline.installInference(std::move(candidate));
    return true;
}

bool ModelLoadService::saveAndActivateTrainedModel(
    const QString& runDir,
    const QString& modelOnnxPath,
    const QString& metadataJsonPath,
    const QString& modelName,
    const QString& destinationRoot,
    const QString& requestedDevice,
    PipelineRunner& pipeline,
    QString* registeredEntryId,
    QString* warning,
    QString* error) const {
    if (registeredEntryId)
        registeredEntryId->clear();
    if (warning)
        warning->clear();
    if (error)
        error->clear();

    RegistrySnapshot priorRegistry;
    QString snapshotError;
    if (!readRegistrySnapshot(registryFilePath_, priorRegistry, &snapshotError)) {
        if (error)
            *error = snapshotError;
        return false;
    }

    QString packagePath;
    QString entryId;
    QString saveError;
    if (!saveTrainedModelArtifacts(
            registryFilePath_, runDir, modelOnnxPath, metadataJsonPath, QString(), QString(),
            QString(), QString(), QString(), modelName, destinationRoot, &packagePath, &entryId,
            &saveError)) {
        if (error)
            *error = saveError;
        return false;
    }

    QString prepareWarning;
    QString completionError;
    auto candidate = prepare(entryId, requestedDevice, &prepareWarning, &completionError);
    if (candidate && activateAndInstall(std::move(candidate), pipeline, &completionError)) {
        if (registeredEntryId)
            *registeredEntryId = entryId;
        if (warning)
            *warning = prepareWarning;
        return true;
    }

    QString rollbackError;
    if (!restoreRegistrySnapshot(registryFilePath_, priorRegistry, &rollbackError)) {
        if (error) {
            *error = completionError + " Registry rollback also failed: " + rollbackError
                + " The saved package was retained at " + packagePath + '.';
        }
        return false;
    }

    QDir packageDirectory(packagePath);
    if (!packageDirectory.removeRecursively()) {
        if (error)
            *error = completionError + " Registry rollback succeeded, but the saved package could not be removed: "
                + packagePath;
        return false;
    }

    if (error)
        *error = completionError;
    return false;
}

void ModelLoadService::installPersisted(std::unique_ptr<OnnxInferenceAdapter> candidate,
                                        PipelineRunner& pipeline) const noexcept {
    pipeline.installInference(std::move(candidate));
}
#endif

} // namespace desktop_app::v2
