#include "pipeline_runner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "../detection/droplet_detector_adapters.h"

namespace fs = std::filesystem;
namespace {
std::string sanitizeLabel(const std::string& label) {
    std::string out;
    out.reserve(label.size());
    for (unsigned char c : label) {
        if (std::isalnum(c) || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('_');
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "unclassified";
    }
    return out;
}
} // namespace

PipelineRunner::~PipelineRunner() {}

bool liveSortShouldTrigger(const std::string& predictedClassId, const std::string& targetClassId, bool sortNonTarget) {
    if (predictedClassId.empty() || targetClassId.empty()) {
        return false;
    }
    const bool matchesTarget = predictedClassId == targetClassId;
    return sortNonTarget ? !matchesTarget : matchesTarget;
}

std::string PipelineRunner::toLowerAscii(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool PipelineRunner::init(const PipelineConfig& cfg, std::string& err) {
    if (cfg.detectorOnly)
        return init(cfg, nullptr, err);

    Metadata metadata;
    if (!LoadMetadata(cfg.metadataPath, metadata, err))
        return false;
    auto candidate = std::make_unique<OnnxInferenceAdapter>();
    const std::string requestedDevice = cfg.computeDevice.empty()
                                            ? (cfg.useCuda ? std::string("cuda") : std::string("auto"))
                                            : cfg.computeDevice;
    if (!candidate->load({}, cfg.onnxPath, cfg.metadataPath, metadata, requestedDevice, err))
        return false;
    return init(cfg, std::move(candidate), err);
}

bool PipelineRunner::init(const PipelineConfig& cfg, std::unique_ptr<OnnxInferenceAdapter> candidate,
                          std::string& err) {
    cfg_ = cfg;
    ready_ = false;
    triggerReady_ = false;
    trigger_.shutdown();
    frameCounter_ = 0;
    resolvedTargetClassId_.clear();
    resolvedTargetDisplayLabel_.clear();

    detector_ = std::make_unique<FastEventDetectorAdapter>(cfg_.detect);
    processor_ = std::make_unique<DropletFrameProcessor>(*detector_);
    if (cfg_.detectorOnly) {
        ready_ = true;
        return true;
    }

    OnnxInferenceAdapter* inference = candidate ? candidate.get() : classifier_.get();
    if (!inference) {
        err = "no installed model is available";
        return false;
    }
    const Metadata& metadata = inference->metadata();
    if (!ResolveTargetClassId(metadata, cfg_.targetClassId, cfg_.targetLabel, resolvedTargetClassId_,
                              resolvedTargetDisplayLabel_, err)) {
        return false;
    }
    cfg_.targetClassId = resolvedTargetClassId_;
    cfg_.targetLabel = resolvedTargetDisplayLabel_;

    if (!cfg_.daq.channel.empty()) {
        std::string trigErr;
        if (!trigger_.init(cfg_.daq, trigErr)) {
            if (!err.empty())
                err += "; ";
            err += "DAQ init disabled: " + trigErr;
        } else {
            triggerReady_ = true;
        }
    }

    if (candidate)
        installInference(std::move(candidate));
    ready_ = true;
    return true;
}

bool PipelineRunner::configureInstalled(const PipelineConfig& cfg, std::string& err) {
    if (!classifier_) {
        err = "no installed model is available";
        return false;
    }
    if (classifier_->sortingTargetClassId().empty() ||
        classifier_->sortingTriggerRule() != "trigger_on_target_class") {
        err = "installed model sorting_policy is unavailable or unsupported";
        return false;
    }

    PipelineConfig effective = cfg;
    effective.onnxPath = classifier_->modelPath();
    effective.metadataPath = classifier_->metadataPath();
    return init(effective, nullptr, err);
}

void PipelineRunner::installInference(std::unique_ptr<OnnxInferenceAdapter> candidate) noexcept {
    classifier_.swap(candidate);
}

void PipelineRunner::reset() {
    if (detector_) {
        processor_->reset();
    }
    frameCounter_ = 0;
}

void PipelineRunner::clear() {
    trigger_.shutdown();
    cfg_ = PipelineConfig{};
    processor_.reset();
    detector_.reset();
    ready_ = false;
    triggerReady_ = false;
    frameCounter_ = 0;
    resolvedTargetClassId_.clear();
    resolvedTargetDisplayLabel_.clear();
}

bool PipelineRunner::isReady() const {
    return ready_;
}

bool PipelineRunner::isTriggerReady() const {
    return triggerReady_;
}

int PipelineRunner::backgroundFramesRemaining() const {
    if (!processor_)
        return 0;
    return processor_->backgroundFramesRemaining();
}

bool PipelineRunner::fireTrigger(std::string& err) {
    if (!triggerReady_) {
        err = "DAQ trigger not ready";
        return false;
    }
    std::lock_guard<std::mutex> lock(triggerFireMutex_);
    return trigger_.fire(err);
}

const std::vector<std::string>& PipelineRunner::classLabels() const {
    static const std::vector<std::string> empty;
    return classifier_ ? classifier_->metadata().classes : empty;
}

std::string PipelineRunner::targetClassId() const {
    return resolvedTargetClassId_;
}

std::string PipelineRunner::targetDisplayLabel() const {
    return resolvedTargetDisplayLabel_;
}

std::string PipelineRunner::targetDisplayText() const {
    return FormatClassForDisplay(resolvedTargetClassId_, resolvedTargetDisplayLabel_);
}

std::string PipelineRunner::executionProvider() const {
    return classifier_ ? classifier_->executionProvider() : std::string();
}

std::string PipelineRunner::loadedModelId() const {
    return classifier_ ? classifier_->modelId() : std::string();
}

std::string PipelineRunner::loadedModelPath() const {
    return classifier_ ? classifier_->modelPath() : std::string();
}

std::string PipelineRunner::loadedMetadataPath() const {
    return classifier_ ? classifier_->metadataPath() : std::string();
}

std::string PipelineRunner::loadedModelSha256() const {
    return classifier_ ? classifier_->declaredOnnxSha256() : std::string();
}

std::string PipelineRunner::loadedMetadataSha256() const {
    return classifier_ ? classifier_->metadataSha256() : std::string();
}

bool PipelineRunner::processFrameBatch(const cv::Mat& gray8In, std::vector<PipelineEvent>& out) {
    out.clear();
    if (!ready_ || !processor_)
        return false;
    if (gray8In.empty())
        return false;

    frameCounter_++;
    if (cfg_.frameSkip > 0 && (frameCounter_ % (cfg_.frameSkip + 1)) != 0) {
        return false;
    }

    cv::Mat gray8 = gray8In;
    if (gray8.type() != CV_8UC1) {
        if (gray8.channels() == 3) {
            cv::cvtColor(gray8, gray8, cv::COLOR_BGR2GRAY);
        }
        gray8.convertTo(gray8, CV_8U);
    }
    const DropletFrameProcessingResult result = processor_->process(gray8);
    const DropletDetectionFrame& det = result.detection;
    PipelineEvent base;
    base.frameNumber = frameCounter_;
    base.frameWidth = gray8.cols;
    base.frameHeight = gray8.rows;
    base.detected = det.detected;
    base.fired = det.eventEntered;
    base.area = det.area;
    base.bbox = det.bbox;
    base.centroid = det.centroid;

    if (cfg_.detectorOnly) {
        out.push_back(std::move(base));
        return true;
    }

    if (result.enteredCropCount == 0) {
        out.push_back(std::move(base));
        return true;
    }

    for (std::size_t index = 0; index < result.enteredCropCount; ++index) {
        const DropletEnteredCrop& entered = result.enteredCrops[index];
        PipelineEvent event = base;
        const DropletTrackObservation& observation = det.enteredTracks[index];
        event.fired = true;
        event.area = observation.area;
        event.bbox = observation.bbox;
        event.centroid = observation.centroid;
        event.cropRect = entered.crop.sourceRect;
        const cv::Mat& crop = entered.crop.image;
        ClassificationResult cls = classifier_ ? classifier_->classify(crop) : ClassificationResult{};
        event.label = cls.label;
        event.predictedIndex = cls.index;
        event.scores = cls.scores;
        event.classified = true;
        if (!cls.scores.empty()) {
            auto bestIt = std::max_element(cls.scores.begin(), cls.scores.end());
            event.score = (bestIt != cls.scores.end()) ? *bestIt : 0.0f;
        }

        if (!cls.label.empty()) {
            event.shouldTrigger = liveSortShouldTrigger(cls.label, resolvedTargetClassId_, cfg_.sortNonTarget);
            if (triggerReady_ && event.shouldTrigger) {
                event.triggered = true;
                std::string trigErr;
                event.triggerOk = fireTrigger(trigErr);
                event.triggerError = trigErr;
            }
        }

        if (!cfg_.outputDir.empty()) {
            fs::path basePath(cfg_.outputDir);
            fs::create_directories(basePath);
            std::string labelName = sanitizeLabel(event.label.empty() ? "unclassified" : event.label);
            fs::path labelDir = basePath / labelName;
            fs::create_directories(labelDir);
            std::string name = "event_frame_" + std::to_string(frameCounter_) + "_track_" +
                               std::to_string(entered.trackId) + "_label_" + labelName;
            if (cfg_.saveCrop) {
                fs::path outPath = labelDir / (name + ".png");
                cv::imwrite(outPath.string(), crop);
                event.cropPath = outPath.string();
            }
            if (cfg_.saveOverlay) {
                cv::Mat overlay;
                cv::cvtColor(gray8, overlay, cv::COLOR_GRAY2BGR);
                cv::rectangle(overlay, event.cropRect, cv::Scalar(0, 255, 0), 2);
                fs::path outPath = labelDir / (name + "_overlay.png");
                cv::imwrite(outPath.string(), overlay);
                event.overlayPath = outPath.string();
            }
        }
        out.push_back(std::move(event));
    }

    return true;
}

bool PipelineRunner::processFrame(const cv::Mat& gray8, PipelineEvent& out) {
    std::vector<PipelineEvent> events;
    if (!processFrameBatch(gray8, events)) {
        out = PipelineEvent{};
        return false;
    }
    out = events.empty() ? PipelineEvent{} : std::move(events.front());
    return true;
}
