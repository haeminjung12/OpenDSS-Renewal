#pragma once

#include "../../inference/onnx_inference_adapter.h"

#include <QString>

#include <memory>

class PipelineRunner;

namespace desktop_app::v2 {

class ModelLoadService {
public:
    explicit ModelLoadService(QString registryFilePath);

    std::unique_ptr<OnnxInferenceAdapter> prepare(const QString& registryEntryId,
                                                  const QString& requestedDevice,
                                                  QString* warning = nullptr,
                                                  QString* error = nullptr) const;
    std::unique_ptr<OnnxInferenceAdapter> preparePersistedActive(const QString& requestedDevice,
                                                                 QString* warning = nullptr,
                                                                 QString* error = nullptr) const;
    bool activateAndInstall(std::unique_ptr<OnnxInferenceAdapter> candidate,
                            PipelineRunner& pipeline,
                            QString* error = nullptr) const;
    void installPersisted(std::unique_ptr<OnnxInferenceAdapter> candidate,
                          PipelineRunner& pipeline) const noexcept;

private:
    QString registryFilePath_;
};

} // namespace desktop_app::v2
