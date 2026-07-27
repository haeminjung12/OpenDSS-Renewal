#pragma once

#include <opencv2/core.hpp>

struct DropletDetectionFrame {
    bool detected = false;
    bool eventEntered = false;
    bool lifecycleEnded = false;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
    cv::Mat mask;
};

class IDropletDetector {
  public:
    virtual ~IDropletDetector() = default;

    virtual void reset() = 0;
    virtual int backgroundFramesRemaining() const = 0;
    virtual DropletDetectionFrame processFrame(const cv::Mat& frame) = 0;
};
