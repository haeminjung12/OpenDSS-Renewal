#pragma once

#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "onnxruntime_cxx_api.h"
#include "metadata_loader.h"

struct ClassificationResult {
    int index = -1;
    std::string label;
    std::vector<float> scores;
};

class OnnxClassifier {
  public:
    OnnxClassifier();

    bool init(const std::string& modelPath, const Metadata& meta, const std::string& requestedDevice, std::string& err);
    bool init(const std::string& modelPath, const Metadata& meta, bool preferCuda, std::string& err);
    bool isReady() const;
    ClassificationResult classify(const cv::Mat& input) const;
    std::string executionProvider() const;

  private:
    void preprocess(const cv::Mat& input, std::vector<float>& out) const;
    void setupNormalizationLuts();

    Metadata meta_;
    bool ready_ = false;
    bool useCuda_ = false;
    std::string lastWarning_;

    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string inputName_;
    std::string outputName_;
    std::vector<int64_t> inputShape_;
    mutable cv::Mat preprocessFloat_;
    mutable cv::Mat resizedFloat_;
    mutable std::vector<float> preprocessBuffer_;
    mutable std::vector<float> normMean_;
    mutable std::vector<float> normScale_;
    mutable std::vector<float> outputBuffer_;
};
