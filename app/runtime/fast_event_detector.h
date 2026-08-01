#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <deque>
#include <vector>
#include <opencv2/core.hpp>

struct FastEventConfig {
    int bgFrames = 100;
    int bgUpdateFrames = 50;
    int resetFrames = 2;
    double minArea = 100.0;
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

constexpr std::size_t kFastEventTrackCapacity = 3;
constexpr std::size_t kFastEventCandidateCapacity = 16;

struct FastEventTrackObservation {
    int trackId = 0;
    int missedFrames = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
};

struct FastEventResult {
    bool detected = false;
    bool fired = false;
bool lifecycleEnded = false;
const double* rejectedAreas = nullptr;
std::size_t rejectedCount = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
    cv::Mat mask;
    std::array<FastEventTrackObservation, kFastEventTrackCapacity> visibleTracks{};
    std::size_t visibleTrackCount = 0;
    std::array<FastEventTrackObservation, kFastEventTrackCapacity> enteredTracks{};
    std::size_t enteredTrackCount = 0;
    std::array<int, kFastEventTrackCapacity> endedTrackIds{};
    std::size_t endedTrackCount = 0;
    bool capacityExceeded = false;
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
    int minimumContourArea() const noexcept;
    void setMinimumContourArea(int area) noexcept;

  private:
    struct RollingBackground8 {
        std::deque<cv::Mat> frames;
        cv::Mat sum;
        int maxFrames = 0;
    };

    struct TrackState {
        bool active = false;
        int id = 0;
        int missedFrames = 0;
        double area = 0.0;
        cv::Rect bbox;
        cv::Point2f centroid = {0.0f, 0.0f};
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
    std::vector<double> rejectedAreas_;

    std::array<TrackState, kFastEventTrackCapacity> tracks_{};
    int nextTrackId_ = 1;

    double areaScale_ = 1.0;
    std::atomic<int> minimumContourArea_{100};
    int minAreaByFracScaled_ = 0;
    int maxAreaScaled_ = 1;
    int marginScaled_ = 1;
    int minBboxScaled_ = 1;
    int gapFireShift_ = 0;
};
