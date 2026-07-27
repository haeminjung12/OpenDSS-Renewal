#include "droplet_detector_adapters.h"

namespace {
DropletDetectionFrame mapFastResult(const FastEventResult& result) {
    DropletDetectionFrame frame;
    frame.detected = result.detected;
    frame.eventEntered = result.fired;
frame.lifecycleEnded = result.lifecycleEnded;
frame.rejectedAreas = result.rejectedAreas;
frame.rejectedCount = result.rejectedCount;
    frame.area = result.area;
    frame.bbox = result.bbox;
    frame.centroid = result.centroid;
    frame.mask = result.mask;
    return frame;
}
} // namespace

FastEventDetectorAdapter::FastEventDetectorAdapter(const FastEventConfig& config) : detector_(config) {}

void FastEventDetectorAdapter::reset() {
    detector_.reset();
}

int FastEventDetectorAdapter::backgroundFramesRemaining() const {
    return detector_.backgroundFramesRemaining();
}

DropletDetectionFrame FastEventDetectorAdapter::processFrame(const cv::Mat& frame) {
    FastEventResult result;
    detector_.processFrame(frame, result);
    return mapFastResult(result);
}

bool FastEventDetectorAdapter::isReady() const {
    return detector_.isReady();
}

bool FastEventDetectorAdapter::addBackgroundFrame(const cv::Mat& frame) {
    return detector_.addBackgroundFrame(frame);
}

const cv::Mat& FastEventDetectorAdapter::background() const {
    return detector_.background();
}

int FastEventDetectorAdapter::minimumContourArea() const noexcept {
    return detector_.minimumContourArea();
}

void FastEventDetectorAdapter::setMinimumContourArea(int area) noexcept {
    detector_.setMinimumContourArea(area);
}

EventDetectorAdapter::EventDetectorAdapter(const EventDetectorConfig& config, int resetFrames, double minAreaFrac,
                                           int minBbox, bool includeMask)
    : detector_(config), resetFrames_(resetFrames), minAreaFrac_(minAreaFrac), minBbox_(minBbox),
      includeMask_(includeMask) {}

void EventDetectorAdapter::reset() {
    eventActive_ = false;
    noDetectionFrames_ = 0;
}

int EventDetectorAdapter::backgroundFramesRemaining() const {
    return 0;
}

DropletDetectionFrame EventDetectorAdapter::processFrame(const cv::Mat& frame) {
    const EventResult result = detector_.detect(frame, includeMask_);
    bool detected = result.detected;
    if (detected) {
        const double imageArea = static_cast<double>(frame.rows) * static_cast<double>(frame.cols);
        if (result.area < (minAreaFrac_ * imageArea) || result.bbox.width < minBbox_ ||
            result.bbox.height < minBbox_) {
            detected = false;
        }
    }

    bool eventEntered = false;
    bool lifecycleEnded = false;
    if (detected) {
        noDetectionFrames_ = 0;
        if (!eventActive_) {
            eventEntered = true;
            eventActive_ = true;
        }
    } else if (eventActive_) {
        noDetectionFrames_++;
        if (noDetectionFrames_ >= resetFrames_) {
            eventActive_ = false;
            noDetectionFrames_ = 0;
            lifecycleEnded = true;
        }
    }

    DropletDetectionFrame frameResult;
    frameResult.detected = detected;
    frameResult.eventEntered = eventEntered;
    frameResult.lifecycleEnded = lifecycleEnded;
    frameResult.area = result.area;
    frameResult.bbox = result.bbox;
    frameResult.centroid = result.centroid;
    frameResult.mask = result.mask;
    return frameResult;
}

bool EventDetectorAdapter::buildBackground(const std::vector<cv::Mat>& frames, std::string& error) {
    return detector_.buildBackground(frames, error);
}

const cv::Mat& EventDetectorAdapter::background() const {
    return detector_.background();
}
