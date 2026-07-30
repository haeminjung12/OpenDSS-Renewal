#pragma once

#include <opencv2/core.hpp>

#include <QString>

namespace desktop_app {

struct DatasetCrop {
    cv::Mat image;
    cv::Rect sourceRect;
};

class CropService final {
  public:
    static bool makeDatasetCrop(const cv::Mat& frame, const cv::Rect& boundingBox,
                                DatasetCrop* output, QString* error = nullptr);
};

} // namespace desktop_app
