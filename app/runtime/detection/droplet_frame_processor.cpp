#include "droplet_frame_processor.h"

DropletFrameProcessor::DropletFrameProcessor(IDropletDetector& detector) : detector_(detector) {}

void DropletFrameProcessor::reset() {
    detector_.reset();
}

int DropletFrameProcessor::backgroundFramesRemaining() const {
    return detector_.backgroundFramesRemaining();
}

DropletFrameProcessingResult DropletFrameProcessor::process(const cv::Mat& orderedGray8Frame) {
    DropletFrameProcessingResult result;
    result.detection = detector_.processFrame(orderedGray8Frame);
    for (std::size_t index = 0; index < result.detection.enteredTrackCount; ++index) {
        const DropletTrackObservation& entry = result.detection.enteredTracks[index];
        DropletEnteredCrop& output = result.enteredCrops[result.enteredCropCount];
        output.trackId = entry.trackId;
        if (!desktop_app::CropService::makeDatasetCrop(orderedGray8Frame, entry.bbox,
                                                       &output.crop, &result.cropError)) {
            result.cropFailed = true;
            return result;
        }
        ++result.enteredCropCount;
    }
    return result;
}
