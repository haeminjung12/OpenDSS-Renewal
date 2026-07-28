#include "model_library_controller.h"
#include "model_load_service.h"

#include "../operation/operation_coordinator.h"
#include "../../desktop_app/model_registry_service.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>
#include <QTemporaryDir>

#include <utility>

namespace desktop_app::v2 {
namespace {

QString entryId(const QJsonObject &entry)
{
    return registryString(entry, QStringLiteral("registry_entry_id")).trimmed();
}

QString entryName(const QJsonObject &entry)
{
    const QString name = registryString(entry, QStringLiteral("display_name")).trimmed();
    return name.isEmpty() ? entryId(entry) : name;
}

QJsonObject projectionEntry(const QJsonObject &entry,
                            const ModelPackageInspection &inspection)
{
    const QString id = entryId(entry);
    QString architectureId;
    QString origin;
    if (id == QStringLiteral("opendss_blank_mobilenet_v3_small")) {
        architectureId = QStringLiteral("mobilenet_v3_small");
        origin = QStringLiteral("blank");
    } else if (id == QStringLiteral("opendss_pretrained_mobilenet_v3_small")) {
        architectureId = QStringLiteral("mobilenet_v3_small");
        origin = QStringLiteral("pretrained");
    } else if (id == QStringLiteral("opendss_blank_efficientnet_b0")) {
        architectureId = QStringLiteral("efficientnet_b0");
        origin = QStringLiteral("blank");
    } else if (id == QStringLiteral("opendss_pretrained_efficientnet_b0")) {
        architectureId = QStringLiteral("efficientnet_b0");
        origin = QStringLiteral("pretrained");
    }

    QJsonObject projected = entry;
    if (!architectureId.isEmpty()) {
        projected = packagedModernModelRegistryEntry(architectureId, origin);
        for (const QString &key :
             {QStringLiteral("registry_entry_id"), QStringLiteral("display_name"),
              QStringLiteral("package_path"), QStringLiteral("active")}) {
            projected.insert(key, entry.value(key));
        }
    }

    QFile metadataFile(inspection.metadataPath);
    if (metadataFile.open(QIODevice::ReadOnly)) {
        const QJsonObject metadata =
            QJsonDocument::fromJson(metadataFile.readAll()).object();
        const QJsonArray classes =
            metadata.value(QStringLiteral("classes"))
                .toArray(metadata.value(QStringLiteral("class_ids")).toArray());
        if (!classes.isEmpty())
            projected.insert(QStringLiteral("classes"), classes);
        const QJsonObject displayLabels =
            metadata.value(QStringLiteral("display_labels")).toObject();
        if (!displayLabels.isEmpty())
            projected.insert(QStringLiteral("display_labels"), displayLabels);
        const QString createdAt =
            metadata.value(QStringLiteral("created_at")).toString();
        if (!createdAt.isEmpty())
            projected.insert(QStringLiteral("created_at"), createdAt);
    }
    return projected;
}

QString performanceLabel(const QJsonObject &entry, const QString &architectureId)
{
    Q_UNUSED(entry);
    if (architectureId == QStringLiteral("mobilenet_v3_small"))
        return QStringLiteral("Faster");
    if (architectureId == QStringLiteral("efficientnet_b0"))
        return QStringLiteral("More Accurate");
    return {};
}

QString classSummary(const QJsonObject &entry)
{
    QStringList labels;
    const QJsonObject displayLabels = entry.value(QStringLiteral("display_labels")).toObject();
    for (const QJsonValue &value : entry.value(QStringLiteral("classes")).toArray()) {
        const QString id = value.toString();
        labels.append(displayLabels.value(id).toString(id));
    }
    return labels.join(QStringLiteral(", "));
}

QVariantMap rowMap(const QJsonObject &entry)
{
    const ModelPackageInspection inspection = inspectModelPackage(entry);
    const QJsonObject projected = projectionEntry(entry, inspection);
    return {{QStringLiteral("id"), entryId(entry)},
            {QStringLiteral("name"), entryName(entry)},
            {QStringLiteral("architecture"), inspection.architectureId},
            {QStringLiteral("userFacingLabel"),
             registryString(projected, QStringLiteral("user_facing_label")).trimmed()},
            {QStringLiteral("performanceLabel"),
             performanceLabel(projected, inspection.architectureId)},
            {QStringLiteral("classSummary"), classSummary(projected)},
            {QStringLiteral("active"), entry.value(QStringLiteral("active")).toBool(false)},
            {QStringLiteral("status"), inspection.status},
            {QStringLiteral("message"), inspection.message},
            {QStringLiteral("canActivate"), inspection.canActivate}};
}

QString localPath(const QUrl &url)
{
    return url.isLocalFile() ? QFileInfo(url.toLocalFile()).absoluteFilePath()
                             : QString{};
}

QJsonObject packageMetadata(const QString &packagePath)
{
    QFile file(QDir(packagePath).filePath(QStringLiteral("metadata.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

bool recordedWeightIsValid(const QString &packagePath, const QJsonObject &metadata,
                           QString *weightPath = nullptr)
{
    const QJsonObject initialization =
        metadata.value(QStringLiteral("initialization")).toObject();
    const QString weightFile =
        initialization.value(QStringLiteral("weight_file")).toString().trimmed();
    const QString expectedHash =
        initialization.value(QStringLiteral("weight_sha256")).toString().trimmed();
    const QFileInfo weight(QDir(packagePath).filePath(weightFile));
    if (weightFile.isEmpty() || QFileInfo(weightFile).isAbsolute()
        || weight.fileName() != weightFile || !weight.isFile() || !weight.isReadable()
        || expectedHash.isEmpty()
        || fileSha256(weight.absoluteFilePath()).compare(
               expectedHash, Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (weightPath)
        *weightPath = weight.absoluteFilePath();
    return true;
}

bool imageNetTemplateIsValid(const QString &architecture, QString *packagePath = nullptr)
{
    const ModelPackageInspection inspection = inspectModelPackage(
        packagedModernModelRegistryEntry(architecture, QStringLiteral("blank")));
    const QJsonObject metadata = packageMetadata(inspection.packagePath);
    const QFileInfo weight(QDir(inspection.packagePath).filePath(
        QStringLiteral("imagenet_weights.pth")));
    const QString expectedHash =
        metadata.value(QStringLiteral("initialization")).toObject()
            .value(QStringLiteral("source_checkpoint_sha256")).toString().trimmed();
    if (metadata.value(QStringLiteral("status")).toString()
            != QStringLiteral("imagenet_transfer_start")
        || !weight.isFile() || expectedHash.isEmpty()
        || fileSha256(weight.absoluteFilePath()).compare(
               expectedHash, Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (packagePath)
        *packagePath = inspection.packagePath;
    return true;
}

bool packageCheckpointIsValid(const QString &packagePath, const QJsonObject &metadata,
                              QString *checkpointPath = nullptr,
                              bool migrateMissingHash = false)
{
    QJsonObject artifact = metadata.value(QStringLiteral("artifact")).toObject();
    const QString checkpointFile =
        artifact.value(QStringLiteral("checkpoint_file")).toString().trimmed();
    QString expectedHash =
        artifact.value(QStringLiteral("checkpoint_sha256")).toString().trimmed();
    const QFileInfo checkpoint(QDir(packagePath).filePath(checkpointFile));
    if (checkpointFile.isEmpty() || QFileInfo(checkpointFile).isAbsolute()
        || checkpoint.fileName() != checkpointFile || !checkpoint.isFile()
        || !checkpoint.isReadable()) {
        return false;
    }
    const QString actualHash = fileSha256(checkpoint.absoluteFilePath());
    if (actualHash.isEmpty())
        return false;
    if (expectedHash.isEmpty()) {
        if (!migrateMissingHash)
            return false;
        artifact.insert(QStringLiteral("checkpoint_sha256"), actualHash);
        QJsonObject migrated = metadata;
        migrated.insert(QStringLiteral("artifact"), artifact);
        QSaveFile metadataFile(
            QDir(packagePath).filePath(QStringLiteral("metadata.json")));
        if (!metadataFile.open(QIODevice::WriteOnly)
            || metadataFile.write(
                   QJsonDocument(migrated).toJson(QJsonDocument::Indented))
                   <= 0
            || !metadataFile.commit()) {
            return false;
        }
        expectedHash = packageMetadata(packagePath)
                           .value(QStringLiteral("artifact"))
                           .toObject()
                           .value(QStringLiteral("checkpoint_sha256"))
                           .toString()
                           .trimmed();
    }
    const bool valid =
        actualHash.compare(expectedHash, Qt::CaseInsensitive) == 0;
    if (valid && checkpointPath)
        *checkpointPath = checkpoint.absoluteFilePath();
    return valid;
}

bool pretrainedTemplateIsValid(const QString &architecture,
                               QString *packagePath = nullptr)
{
    const ModelPackageInspection inspection = inspectModelPackage(
        packagedModernModelRegistryEntry(architecture,
                                         QStringLiteral("pretrained")));
    const QJsonObject metadata = packageMetadata(inspection.packagePath);
    if (metadata.value(QStringLiteral("status")).toString()
            != QStringLiteral("trained")
        || !packageCheckpointIsValid(inspection.packagePath, metadata)) {
        return false;
    }
    if (packagePath)
        *packagePath = inspection.packagePath;
    return true;
}

QString architectureIdForIndex(int index)
{
    if (index == 0)
        return QStringLiteral("mobilenet_v3_small");
    if (index == 1)
        return QStringLiteral("efficientnet_b0");
    return {};
}

} // namespace

ModelLibraryController::ModelLibraryController(QString registryFilePath,
                                               OperationCoordinator &operations,
                                               QObject *parent)
    : QObject(parent)
    , operations_(operations)
    , registryFilePath_(std::move(registryFilePath))
{
    connect(&operations_, &OperationCoordinator::resourcesChanged,
            this, &ModelLibraryController::changed);
}

QVariantList ModelLibraryController::modelRows() const
{
    QVariantList rows;
    rows.reserve(entries_.size());
    for (const QJsonValue &value : entries_) {
        if (value.isObject())
            rows.append(rowMap(value.toObject()));
    }
    return rows;
}

int ModelLibraryController::selectedIndex() const
{
    return selectedIndex_;
}

QString ModelLibraryController::selectedId() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= entries_.size())
        return {};
    return entryId(entries_.at(selectedIndex_).toObject());
}

QVariantMap ModelLibraryController::selectedDetail() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= entries_.size())
        return {};

    const QJsonObject entry = entries_.at(selectedIndex_).toObject();
    const ModelPackageInspection inspection = inspectModelPackage(entry);
    const QJsonObject projected = projectionEntry(entry, inspection);
    return {{QStringLiteral("id"), entryId(entry)},
            {QStringLiteral("name"), entryName(entry)},
            {QStringLiteral("active"), entry.value(QStringLiteral("active")).toBool(false)},
            {QStringLiteral("architecture"), inspection.architectureId},
            {QStringLiteral("userFacingLabel"),
             registryString(projected, QStringLiteral("user_facing_label")).trimmed()},
            {QStringLiteral("performanceLabel"),
             performanceLabel(projected, inspection.architectureId)},
            {QStringLiteral("classCount"), inspection.classCount},
            {QStringLiteral("classSummary"), classSummary(projected)},
            {QStringLiteral("createdAt"),
             projected.value(QStringLiteral("created_at")).toString()},
            {QStringLiteral("packageLocation"), inspection.packagePath},
            {QStringLiteral("status"), inspection.status},
            {QStringLiteral("message"), inspection.message},
            {QStringLiteral("canActivate"), inspection.canActivate}};
}

QString ModelLibraryController::activeId() const
{
    for (const QJsonValue &value : entries_) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("active")).toBool(false))
            return entryId(entry);
    }
    return {};
}

QString ModelLibraryController::presentation() const
{
    if (!errorMessage_.isEmpty())
        return QStringLiteral("error");
    return entries_.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
}

QString ModelLibraryController::errorMessage() const
{
    return errorMessage_;
}

bool ModelLibraryController::operationInProgress() const
{
    return operationInProgress_;
}

bool ModelLibraryController::canImport() const
{
    return !operationInProgress_;
}

bool ModelLibraryController::canExport() const
{
    return !operationInProgress_ && !selectedPackagePath().isEmpty()
           && selectedPackageAvailable(ModelAccess::Read);
}

bool ModelLibraryController::canDuplicate() const
{
    return canExport();
}

bool ModelLibraryController::canDelete() const
{
    if (operationInProgress_ || selectedPackagePath().isEmpty())
        return false;
    return selectedPackageAvailable(ModelAccess::Write);
}

int ModelLibraryController::revision() const
{
    return revision_;
}

QVariantList ModelLibraryController::trainingModelRows() const
{
    QVariantList rows;
    for (const QJsonValue &value : entries_) {
        const QJsonObject entry = value.toObject();
        const ModelPackageInspection inspection = inspectModelPackage(entry);
        const QJsonObject metadata = packageMetadata(inspection.packagePath);
        const QJsonObject initialization =
            metadata.value(QStringLiteral("initialization")).toObject();
        const QString mode =
            initialization.value(QStringLiteral("mode")).toString();
        QString architectureId = inspection.architectureId;
        if (architectureId.isEmpty()) {
            const QJsonObject architecture =
                metadata.value(QStringLiteral("architecture")).toObject();
            const QString family =
                architecture.value(QStringLiteral("family")).toString();
            const QString variant =
                architecture.value(QStringLiteral("variant")).toString();
            if (family.compare(QStringLiteral("MobileNetV3"),
                               Qt::CaseInsensitive) == 0
                && variant.compare(QStringLiteral("small"),
                                   Qt::CaseInsensitive) == 0) {
                architectureId = QStringLiteral("mobilenet_v3_small");
            } else if (family.compare(QStringLiteral("EfficientNet"),
                                      Qt::CaseInsensitive) == 0
                       && variant.compare(QStringLiteral("b0"),
                                          Qt::CaseInsensitive) == 0) {
                architectureId = QStringLiteral("efficientnet_b0");
            }
        }
        QString weightPath;
        QString initializationMode;
        if (metadata.value(QStringLiteral("status")).toString()
            == QStringLiteral("library_identity")) {
            if ((mode == QStringLiteral("imagenet")
                 || mode == QStringLiteral("checkpoint"))
                && recordedWeightIsValid(inspection.packagePath, metadata,
                                         &weightPath)) {
                initializationMode = mode;
            }
        } else {
            const bool supportedArchitecture =
                architectureId == QStringLiteral("mobilenet_v3_small")
                || architectureId == QStringLiteral("efficientnet_b0");
            const bool trained =
                metadata.value(QStringLiteral("status")).toString()
                == QStringLiteral("trained");
            const bool checkpointHashMissing =
                metadata.value(QStringLiteral("artifact"))
                    .toObject()
                    .value(QStringLiteral("checkpoint_sha256"))
                    .toString()
                    .trimmed()
                    .isEmpty();
            bool checkpointValid = false;
            if (trained && supportedArchitecture && checkpointHashMissing) {
                auto lock = operations_.acquireModel(
                    inspection.packagePath, ModelAccess::Write);
                if (lock.acquired()) {
                    const QJsonObject lockedMetadata =
                        packageMetadata(inspection.packagePath);
                    checkpointValid = packageCheckpointIsValid(
                        inspection.packagePath, lockedMetadata, &weightPath,
                        true);
                }
            } else if (trained && supportedArchitecture) {
                checkpointValid = packageCheckpointIsValid(
                    inspection.packagePath, metadata, &weightPath);
            }
            if (checkpointValid) {
                initializationMode = QStringLiteral("checkpoint");
            }
        }
        rows.append(QVariantMap{
            {QStringLiteral("id"), entryId(entry)},
            {QStringLiteral("name"), entryName(entry)},
            {QStringLiteral("architecture"), architectureId},
            {QStringLiteral("startingWeights"),
             mode == QStringLiteral("imagenet")
                 ? QStringLiteral("ImageNet")
                 : initialization.value(QStringLiteral("source_model_name"))
                       .toString(QStringLiteral("Library checkpoint"))},
            {QStringLiteral("weightPath"), weightPath},
            {QStringLiteral("packagePath"), inspection.packagePath},
            {QStringLiteral("initializationMode"), initializationMode},
        });
    }
    return rows;
}

bool ModelLibraryController::refresh()
{
    const QString priorSelection = selectedId();
    QString warning;
    QJsonArray refreshed = readModelRegistryEntriesFromPath(registryFilePath_, &warning);
    entries_ = std::move(refreshed);
    ++revision_;
    selectedIndex_ = -1;
    for (int index = 0; index < entries_.size(); ++index) {
        if (!priorSelection.isEmpty()
            && entryId(entries_.at(index).toObject()).compare(
                   priorSelection, Qt::CaseInsensitive) == 0) {
            selectedIndex_ = index;
            break;
        }
    }
    errorMessage_ = warning;
    emit changed();
    return warning.isEmpty();
}

bool ModelLibraryController::select(int index)
{
    if (index < 0 || index >= entries_.size())
        return fail(QStringLiteral("Selected model is unavailable."));

    selectedIndex_ = index;
    errorMessage_.clear();
    emit changed();
    return true;
}

QStringList ModelLibraryController::startingWeightOptions(int architectureIndex) const
{
    const QString architecture = architectureIdForIndex(architectureIndex);
    if (architecture.isEmpty())
        return {};

    QStringList options;
    if (imageNetTemplateIsValid(architecture))
        options.append(QStringLiteral("ImageNet"));
    if (pretrainedTemplateIsValid(architecture))
        options.append(QStringLiteral("Pretrained"));
    return options;
}

bool ModelLibraryController::addModel(const QString &name, int architectureIndex,
                                      int startingWeightsIndex,
                                      const QUrl &destinationRootUrl)
{
    if (operationInProgress_)
        return fail(QStringLiteral("A Model Library operation is already in progress."));
    const QString architecture = architectureIdForIndex(architectureIndex);
    if (architecture.isEmpty())
        return fail(QStringLiteral(
            "Architecture must be MobileNetV3-Small or EfficientNet-B0."));
    if (name.trimmed().isEmpty())
        return fail(QStringLiteral("Model name is required."));

    const QStringList options = startingWeightOptions(architectureIndex);
    if (startingWeightsIndex < 0 || startingWeightsIndex >= options.size())
        return fail(QStringLiteral("Select approved Starting Weights."));

    QString sourcePackagePath;
    QString initializationMode;
    const QString selectedStartingWeights = options.at(startingWeightsIndex);
    if (selectedStartingWeights == QStringLiteral("ImageNet")) {
        if (!imageNetTemplateIsValid(architecture, &sourcePackagePath))
            return fail(QStringLiteral("The approved ImageNet Starting Weights are unavailable."));
        initializationMode = QStringLiteral("imagenet");
    } else if (selectedStartingWeights == QStringLiteral("Pretrained")) {
        if (!pretrainedTemplateIsValid(architecture, &sourcePackagePath))
            return fail(QStringLiteral(
                "The approved Pretrained Starting Weights are unavailable."));
        initializationMode = QStringLiteral("checkpoint");
    } else {
        return fail(QStringLiteral("Select approved Starting Weights."));
    }
    if (sourcePackagePath.isEmpty())
        return fail(QStringLiteral("Select approved Starting Weights."));

    setOperationInProgress(true);
    auto sourceLock = operations_.acquireModel(sourcePackagePath, ModelAccess::Read);
    if (!sourceLock.acquired()) {
        setOperationInProgress(false);
        return fail(sourceLock.fault
                        ? sourceLock.fault->reason
                        : QStringLiteral("The Starting Weights package is in use."));
    }

    QString createdId;
    QString createdPath;
    QString recoveryPath;
    QString error;
    const bool created = createLibraryModelIdentity(
        registryFilePath_, sourcePackagePath, name, architecture,
        initializationMode, localPath(destinationRootUrl), &createdId,
        &createdPath, &recoveryPath, &error);
    setOperationInProgress(false);
    if (!created)
        return fail(QStringLiteral("Add Model failed: ") + error);
    return refreshAndSelect(createdId);
}

bool ModelLibraryController::setActive()
{
    const QString id = selectedId();
    if (id.isEmpty())
        return fail(QStringLiteral("No model is selected."));
    if (id.compare(activeId(), Qt::CaseInsensitive) == 0)
        return fail(QStringLiteral("Selected model is already Active."));

    const QString selectedPath = selectedPackagePath();
    auto selectedLock = operations_.acquireModel(selectedPath, ModelAccess::Write);
    if (!selectedLock.acquired())
        return fail(selectedLock.fault
                        ? selectedLock.fault->reason
                        : QStringLiteral("The selected Model Package is in use."));
    const QString priorActiveId = activeId();
    ModelAcquireResult activeLock;
    bool activeLockRequired = false;
    if (!priorActiveId.isEmpty()) {
        for (const QJsonValue &value : entries_) {
            const QJsonObject entry = value.toObject();
            if (entryId(entry).compare(priorActiveId, Qt::CaseInsensitive) == 0) {
                const QString activePath = inspectModelPackage(entry).packagePath;
                if (activePath.compare(selectedPath, Qt::CaseInsensitive) != 0) {
                    activeLockRequired = true;
                    activeLock = operations_.acquireModel(activePath, ModelAccess::Write);
                }
                break;
            }
        }
        if (activeLockRequired && !activeLock.acquired() && !activeLock.fault.has_value()) {
            return fail(QStringLiteral("The Active Model Package is unavailable."));
        }
        if (activeLock.fault)
            return fail(activeLock.fault->reason);
    }
    QString error;
    if (!activateModelRegistryEntry(registryFilePath_, id, &error))
        return fail(error);
    return refresh();
}

bool ModelLibraryController::renameSelected(const QString &displayName)
{
    const QString id = selectedId();
    if (id.isEmpty())
        return fail(QStringLiteral("No model is selected."));

    auto lock = operations_.acquireModel(selectedPackagePath(), ModelAccess::Write);
    if (!lock.acquired())
        return fail(lock.fault
                        ? lock.fault->reason
                        : QStringLiteral("The selected Model Package is in use."));
    QString error;
    if (!renameRegistryEntryDisplayName(registryFilePath_, id, displayName, &error))
        return fail(error);
    return refresh();
}

bool ModelLibraryController::importModel(const QUrl &packageUrl)
{
    if (operationInProgress_)
        return fail(QStringLiteral("A Model Library operation is already in progress."));
    const QString metadataPath = localPath(packageUrl);
    if (metadataPath.isEmpty()
        || QFileInfo(metadataPath).fileName().compare(
               QStringLiteral("metadata.json"), Qt::CaseInsensitive) != 0
        || !QFileInfo(metadataPath).isFile()) {
        return fail(QStringLiteral(
            "Choose metadata.json inside a complete OpenDSS v2 Model Package."));
    }
    const QString packagePath = QFileInfo(metadataPath).absolutePath();

    setOperationInProgress(true);
    QString error;
    if (!validateCompleteV2ModelPackage(packagePath, &error)
        || !validateLoadablePackage(packagePath, &error)) {
        setOperationInProgress(false);
        return fail(error);
    }
    auto lock = operations_.acquireModel(packagePath, ModelAccess::Read);
    if (!lock.acquired()) {
        setOperationInProgress(false);
        return fail(lock.fault ? lock.fault->reason
                               : QStringLiteral("The Model Package is in use."));
    }

    QString importedId;
    QString importedPath;
    QString recoveryPath;
    const bool imported = importCompleteModelPackage(
        registryFilePath_, packagePath, &importedId, &importedPath, &recoveryPath, &error);
    setOperationInProgress(false);
    if (!imported)
        return fail(QStringLiteral("Import failed: ") + error);
    return refreshAndSelect(importedId);
}

bool ModelLibraryController::exportSelected(const QUrl &destinationRootUrl)
{
    if (operationInProgress_)
        return fail(QStringLiteral("A Model Library operation is already in progress."));
    const QString packagePath = selectedPackagePath();
    const QString destinationRoot = localPath(destinationRootUrl);
    if (packagePath.isEmpty())
        return fail(QStringLiteral("No model is selected."));
    if (destinationRoot.isEmpty())
        return fail(QStringLiteral("Choose a local export destination folder."));

    setOperationInProgress(true);
    auto lock = operations_.acquireModel(packagePath, ModelAccess::Read);
    if (!lock.acquired()) {
        setOperationInProgress(false);
        return fail(lock.fault ? lock.fault->reason
                               : QStringLiteral("The Model Package is in use."));
    }
    QString exportedPath;
    QString error;
    const bool exported =
        exportCompleteModelPackage(packagePath, destinationRoot, &exportedPath, &error);
    setOperationInProgress(false);
    if (!exported)
        return fail(QStringLiteral("Export failed: ") + error);
    errorMessage_.clear();
    emit changed();
    return true;
}

bool ModelLibraryController::duplicateSelected(const QString &displayName,
                                               const QUrl &destinationRootUrl)
{
    if (operationInProgress_)
        return fail(QStringLiteral("A Model Library operation is already in progress."));
    const QString packagePath = selectedPackagePath();
    const QString destinationRoot = localPath(destinationRootUrl);
    if (packagePath.isEmpty())
        return fail(QStringLiteral("No model is selected."));
    if (destinationRoot.isEmpty())
        return fail(QStringLiteral("Choose a local duplicate destination folder."));

    setOperationInProgress(true);
    auto lock = operations_.acquireModel(packagePath, ModelAccess::Read);
    if (!lock.acquired()) {
        setOperationInProgress(false);
        return fail(lock.fault ? lock.fault->reason
                               : QStringLiteral("The Model Package is in use."));
    }
    QString duplicatedId;
    QString duplicatedPath;
    QString recoveryPath;
    QString error;
    const bool duplicated = duplicateCompleteModelPackage(
        registryFilePath_, packagePath, displayName, destinationRoot,
        &duplicatedId, &duplicatedPath, &recoveryPath, &error);
    setOperationInProgress(false);
    if (!duplicated)
        return fail(QStringLiteral("Duplicate failed: ") + error);
    return refreshAndSelect(duplicatedId);
}

bool ModelLibraryController::deleteSelected()
{
    if (operationInProgress_)
        return fail(QStringLiteral("A Model Library operation is already in progress."));
    const QString id = selectedId();
    const QString packagePath = selectedPackagePath();
    if (id.isEmpty() || packagePath.isEmpty())
        return fail(QStringLiteral("No model is selected."));

    setOperationInProgress(true);
    auto lock = operations_.acquireModel(packagePath, ModelAccess::Write);
    if (!lock.acquired()) {
        setOperationInProgress(false);
        return fail(lock.fault ? lock.fault->reason
                               : QStringLiteral("The Model Package is in use."));
    }
    bool registryCommitted = false;
    bool deletedActive = false;
    QString recoveryPath;
    QString error;
    const bool deleted = deleteRegisteredModelPackage(
        registryFilePath_, id, &registryCommitted, &deletedActive, &recoveryPath, &error);
    setOperationInProgress(false);
    if (registryCommitted) {
        refresh();
        if (deletedActive && activeModelCleared_)
            activeModelCleared_();
    }
    if (!deleted)
        return fail(QStringLiteral("Remove Model failed: ") + error);
    errorMessage_ = error;
    emit changed();
    return true;
}

void ModelLibraryController::setActiveModelClearedCallback(std::function<void()> callback)
{
    activeModelCleared_ = std::move(callback);
    emit changed();
}

bool ModelLibraryController::selectedPackageAvailable(ModelAccess access) const
{
    auto lock = operations_.acquireModel(selectedPackagePath(), access);
    return lock.acquired();
}

QString ModelLibraryController::selectedPackagePath() const
{
    if (selectedIndex_ < 0 || selectedIndex_ >= entries_.size())
        return {};
    return inspectModelPackage(entries_.at(selectedIndex_).toObject()).packagePath;
}

bool ModelLibraryController::validateLoadablePackage(const QString &packagePath,
                                                     QString *error) const
{
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        if (error)
            *error = QStringLiteral("Could not create temporary package validation state.");
        return false;
    }
    const QString validationId = QStringLiteral("package-import-validation");
    const QString validationRegistry =
        QDir(temporary.path()).filePath(QStringLiteral("model_registry.json"));
    const QJsonObject registry{
        {QStringLiteral("schema_version"), QStringLiteral("model-registry-v3-simple")},
        {QStringLiteral("entries"),
         QJsonArray{QJsonObject{{QStringLiteral("registry_entry_id"), validationId},
                                {QStringLiteral("display_name"),
                                 QStringLiteral("Package validation")},
                                {QStringLiteral("package_path"), packagePath},
                                {QStringLiteral("active"), false}}}}};
    QSaveFile file(validationRegistry);
    const QByteArray bytes = QJsonDocument(registry).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
        || !file.commit()) {
        if (error)
            *error = QStringLiteral("Could not prepare temporary package validation state.");
        return false;
    }
    ModelLoadService loader(validationRegistry);
    QString warning;
    return static_cast<bool>(loader.prepare(validationId, QStringLiteral("cpu"),
                                            &warning, error));
}

bool ModelLibraryController::refreshAndSelect(const QString &entryIdToSelect)
{
    if (!refresh())
        return false;
    for (int index = 0; index < entries_.size(); ++index) {
        if (entryId(entries_.at(index).toObject()).compare(
                entryIdToSelect, Qt::CaseInsensitive) == 0)
            return select(index);
    }
    return fail(QStringLiteral("The completed Model Package was not found after refresh."));
}

void ModelLibraryController::setOperationInProgress(bool inProgress)
{
    if (operationInProgress_ == inProgress)
        return;
    operationInProgress_ = inProgress;
    if (inProgress)
        errorMessage_.clear();
    emit changed();
}

bool ModelLibraryController::fail(const QString &message)
{
    errorMessage_ = message;
    emit changed();
    return false;
}

} // namespace desktop_app::v2
