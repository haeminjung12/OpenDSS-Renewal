#include "onnx_inference_adapter.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <unordered_set>

bool OnnxInferenceAdapter::load(const std::string& modelId, const std::string& modelPath,
                                const std::string& metadataPath, const Metadata& metadata,
                                const std::string& requestedDevice, std::string& message) {
    if (metadata.classes.empty()) {
        message = "metadata classes are empty";
        return false;
    }
    std::unordered_set<std::string> classIds;
    for (const std::string& classId : metadata.classes) {
        if (classId.empty() || !classIds.insert(classId).second) {
            message = "metadata classes contain an empty or duplicate id";
            return false;
        }
    }
    if (metadata.inputH <= 0 || metadata.inputW <= 0 || (metadata.inputC != 1 && metadata.inputC != 3)) {
        message = "metadata input_size must declare positive height/width and one or three channels";
        return false;
    }
    const auto validNormalization = [channels = metadata.inputC](const std::vector<float>& values, bool standardDeviation) {
        if (values.size() != 1 && values.size() != static_cast<std::size_t>(channels))
            return false;
        for (float value : values) {
            if (!std::isfinite(value) || (standardDeviation && value == 0.0f))
                return false;
        }
        return true;
    };
    if (!validNormalization(metadata.mean, false) || !validNormalization(metadata.std, true)) {
        message = "metadata normalization must contain finite mean/std values for one or every input channel";
        return false;
    }

    Metadata candidateMetadata = metadata;
    OnnxClassifier candidateClassifier;
    std::string classifierMessage;
    if (!candidateClassifier.init(modelPath, candidateMetadata, requestedDevice, classifierMessage)) {
        message = classifierMessage;
        return false;
    }

    try {
        const int type = candidateMetadata.inputC == 3 ? CV_8UC3 : CV_8UC1;
        const cv::Mat smokeInput = cv::Mat::zeros(candidateMetadata.inputH, candidateMetadata.inputW, type);
        const ClassificationResult smokeResult = candidateClassifier.classify(smokeInput);
        if (smokeResult.scores.empty()) {
            message = "ONNX smoke inference returned no scores";
            return false;
        }
        if (smokeResult.scores.size() != candidateMetadata.classes.size()) {
            message = "ONNX output count does not match metadata classes";
            return false;
        }
        for (float score : smokeResult.scores) {
            if (!std::isfinite(score)) {
                message = "ONNX smoke inference returned a nonfinite score";
                return false;
            }
        }
        if (smokeResult.index < 0 ||
            smokeResult.index >= static_cast<int>(candidateMetadata.classes.size()) ||
            smokeResult.label != candidateMetadata.classes[static_cast<std::size_t>(smokeResult.index)]) {
            message = "ONNX smoke inference returned an invalid predicted class";
            return false;
        }
    } catch (const std::exception& exception) {
        message = std::string("ONNX smoke inference failed: ") + exception.what();
        return false;
    } catch (...) {
        message = "ONNX smoke inference failed";
        return false;
    }

    modelId_ = modelId;
    modelPath_ = modelPath;
    metadataPath_ = metadataPath;
    metadata_ = std::move(candidateMetadata);
    classifier_ = std::move(candidateClassifier);
    message = classifierMessage;
    return true;
}

bool OnnxInferenceAdapter::setSortingPolicy(const std::string& targetClassId,
                                            const std::string& targetDisplayLabel,
                                            const std::string& triggerRule,
                                            std::string& message) {
    if (targetClassId.empty() ||
        std::find(metadata_.classes.begin(), metadata_.classes.end(), targetClassId) == metadata_.classes.end()) {
        message = "sorting_policy target_class_id is not present in metadata classes";
        return false;
    }
    if (triggerRule != "trigger_on_target_class") {
        message = "sorting_policy trigger_rule is not supported";
        return false;
    }
    const std::string metadataDisplayLabel = DisplayLabelForClassId(metadata_, targetClassId);
    if (targetDisplayLabel.empty() || targetDisplayLabel != metadataDisplayLabel) {
        message = "sorting_policy target_display_label does not match metadata display_labels";
        return false;
    }
    sortingTargetClassId_ = targetClassId;
    sortingTargetDisplayLabel_ = targetDisplayLabel;
    sortingTriggerRule_ = triggerRule;
    return true;
}

void OnnxInferenceAdapter::setArtifactIdentity(std::string declaredOnnxSha256, std::string metadataSha256) {
    declaredOnnxSha256_ = std::move(declaredOnnxSha256);
    metadataSha256_ = std::move(metadataSha256);
}

ClassificationResult OnnxInferenceAdapter::classify(const cv::Mat& input) const {
    return classifier_.classify(input);
}

const Metadata& OnnxInferenceAdapter::metadata() const noexcept {
    return metadata_;
}

const std::string& OnnxInferenceAdapter::modelId() const noexcept {
    return modelId_;
}

const std::string& OnnxInferenceAdapter::modelPath() const noexcept {
    return modelPath_;
}

const std::string& OnnxInferenceAdapter::metadataPath() const noexcept {
    return metadataPath_;
}

const std::string& OnnxInferenceAdapter::declaredOnnxSha256() const noexcept {
    return declaredOnnxSha256_;
}

const std::string& OnnxInferenceAdapter::metadataSha256() const noexcept {
    return metadataSha256_;
}

const std::string& OnnxInferenceAdapter::sortingTargetClassId() const noexcept {
    return sortingTargetClassId_;
}

const std::string& OnnxInferenceAdapter::sortingTargetDisplayLabel() const noexcept {
    return sortingTargetDisplayLabel_;
}

const std::string& OnnxInferenceAdapter::sortingTriggerRule() const noexcept {
    return sortingTriggerRule_;
}

std::string OnnxInferenceAdapter::executionProvider() const {
    return classifier_.executionProvider();
}
