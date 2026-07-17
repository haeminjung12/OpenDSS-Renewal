#pragma once

#include <functional>

#include <QString>

struct CollectionPostprocessOptions {
    QString sessionDir;
    QString collectionName;
    QString collectionsRoot;
    QString preparedDatasetsRoot;
    bool createTrainingMetadata = true;
};

struct CollectionPostprocessResult {
    bool ok = false;
    QString errorMessage;
    QString collectionDir;
    QString datasetDir;
    QString datasetManifestPath;
    int framesRead = 0;
    int detectedRows = 0;
    int rawCropsWritten = 0;
    int resizedCropsWritten = 0;
};

using CollectionPostprocessProgress = std::function<void(int value, int maximum, const QString& message)>;

CollectionPostprocessResult postprocessCollectionForTraining(const CollectionPostprocessOptions& options,
                                                             CollectionPostprocessProgress progress = {});

