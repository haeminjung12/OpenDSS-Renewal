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

    ClassificationResult classify(const cv::Mat& input) const;
    const Metadata& metadata() const noexcept;
    const std::string& modelId() const noexcept;
    const std::string& modelPath() const noexcept;
    const std::string& metadataPath() const noexcept;
    std::string executionProvider() const;

private:
    std::string modelId_;
    std::string modelPath_;
    std::string metadataPath_;
    Metadata metadata_;
    OnnxClassifier classifier_;
};
