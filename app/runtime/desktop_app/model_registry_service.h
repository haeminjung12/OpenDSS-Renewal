#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct DefaultWorkspacePaths {
    QString root;
    QString models;
    QString datasets;
    QString preparedDatasets;
    QString runs;
    QString trainingRuns;
    QString validationRuns;
    QString activeModel;
    QString activeMetadata;
    QString preparedDataset;
    QString preparedDatasetManifest;
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

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath);
QString modelRegistryPath();
QJsonObject temporaryStaticModelRegistry();
QJsonObject loadModelRegistry(QString* loadedPath = nullptr, QString* loadWarning = nullptr);
QString registryString(const QJsonObject& entry, const QString& key);
QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key);
bool registerTrainedModelArtifacts(const QString& registryFilePath, const QString& runDir,
                                   const QString& modelOnnxPath, const QString& metadataJsonPath,
                                   QString* registeredEntryId = nullptr, QString* error = nullptr);
bool updateModelRegistryImageValidationSummary(const QString& registryFilePath, const QString& validationSummaryPath,
                                               QString* updatedEntryId = nullptr, QString* error = nullptr);
bool renameRegistryEntryDisplayName(const QString& registryFilePath, const QString& registryEntryId,
                                    const QString& displayName, QString* error = nullptr);
QString runtimePathFromRegistryPath(const QString& path);
QString registryEntrySummary(const QJsonObject& entry, const QString& registryPath, const QString& warning);
QString resolvePackagedPathFromRegistryPath(const QString& registryPath);
QJsonObject activeRegistryEntry(const QJsonArray& entries);
DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries);
