#pragma once

#include <cstddef>
#include <array>
#include <opencv2/core.hpp>

constexpr std::size_t kDropletTrackCapacity = 3;

struct DropletTrackObservation {
    int trackId = 0;
    int missedFrames = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
};

struct DropletDetectionFrame {
    bool detected = false;
    bool eventEntered = false;
bool lifecycleEnded = false;
const double* rejectedAreas = nullptr;
std::size_t rejectedCount = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
    cv::Mat mask;
    std::array<DropletTrackObservation, kDropletTrackCapacity> visibleTracks{};
    std::size_t visibleTrackCount = 0;
    std::array<DropletTrackObservation, kDropletTrackCapacity> enteredTracks{};
    std::size_t enteredTrackCount = 0;
    std::array<int, kDropletTrackCapacity> endedTrackIds{};
    std::size_t endedTrackCount = 0;
    bool capacityExceeded = false;
};

class IDropletDetector {
  public:
    virtual ~IDropletDetector() = default;

    virtual void reset() = 0;
    virtual int backgroundFramesRemaining() const = 0;
    virtual DropletDetectionFrame processFrame(const cv::Mat& frame) = 0;
};
