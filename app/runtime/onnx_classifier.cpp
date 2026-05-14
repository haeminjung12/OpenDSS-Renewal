#include "onnx_classifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <opencv2/imgproc.hpp>
#include "onnxruntime_cxx_api.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
#ifdef _WIN32
std::wstring widenPath(const std::string& path) {
    if (path.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return std::wstring(path.begin(), path.end());
    }
    std::wstring wide(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), size);
    return wide;
}
#endif
} // namespace

OnnxClassifier::OnnxClassifier()
    : ready_(false) {}

void OnnxClassifier::setupNormalizationLuts() {
    normMean_.assign(meta_.inputC, 0.0f);
    normScale_.assign(meta_.inputC, 1.0f);

    for (int c = 0; c < meta_.inputC; ++c) {
        int mi = std::min(c, static_cast<int>(meta_.mean.size()) - 1);
        int si = std::min(c, static_cast<int>(meta_.std.size()) - 1);
        float mean = meta_.mean[std::max(0, mi)];
        float stdv = meta_.std[std::max(0, si)];
        normMean_[c] = mean;
        if (std::abs(stdv) > std::numeric_limits<float>::epsilon()) {
            normScale_[c] = 1.0f / stdv;
        }
    }
}

bool OnnxClassifier::init(const std::string& modelPath, const Metadata& meta, bool preferCuda, std::string& err) {
    meta_ = meta;
    if (meta_.inputH <= 0 || meta_.inputW <= 0) {
        err = "invalid input_size in metadata";
        return false;
    }
    if (meta_.inputC <= 0) meta_.inputC = 1;
    if (meta_.mean.empty()) meta_.mean.assign(meta_.inputC, 0.0f);
    if (meta_.std.empty()) meta_.std.assign(meta_.inputC, 1.0f);

    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "droplet");
    useCuda_ = false;

    auto createSessionWithOptions = [&](bool useCuda, std::string* outErr) -> std::unique_ptr<Ort::Session> {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

        if (useCuda) {
            try {
                OrtCUDAProviderOptions cudaOptions{};
                cudaOptions.device_id = 0;
                opts.AppendExecutionProvider_CUDA(cudaOptions);
            } catch (...) {
                if (outErr) {
                    *outErr = "CUDA provider unavailable or failed to initialize; falling back to CPU";
                }
                return {};
            }
        }

        try {
#ifdef _WIN32
            std::wstring widePath = widenPath(modelPath);
            return std::make_unique<Ort::Session>(*env_, widePath.c_str(), opts);
#else
            return std::make_unique<Ort::Session>(*env_, modelPath.c_str(), opts);
#endif
        } catch (const Ort::Exception&) {
            if (outErr) {
                *outErr = "failed to create ONNX session";
            }
            return {};
        }
    };

    bool usingCuda = false;
    std::string sessionErr;
    session_ = createSessionWithOptions(preferCuda, &sessionErr);
    usingCuda = preferCuda && session_;
    if (!session_ && preferCuda) {
        err = sessionErr;
        std::string cpuErr;
        session_ = createSessionWithOptions(false, &cpuErr);
        sessionErr = cpuErr;
    }
    if (!session_) {
        err = sessionErr.empty() ? "failed to create ONNX session" : sessionErr;
        return false;
    }
    if (preferCuda && !usingCuda) {
        err.clear();
    }

    useCuda_ = usingCuda;

    Ort::AllocatorWithDefaultOptions allocator;
    try {
        inputName_ = session_->GetInputNameAllocated(0, allocator).get();
        outputName_ = session_->GetOutputNameAllocated(0, allocator).get();
    } catch (const Ort::Exception& e) {
        err = e.what();
        return false;
    }
    inputShape_ = {1, meta_.inputC, meta_.inputH, meta_.inputW};
    preprocessBuffer_.assign(static_cast<size_t>(meta_.inputC * meta_.inputH * meta_.inputW), 0.0f);
    setupNormalizationLuts();
    ready_ = true;
    return true;
}

bool OnnxClassifier::isReady() const {
    return ready_;
}

std::string OnnxClassifier::executionProvider() const {
    return useCuda_ ? "CUDA" : "CPU";
}

