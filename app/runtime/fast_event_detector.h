#pragma once

#include <deque>
#include <vector>
#include <opencv2/core.hpp>

struct FastEventConfig {
    int bgFrames = 100;
    int bgUpdateFrames = 50;
    int resetFrames = 2;
    double minArea = -1.0; // <=0 means auto
    double minAreaFrac = 0.0;
    double maxAreaFrac = 0.10;
    int minBbox = 32;
    int margin = 5;
    int diffThresh = 15;
    int blurRadius = 1;
    int morphRadius = 1;
    bool useContourExtraction = false;
    double scale = 0.5;
    int gapFireShift = 0; // <=0 means auto
};

struct FastEventResult {
    bool detected = false;
    bool fired = false;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
    cv::Mat mask;
};

class FastEventDetector {
  public:
    explicit FastEventDetector(const FastEventConfig& cfg);

    void reset();
    bool isReady() const;
    int backgroundFramesRemaining() const;
    const cv::Mat& background() const;

    bool addBackgroundFrame(const cv::Mat& gray8);
    bool processFrame(const cv::Mat& gray8, FastEventResult& out);

  private:
    struct RollingBackground8 {
        std::deque<cv::Mat> frames;
        cv::Mat sum;
        int maxFrames = 0;
    };

    void updateDerivedParams(const cv::Size& fullSize, const cv::Size& scaledSize);
    bool updateRollingBackground(const cv::Mat& gray8Scaled);
    cv::Mat toGray8Fast(const cv::Mat& src) const;

    FastEventConfig cfg_;
    bool ready_ = false;
    int initFrames_ = 0;
    int collected_ = 0;

    cv::Size fullSize_;
    cv::Mat backgroundScaled_;
    RollingBackground8 rolling_;
    std::vector<cv::Mat> bgStack_;
    cv::Mat morphKernel_;

    bool triggered_ = false;
    int noDetectCount_ = 0;
    bool hasLastDet_ = false;
    cv::Point2f lastCentroid_ = {0.0f, 0.0f};

    double areaScale_ = 1.0;
    int minAreaScaled_ = 1;
    int minAreaByFracScaled_ = 0;
    int maxAreaScaled_ = 1;
    int marginScaled_ = 1;
    int minBboxScaled_ = 1;
    int gapFireShift_ = 0;
};
