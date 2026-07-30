#pragma once

#include "../../inference/onnx_inference_adapter.h"

#include <QString>
#include <QVector>

#include <memory>

class PipelineRunner;

namespace desktop_app::v2 {

struct PersistedActiveModelClass {
    QString id;
    QString displayLabel;
};

struct PersistedActiveModelInspection {
    bool loadable = false;
    QString id;
    QString displayName;
    QString modelSha256;
    QVector<PersistedActiveModelClass> classes;
    int classCount = 0;
    QString plannedDevice;
    QString error;
};

struct PersistedActiveCheckpointInspection {
    bool loadable = false;
    QString id;
    QString displayName;
    QString checkpointPath;
    QString checkpointSha256;
    QString metadataSha256;
    QVector<PersistedActiveModelClass> classes;
    QString error;
};

class ModelLoadService {
public:
    explicit ModelLoadService(QString registryFilePath);

    PersistedActiveModelInspection inspectPersistedActive() const;
    PersistedActiveCheckpointInspection
    inspectAndMigratePersistedActiveCheckpoint() const;
    std::unique_ptr<OnnxInferenceAdapter> prepare(const QString& registryEntryId,
                                                  const QString& requestedDevice,
                                                  QString* warning = nullptr,
                                                  QString* error = nullptr) const;
    std::unique_ptr<OnnxInferenceAdapter> preparePersistedActive(const QString& requestedDevice,
                                                                 QString* warning = nullptr,
                                                                 QString* error = nullptr) const;
    std::unique_ptr<OnnxInferenceAdapter> preparePersistedActive(
        const QString& requestedDevice, QString* warning, QString* error,
        QString* activeDisplayName) const;
    bool activateAndInstall(std::unique_ptr<OnnxInferenceAdapter> candidate,
                            PipelineRunner& pipeline,
                            QString* error = nullptr) const;
    bool saveAndActivateTrainedModel(const QString& runDir,
                                    const QString& modelOnnxPath,
                                    const QString& metadataJsonPath,
                                    const QString& modelName,
                                    const QString& destinationRoot,
                                    const QString& requestedDevice,
                                    PipelineRunner& pipeline,
                                    QString* registeredEntryId = nullptr,
                                    QString* warning = nullptr,
                                    QString* error = nullptr,
                                    const QString& libraryEntryId = {}) const;
    void installPersisted(std::unique_ptr<OnnxInferenceAdapter> candidate,
                          PipelineRunner& pipeline) const noexcept;

private:
    QString registryFilePath_;
};

} // namespace desktop_app::v2
