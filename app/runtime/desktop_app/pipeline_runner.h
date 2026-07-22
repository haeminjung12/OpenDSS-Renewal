#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "../daq_trigger.h"
#include "../detection/droplet_detector.h"
#include "../fast_event_detector.h"
#include "../metadata_loader.h"
#include "../onnx_classifier.h"

struct PipelineConfig {
    std::string onnxPath;
    std::string metadataPath;
    std::string targetClassId;
    std::string targetLabel = "Single";
    bool sortNonTarget = false;
    std::string outputDir;
    std::string computeDevice = "auto";
    bool detectorOnly = false;
    bool useCuda = false;
    bool saveCrop = false;
    bool saveOverlay = false;
    int cropSize = 64;
    int frameSkip = 0;
    FastEventConfig detect;
    DaqConfig daq;
};

struct PipelineEvent {
    bool detected = false;
    bool fired = false;
    bool classified = false;
    bool shouldTrigger = false;
    bool triggered = false;
    bool triggerOk = false;
    double area = 0.0;
    int frameWidth = 0;
    int frameHeight = 0;
    cv::Rect bbox;
    cv::Rect cropRect;
    cv::Point2f centroid = {0.0f, 0.0f};
    std::string label;
    int predictedIndex = -1;
    float score = 0.0f;
    std::vector<float> scores;
    std::string cropPath;
    std::string overlayPath;
    std::string triggerError;
    int64_t frameNumber = 0;
};

bool liveSortShouldTrigger(const std::string& predictedClassId, const std::string& targetClassId, bool sortNonTarget);

class PipelineRunner {
  public:
    PipelineRunner() = default;
    ~PipelineRunner();
    bool init(const PipelineConfig& cfg, std::string& err);
    void clear();
    void reset();
    bool isReady() const;
    bool isTriggerReady() const;
    int backgroundFramesRemaining() const;
    bool processFrame(const cv::Mat& gray8, PipelineEvent& out);
    bool fireTrigger(std::string& err);
    const std::vector<std::string>& classLabels() const;
    std::string targetClassId() const;
    std::string targetDisplayLabel() const;
    std::string targetDisplayText() const;
    std::string executionProvider() const;

  private:
    static std::string toLowerAscii(const std::string& s);
    static cv::Rect makeSquareRect(const cv::Rect& bbox, const cv::Size& size);

    PipelineConfig cfg_;
    Metadata meta_;
    OnnxClassifier classifier_;
    std::unique_ptr<IDropletDetector> detector_;
    DaqTrigger trigger_;
    bool ready_ = false;
    bool triggerReady_ = false;
    int64_t frameCounter_ = 0;
    std::string resolvedTargetClassId_;
    std::string resolvedTargetDisplayLabel_;
    std::mutex triggerFireMutex_;
};
