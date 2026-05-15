#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct DefaultWorkspacePaths {
    QString root;
    QString models;
    QString datasets;
    QString runs;
    QString activeModel;
    QString activeMetadata;
    QString preparedDataset;
};

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
