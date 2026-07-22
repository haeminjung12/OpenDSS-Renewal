#pragma once

#include <string>
#include <vector>

#include "droplet_detector.h"
#include "../event_detector.h"
#include "../fast_event_detector.h"

class FastEventDetectorAdapter final : public IDropletDetector {
  public:
    explicit FastEventDetectorAdapter(const FastEventConfig& config);

    void reset() override;
    int backgroundFramesRemaining() const override;
    DropletDetectionFrame processFrame(const cv::Mat& frame) override;

    bool isReady() const;
    bool addBackgroundFrame(const cv::Mat& frame);
    const cv::Mat& background() const;

  private:
    FastEventDetector detector_;
};

class EventDetectorAdapter final : public IDropletDetector {
  public:
    EventDetectorAdapter(const EventDetectorConfig& config, int resetFrames, double minAreaFrac, int minBbox,
                         bool includeMask);

    void reset() override;
    int backgroundFramesRemaining() const override;
    DropletDetectionFrame processFrame(const cv::Mat& frame) override;

    bool buildBackground(const std::vector<cv::Mat>& frames, std::string& error);
    const cv::Mat& background() const;

  private:
    EventDetector detector_;
    int resetFrames_ = 0;
    double minAreaFrac_ = 0.0;
    int minBbox_ = 0;
    bool includeMask_ = false;
    bool eventActive_ = false;
    int noDetectionFrames_ = 0;
};
