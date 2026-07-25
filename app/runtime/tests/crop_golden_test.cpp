#include "../crops/crop_service.h"

#include <opencv2/imgproc.hpp>

#include <iostream>

namespace {
cv::Rect referenceRect(const cv::Rect& bbox, const cv::Size& size) {
    if (bbox.width <= 0 || bbox.height <= 0 || size.width <= 0 || size.height <= 0)
        return {};
    int side = (std::max)(bbox.width, bbox.height);
    side = (std::min)(side, (std::min)(size.width, size.height));
    int x = bbox.x + bbox.width / 2 - side / 2;
    int y = bbox.y + bbox.height / 2 - side / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + side > size.width) x = size.width - side;
    if (y + side > size.height) y = size.height - side;
    return {(std::max)(0, x), (std::max)(0, y), side, side};
}

bool check(const cv::Mat& frame, const cv::Rect& bbox) {
    const cv::Rect bounded = bbox & cv::Rect(0, 0, frame.cols, frame.rows);
    const cv::Rect expectedRect = referenceRect(bounded, frame.size());
    cv::Mat expected;
    cv::resize(frame(expectedRect), expected, {64, 64}, 0, 0, cv::INTER_AREA);
    desktop_app::DatasetCrop actual;
    QString error;
    return desktop_app::CropService::makeDatasetCrop(frame, bbox, &actual, &error) &&
           actual.sourceRect == expectedRect &&
           cv::countNonZero(actual.image != expected) == 0 &&
           actual.image.type() == CV_8UC1 && actual.image.isContinuous();
}
}

int main() {
    cv::Mat frame(81, 121, CV_8UC1);
    for (int y = 0; y < frame.rows; ++y)
        for (int x = 0; x < frame.cols; ++x)
            frame.at<uchar>(y, x) = static_cast<uchar>((x * 7 + y * 13) % 256);
    if (!check(frame, {30, 20, 15, 29}) || !check(frame, {-4, 3, 18, 9}) ||
        !check(frame, {70, 60, 45, 12}) || !check(frame, {-20, -20, 200, 200})) {
        std::cerr << "Dataset crop does not match legacy centered/clamped INTER_AREA pixels.\n";
        return 1;
    }
    desktop_app::DatasetCrop output;
    QString error;
    if (desktop_app::CropService::makeDatasetCrop(cv::Mat(), {0, 0, 1, 1}, &output,
                                                   &error) ||
        desktop_app::CropService::makeDatasetCrop(cv::Mat(4, 4, CV_16UC1),
                                                   {0, 0, 2, 2}, &output, &error) ||
        desktop_app::CropService::makeDatasetCrop(frame, {200, 200, 2, 2},
                                                   &output, &error)) {
        std::cerr << "Invalid crop input was accepted.\n";
        return 2;
    }
    return 0;
}
