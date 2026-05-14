#pragma once

#include <string>
#include <vector>
#include <opencv2/core.hpp>

struct EventDetectorConfig {
    int minArea = 40;
    int minMaskArea = 20;
    double maxAreaFrac = 0.10;
    int borderMargin = 5;
    double sigma = 1.0;
    int morphRadius = 2;
    double contrastClip = 0.01;
    std::string bgMode = "mean";
    size_t medianMaxElements = 40000000;
};

struct EventResult {
    bool detected = false;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
    cv::Mat mask;
};

class EventDetector {
public:
    explicit EventDetector(const EventDetectorConfig& cfg);

    bool buildBackground(const std::vector<cv::Mat>& frames, std::string& err);
    bool hasBackground() const;
    const cv::Mat& background() const;

    EventResult detect(const cv::Mat& gray8, bool includeMask);

private:
    EventDetectorConfig cfg_;
    cv::Mat background_;
};
