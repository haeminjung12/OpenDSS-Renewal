#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct DefaultWorkspacePaths {
    QString root;
    QString collections;
    QString models;
    QString datasets;
    QString preparedDatasets;
    QString runs;
    QString trainingRuns;
    QString validationRuns;
    QString reports;
    QString activeModel;
    QString activeMetadata;
    QString preparedDataset;
    QString preparedDatasetManifest;
};

struct ActiveModelReadiness {
    bool ready = false;
    QString missingItem;
    QString message;
};

struct ModelPackageInspection {
    QString status;
    QString message;
    QString packagePath;
    QString metadataPath;
    QString checkpointPath;
    QString onnxPath;
    QString architectureId;
    int classCount = 0;
    bool canTrain = false;
    bool canActivate = false;
};

QString defaultOpenDssRootPath();
QString defaultOpenDssModelsPath();
QString defaultOpenDssDatasetsPath();
QString defaultOpenDssPreparedDatasetsPath();
QString defaultOpenDssRunsPath();
QString defaultOpenDssTrainingRunsPath();
QString defaultOpenDssValidationRunsPath();
QString findPackagedAppPath(const QString& relativePath);
QString chooseOpenFileDialogPath(const QString& currentPath, const QString& workspacePath,
                                 const QString& packagedPath = QString());
QString chooseExistingDirectoryDialogPath(const QString& currentPath, const QString& workspacePath,
                                          const QString& packagedPath = QString());
bool isDeveloperInternalDefaultPath(const QString& path);

QJsonObject packagedPromotedModelRegistryEntry();
QJsonObject packagedBlankModelRegistryEntry();
QJsonObject packagedPretrainedModelRegistryEntry();
QJsonObject packagedModernModelRegistryEntry(const QString& architectureId, const QString& origin);
QJsonArray packagedModernModelRegistryEntries(const QString& origin);
QString packagedModelEntryAvailabilityError(const QJsonObject& entry);

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath);
QString modelRegistryPath();
QJsonObject temporaryStaticModelRegistry();
QJsonObject loadModelRegistry(QString* loadedPath = nullptr, QString* loadWarning = nullptr);
QJsonArray readModelRegistryEntriesFromPath(const QString& registryFilePath, QString* warning = nullptr);
QString registryString(const QJsonObject& entry, const QString& key);
QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key);
bool registerTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                                   const QString& modelOnnxPath, const QString& metadataJsonPath,
                                   QString* registeredEntryId = nullptr, QString* error = nullptr);
bool saveTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                               const QString& modelOnnxPath, const QString& metadataJsonPath,
                               const QString& metricsCsvPath, const QString& trainingConfigJsonPath,
                               const QString& metricsJsonPath, const QString& classMetricsCsvPath,
                               const QString& confusionMatrixCsvPath, const QString& modelName,
                               QString* registeredEntryId = nullptr, QString* error = nullptr);
bool saveTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                               const QString& modelOnnxPath, const QString& metadataJsonPath,
                               const QString& metricsCsvPath, const QString& trainingConfigJsonPath,
                               const QString& metricsJsonPath, const QString& classMetricsCsvPath,
                               const QString& confusionMatrixCsvPath, const QString& modelName,
                               const QString& destinationRoot, QString* savedPackagePath = nullptr,
                               QString* registeredEntryId = nullptr, QString* error = nullptr,
                               const QString& replaceRegistryEntryId = {});
ActiveModelReadiness evaluateActiveModelReadiness(const QJsonObject& entry);
ModelPackageInspection inspectModelPackage(const QJsonObject& entry);
bool activateModelRegistryEntry(const QString& registryFilePath, const QString& registryEntryId,
                                QString* error = nullptr);
bool updateModelRegistryImageValidationSummary(const QString& registryFilePath, const QString& validationSummaryPath,
                                               QString* updatedEntryId = nullptr, QString* error = nullptr);
bool renameRegistryEntryDisplayName(const QString& registryFilePath, const QString& registryEntryId,
                                    const QString& displayName, QString* error = nullptr);
bool validateCompleteV2ModelPackage(const QString& packagePath, QString* error = nullptr);
bool importCompleteModelPackage(const QString& registryFilePath, const QString& sourcePackagePath,
                                QString* importedEntryId = nullptr, QString* importedPackagePath = nullptr,
                                QString* recoveryPath = nullptr, QString* error = nullptr);
bool exportCompleteModelPackage(const QString& sourcePackagePath, const QString& destinationRoot,
                                QString* exportedPackagePath = nullptr, QString* error = nullptr);
bool duplicateCompleteModelPackage(const QString& registryFilePath, const QString& sourcePackagePath,
                                   const QString& displayName, const QString& destinationRoot,
                                   QString* duplicatedEntryId = nullptr, QString* duplicatedPackagePath = nullptr,
                                   QString* recoveryPath = nullptr, QString* error = nullptr);
bool createLibraryModelIdentity(const QString& registryFilePath, const QString& sourcePackagePath,
                                const QString& displayName, const QString& architectureId,
                                const QString& initializationMode, const QString& destinationRoot,
                                QString* createdEntryId = nullptr,
                                QString* createdPackagePath = nullptr,
                                QString* recoveryPath = nullptr, QString* error = nullptr);
bool deleteRegisteredModelPackage(const QString& registryFilePath, const QString& registryEntryId,
                                  bool* registryCommitted = nullptr, bool* deletedActive = nullptr,
                                  QString* recoveryPath = nullptr, QString* error = nullptr);
QString runtimePathFromRegistryPath(const QString& path);
QString registryEntrySummary(const QJsonObject& entry, const QString& registryPath, const QString& warning);
QString resolvePackagedPathFromRegistryPath(const QString& registryPath);
QJsonObject activeRegistryEntry(const QJsonArray& entries);
bool repairTrustedPretrainedMetadataHash(const QString& installedPackagePath,
                                         const QString& trustedPackagePath,
                                         QString* error = nullptr);
DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries);