void OnnxClassifier::preprocess(const cv::Mat& input, std::vector<float>& out) const {
    cv::Mat img = input;
    if (img.channels() == 1 && meta_.inputC == 3) {
        cv::cvtColor(img, img, cv::COLOR_GRAY2RGB);
    } else if (img.channels() == 3 && meta_.inputC == 1) {
        cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
    }

    resizedFloat_.create(meta_.inputH, meta_.inputW, img.type());
    cv::resize(img, resizedFloat_, cv::Size(meta_.inputW, meta_.inputH), 0, 0, cv::INTER_LINEAR);

    if (resizedFloat_.depth() == CV_16U) {
        resizedFloat_.convertTo(preprocessFloat_, CV_32F, 1.0 / 65535.0);
    } else if (resizedFloat_.depth() == CV_8U) {
        resizedFloat_.convertTo(preprocessFloat_, CV_32F, 1.0 / 255.0);
    } else if (resizedFloat_.depth() != CV_32F) {
        resizedFloat_.convertTo(preprocessFloat_, CV_32F);
    } else {
        preprocessFloat_ = resizedFloat_;
    }

    const size_t required = static_cast<size_t>(meta_.inputC * meta_.inputH * meta_.inputW);
    out.resize(required);
    const int hw = meta_.inputH * meta_.inputW;
    const int inChannels = preprocessFloat_.channels();

    if (meta_.inputC == 1) {
        const float mean = normMean_.empty() ? 0.0f : normMean_[0];
        const float scale = normScale_.empty() ? 1.0f : normScale_[0];
        for (int y = 0; y < meta_.inputH; ++y) {
            const float* row = preprocessFloat_.ptr<float>(y);
            float* outRow = out.data() + static_cast<size_t>(y * meta_.inputW);
            for (int x = 0; x < meta_.inputW; ++x) {
                outRow[x] = (row[x] - mean) * scale;
            }
        }
        return;
    }

    for (int c = 0; c < meta_.inputC; ++c) {
        const float mean = normMean_[std::min(c, static_cast<int>(normMean_.size()) - 1)];
        const float scale = normScale_[std::min(c, static_cast<int>(normScale_.size()) - 1)];
        const size_t outBase = static_cast<size_t>(c) * static_cast<size_t>(hw);
        for (int y = 0; y < meta_.inputH; ++y) {
            const float* row = preprocessFloat_.ptr<float>(y);
            float* outRow = out.data() + outBase + static_cast<size_t>(y * meta_.inputW);
            for (int x = 0; x < meta_.inputW; ++x) {
                const float* pix = row + static_cast<size_t>(x) * inChannels;
                int ci = std::min(c, inChannels - 1);
                outRow[x] = (pix[ci] - mean) * scale;
            }
        }
    }
}

ClassificationResult OnnxClassifier::classify(const cv::Mat& input) const {
    ClassificationResult result;
    if (!ready_ || !session_) return result;

    preprocess(input, preprocessBuffer_);
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(
        memInfo,
        preprocessBuffer_.data(),
        preprocessBuffer_.size(),
        inputShape_.data(),
        inputShape_.size());

    std::array<const char*, 1> inputNames = {inputName_.c_str()};
    std::array<const char*, 1> outputNames = {outputName_.c_str()};

    auto outputs = session_->Run(Ort::RunOptions{nullptr},
                                 inputNames.data(),
                                 &tensor,
                                 1,
                                 outputNames.data(),
                                 1);
    if (outputs.empty()) return result;

    const auto& out = outputs[0];
    auto info = out.GetTensorTypeAndShapeInfo();
    const size_t count = info.GetElementCount();
    const float* scores = out.GetTensorData<float>();
    outputBuffer_.resize(count);
    if (count > 0) {
        std::copy(scores, scores + count, outputBuffer_.begin());
    }
    result.scores = outputBuffer_;
    auto bestIt = std::max_element(result.scores.begin(), result.scores.end());
    if (bestIt != result.scores.end()) {
        result.index = static_cast<int>(std::distance(result.scores.begin(), bestIt));
        if (result.index >= 0 && result.index < static_cast<int>(meta_.classes.size())) {
            result.label = meta_.classes[result.index];
        }
    }
    return result;
}
