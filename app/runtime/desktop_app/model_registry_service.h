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

QJsonObject packagedPromotedModelRegistryEntry();
QJsonObject packagedBlankModelRegistryEntry();
QJsonObject packagedPretrainedModelRegistryEntry();

QString runtimeModelArtifactPath(const QString& projectRoot, const QString& relativePath);
QString modelRegistryPath();
QJsonObject temporaryStaticModelRegistry();
QJsonObject loadModelRegistry(QString* loadedPath = nullptr, QString* loadWarning = nullptr);
QString registryString(const QJsonObject& entry, const QString& key);
QString registryNestedString(const QJsonObject& entry, const QString& objectKey, const QString& key);
QString runtimePathFromRegistryPath(const QString& path);
QString registryEntrySummary(const QJsonObject& entry, const QString& registryPath, const QString& warning);
QString resolvePackagedPathFromRegistryPath(const QString& registryPath);
QJsonObject activeRegistryEntry(const QJsonArray& entries);
DefaultWorkspacePaths ensureDefaultWorkspaceAssets(const QJsonArray& registryEntries);
