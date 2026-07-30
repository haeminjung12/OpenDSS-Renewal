#pragma once

#include "../metadata_loader.h"
#include "../onnx_classifier.h"

#include <memory>
#include <string>
#include <vector>

class OnnxInferenceAdapter {
public:
    bool load(const std::string& modelId, const std::string& modelPath, const std::string& metadataPath,
              const Metadata& metadata, const std::string& requestedDevice, std::string& message);
    bool setSortingPolicy(const std::string& targetClassId, const std::string& targetDisplayLabel,
                          const std::string& triggerRule, std::string& message);
    void setArtifactIdentity(std::string declaredOnnxSha256, std::string metadataSha256);

    ClassificationResult classify(const cv::Mat& input) const;
    const Metadata& metadata() const noexcept;
    const std::string& modelId() const noexcept;
    const std::string& modelPath() const noexcept;
    const std::string& metadataPath() const noexcept;
    const std::string& declaredOnnxSha256() const noexcept;
    const std::string& metadataSha256() const noexcept;
    const std::string& sortingTargetClassId() const noexcept;
    const std::string& sortingTargetDisplayLabel() const noexcept;
    const std::string& sortingTriggerRule() const noexcept;
    std::string executionProvider() const;

private:
    std::string modelId_;
    std::string modelPath_;
    std::string metadataPath_;
    std::string declaredOnnxSha256_;
    std::string metadataSha256_;
    std::string sortingTargetClassId_;
    std::string sortingTargetDisplayLabel_;
    std::string sortingTriggerRule_;
    Metadata metadata_;
    OnnxClassifier classifier_;
};
