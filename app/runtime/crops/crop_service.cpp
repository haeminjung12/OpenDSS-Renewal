#include "crop_service.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace desktop_app {

bool CropService::makeDatasetCrop(const cv::Mat& frame, const cv::Rect& boundingBox,
                                  DatasetCrop* output, QString* error) {
    if (error)
        error->clear();
    if (!output) {
        if (error)
            *error = "Crop output is required.";
        return false;
    }
    output->image.release();
    output->sourceRect = {};
    if (frame.empty() || frame.type() != CV_8UC1 || boundingBox.width <= 0 ||
        boundingBox.height <= 0) {
        if (error)
            *error = "Dataset crop requires a nonempty CV_8UC1 frame and valid bounding box.";
        return false;
    }
    const cv::Rect bounded = boundingBox & cv::Rect(0, 0, frame.cols, frame.rows);
    if (bounded.width <= 0 || bounded.height <= 0) {
        if (error)
            *error = "Dataset crop bounding box does not intersect the frame.";
        return false;
    }
    int side = (std::max)(bounded.width, bounded.height);
    side = (std::min)(side, (std::min)(frame.cols, frame.rows));
    int x = bounded.x + bounded.width / 2 - side / 2;
    int y = bounded.y + bounded.height / 2 - side / 2;
    x = (std::max)(0, (std::min)(x, frame.cols - side));
    y = (std::max)(0, (std::min)(y, frame.rows - side));
    output->sourceRect = cv::Rect(x, y, side, side);
    cv::resize(frame(output->sourceRect), output->image, cv::Size(64, 64), 0, 0,
               cv::INTER_AREA);
    return output->image.type() == CV_8UC1 && output->image.isContinuous();
}

} // namespace desktop_app
