#pragma once

#include "droplet_detector.h"
#include "../crops/crop_service.h"

#include <array>

struct DropletEnteredCrop {
    int trackId = 0;
    desktop_app::DatasetCrop crop;
};

struct DropletFrameProcessingResult {
    DropletDetectionFrame detection;
    std::array<DropletEnteredCrop, kDropletTrackCapacity> enteredCrops{};
    std::size_t enteredCropCount = 0;
    bool cropFailed = false;
    QString cropError;
};

class DropletFrameProcessor final {
  public:
    explicit DropletFrameProcessor(IDropletDetector& detector);

    void reset();
    int backgroundFramesRemaining() const;
    DropletFrameProcessingResult process(const cv::Mat& orderedGray8Frame);

  private:
    IDropletDetector& detector_;
};
