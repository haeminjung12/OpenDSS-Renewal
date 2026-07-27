#include "../event_detector.h"
#include "../fast_event_detector.h"

#include <cmath>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

constexpr int kWidth = 96;
constexpr int kHeight = 80;
const cv::Rect kDropletRect(28, 24, 24, 20);

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool sameMat(const cv::Mat& lhs, const cv::Mat& rhs) {
    if (lhs.empty() || rhs.empty())
        return lhs.empty() && rhs.empty();
    return lhs.type() == rhs.type() && lhs.size() == rhs.size() && cv::norm(lhs, rhs, cv::NORM_INF) == 0.0;
}

bool samePoint(const cv::Point2f& lhs, const cv::Point2f& rhs) {
    return std::abs(lhs.x - rhs.x) < 0.001f && std::abs(lhs.y - rhs.y) < 0.001f;
}

bool sameEventResult(const EventResult& lhs, const EventResult& rhs) {
    return lhs.detected == rhs.detected && std::abs(lhs.area - rhs.area) < 0.001 && lhs.bbox == rhs.bbox &&
           samePoint(lhs.centroid, rhs.centroid) && sameMat(lhs.mask, rhs.mask);
}

bool sameFastResult(const FastEventResult& lhs, const FastEventResult& rhs) {
    return lhs.detected == rhs.detected && lhs.fired == rhs.fired &&
           lhs.lifecycleEnded == rhs.lifecycleEnded && std::abs(lhs.area - rhs.area) < 0.001 &&
           lhs.bbox == rhs.bbox && samePoint(lhs.centroid, rhs.centroid) && sameMat(lhs.mask, rhs.mask);
}

cv::Mat frame8(int value = 100) {
    return cv::Mat(kHeight, kWidth, CV_8UC1, cv::Scalar(value)).clone();
}

cv::Mat droplet8(const cv::Rect& rect = kDropletRect) {
    cv::Mat frame = frame8();
    cv::rectangle(frame, rect, cv::Scalar(200), cv::FILLED);
    return frame;
}

cv::Mat frame16(int value = 25600) {
    return cv::Mat(kHeight, kWidth, CV_16UC1, cv::Scalar(value)).clone();
}

cv::Mat droplet16(const cv::Rect& rect = kDropletRect) {
    cv::Mat frame = frame16();
    cv::rectangle(frame, rect, cv::Scalar(51200), cv::FILLED);
    return frame;
}

EventDetectorConfig preciseConfig() {
    EventDetectorConfig cfg;
    cfg.minArea = 20;
    cfg.minMaskArea = 10;
    cfg.maxAreaFrac = 0.5;
    cfg.borderMargin = 1;
    cfg.sigma = 0.1;
    cfg.morphRadius = 1;
    cfg.contrastClip = 0.0;
    cfg.bgMode = "mean";
    return cfg;
}

FastEventConfig fastConfig() {
    FastEventConfig cfg;
    cfg.bgFrames = 2;
    cfg.bgUpdateFrames = 0;
    cfg.resetFrames = 2;
    cfg.minArea = 20.0;
    cfg.minAreaFrac = 0.0;
    cfg.maxAreaFrac = 0.5;
    cfg.minBbox = 4;
    cfg.margin = 1;
    cfg.diffThresh = 10;
    cfg.blurRadius = 0;
    cfg.morphRadius = 0;
    cfg.scale = 1.0;
    cfg.gapFireShift = 16;
    return cfg;
}

void characterizePreciseDetector() {
    EventDetectorConfig cfg = preciseConfig();
    EventDetector detector(cfg);

    expect(!detector.hasBackground(), "precise detector starts without a background");
    expect(!detector.detect(cv::Mat(), true).detected, "precise detector ignores an empty frame");
    expect(!detector.detect(droplet8(), true).detected, "precise detector does not detect before background setup");

    std::string error;
    expect(!detector.buildBackground({}, error), "precise mean background rejects an empty frame set");
    expect(error == "no frames provided for background", "precise empty-background error is stable");

    error.clear();
    const std::vector<cv::Mat> backgrounds{frame8(), frame8()};
    expect(detector.buildBackground(backgrounds, error), "precise detector builds a mean background");
    expect(error.empty(), "precise background build does not report an error");
    expect(detector.hasBackground(), "precise detector reports background readiness");
    expect(detector.background().type() == CV_32FC1, "precise background is stored as normalized float");

    const EventResult noMask = detector.detect(droplet8(), false);
    expect(noMask.detected, "precise detector finds the deterministic 8-bit droplet");
    expect(noMask.mask.empty(), "precise detector omits the mask when it is not requested");

    const EventResult first = detector.detect(droplet8(), true);
    const EventResult repeated = detector.detect(droplet8(), true);
    expect(first.detected, "precise detector returns a detection with mask requested");
    expect(first.bbox == kDropletRect, "precise detector preserves the synthetic droplet bounding box");
    expect(samePoint(first.centroid, cv::Point2f(39.5f, 33.5f)),
           "precise detector reports the synthetic droplet centroid");
    expect(first.area == 435.0, "precise detector contour area is characterized");
    expect(first.mask.type() == CV_8UC1 && first.mask.size() == cv::Size(kWidth, kHeight),
           "precise detector returns a full-frame 8-bit mask");
    expect(cv::countNonZero(first.mask) == 476, "precise detector mask area is characterized");
    expect(sameEventResult(first, repeated), "precise detection is stateless across repeated frames");

    EventDetector replayA(cfg);
    EventDetector replayB(cfg);
    std::string replayErrorA;
    std::string replayErrorB;
    expect(replayA.buildBackground(backgrounds, replayErrorA) && replayB.buildBackground(backgrounds, replayErrorB),
           "precise replay detectors build identical backgrounds");
    const std::vector<cv::Mat> replayFrames{frame8(), droplet8(), droplet8(), frame8()};
    for (std::size_t i = 0; i < replayFrames.size(); ++i) {
        expect(sameEventResult(replayA.detect(replayFrames[i], true), replayB.detect(replayFrames[i], true)),
               "precise replay frame " + std::to_string(i) + " is deterministic");
    }

    EventDetector detector16(cfg);
    std::string error16;
    expect(detector16.buildBackground({frame16(), frame16()}, error16),
           "precise detector accepts 16-bit background input");
    expect(!detector16.detect(droplet16(), true).detected,
           "precise detector currently saturates 16-bit input during CV_8U conversion");
}

std::vector<FastEventResult> runFastReplay(const FastEventConfig& cfg, const std::vector<cv::Mat>& frames) {
    FastEventDetector detector(cfg);
    expect(detector.addBackgroundFrame(frame8()) == false, "fast replay first background frame is not ready");
    expect(detector.addBackgroundFrame(frame8()), "fast replay second background frame becomes ready");

    std::vector<FastEventResult> results;
    results.reserve(frames.size());
    for (const cv::Mat& frame : frames) {
        FastEventResult result;
        expect(detector.processFrame(frame, result), "fast replay processes frame after readiness");
        results.push_back(result);
    }
    return results;
}

void characterizeFastDetector() {
    const FastEventConfig cfg = fastConfig();
    FastEventDetector detector(cfg);

    expect(!detector.isReady(), "fast detector starts unready");
    expect(detector.backgroundFramesRemaining() == 2, "fast detector reports its initial background countdown");
    FastEventResult empty;
    expect(!detector.processFrame(cv::Mat(), empty), "fast detector rejects an empty frame");
    expect(!empty.detected && !empty.fired, "fast empty-frame result is cleared");
    expect(detector.backgroundFramesRemaining() == 2, "fast empty frame does not advance background readiness");

    expect(!detector.addBackgroundFrame(frame8()), "fast first background frame is accepted but not ready");
    expect(detector.backgroundFramesRemaining() == 1, "fast background countdown advances");
    expect(detector.addBackgroundFrame(frame8()), "fast second background frame establishes readiness");
    expect(detector.isReady(), "fast detector reports readiness after the configured frames");
    expect(detector.backgroundFramesRemaining() == 0, "fast ready detector has no background frames remaining");
    expect(detector.background().type() == CV_8UC1, "fast background is stored as 8-bit data");

    FastEventResult first;
    expect(detector.processFrame(droplet8(), first), "fast detector processes the deterministic 8-bit droplet");
    expect(first.detected && first.fired, "fast first detected frame is also a newly entered event");
    expect(first.bbox == kDropletRect, "fast detector preserves the synthetic droplet bounding box");
    expect(samePoint(first.centroid, cv::Point2f(39.5f, 33.5f)),
           "fast detector reports the synthetic droplet centroid");
    expect(first.area == 480.0, "fast detector connected-component area is characterized");
    expect(first.mask.type() == CV_8UC1 && cv::countNonZero(first.mask) == 480,
           "fast detector returns the characterized full-frame mask");

    FastEventResult repeated;
    detector.processFrame(droplet8(), repeated);
    expect(repeated.detected && !repeated.fired,
           "fast repeated detection remains observed but does not enter a second event");

    FastEventResult oneGap;
    detector.processFrame(frame8(), oneGap);
    expect(!oneGap.detected && !oneGap.fired && !oneGap.lifecycleEnded,
           "fast first missing frame retains the lifecycle");
    FastEventResult sameAfterGap;
    detector.processFrame(droplet8(), sameAfterGap);
    expect(sameAfterGap.detected && !sameAfterGap.fired,
           "fast same-position detection after a short gap does not re-enter");

    FastEventResult gapOne;
    FastEventResult gapTwo;
    detector.processFrame(frame8(), gapOne);
    detector.processFrame(frame8(), gapTwo);
    expect(!gapOne.lifecycleEnded && gapTwo.lifecycleEnded,
           "fast lifecycle ends only on resetFrames consecutive misses");
    FastEventResult afterReset;
    detector.processFrame(droplet8(), afterReset);
    expect(afterReset.detected && afterReset.fired,
           "fast detection enters a new event after resetFrames consecutive misses");

    FastEventResult shiftedGap;
    detector.processFrame(frame8(), shiftedGap);
    FastEventResult shifted;
    detector.processFrame(droplet8(cv::Rect(kDropletRect.x + 20, kDropletRect.y, kDropletRect.width, kDropletRect.height)),
                          shifted);
    expect(shifted.detected && shifted.fired,
           "fast centroid shift after a short gap enters a new event before reset hysteresis completes");

    detector.reset();
    expect(!detector.isReady(), "fast reset clears background readiness");
    expect(detector.backgroundFramesRemaining() == 2, "fast reset restores the configured background countdown");

    const std::vector<cv::Mat> replayFrames{
        frame8(), droplet8(), droplet8(), frame8(), droplet8(), frame8(), frame8(), droplet8()};
    const std::vector<FastEventResult> replayA = runFastReplay(cfg, replayFrames);
    const std::vector<FastEventResult> replayB = runFastReplay(cfg, replayFrames);
    expect(replayA.size() == replayB.size(), "fast deterministic replays return the same number of results");
    for (std::size_t i = 0; i < replayA.size() && i < replayB.size(); ++i) {
        expect(sameFastResult(replayA[i], replayB[i]),
               "fast replay frame " + std::to_string(i) + " is deterministic");
    }

    FastEventDetector detector16(cfg);
    expect(!detector16.addBackgroundFrame(frame16()), "fast 16-bit first background frame is not ready");
    expect(detector16.addBackgroundFrame(frame16()), "fast 16-bit second background frame becomes ready");
    FastEventResult result16;
    expect(detector16.processFrame(droplet16(), result16), "fast detector processes 16-bit input");
    expect(result16.detected && result16.fired,
           "fast detector currently converts 16-bit input with the documented 1/256 scale");
    expect(result16.bbox == kDropletRect, "fast 16-bit conversion preserves detection coordinates");

    FastEventConfig defaultConfig;
    FastEventDetector defaultDetector(defaultConfig);
    expect(defaultDetector.minimumContourArea() == 100,
           "fast detector starts with the authoritative 100 px2 minimum contour area");
    defaultConfig.minArea = -1.0;
    FastEventDetector legacyDetector(defaultConfig);
    expect(legacyDetector.minimumContourArea() == 100,
           "legacy -1 minimum contour area converts to 100 px2");

    FastEventDetector liveThresholdDetector(cfg);
    expect(!liveThresholdDetector.addBackgroundFrame(frame8()),
           "live-threshold detector accepts its first background frame");
    expect(liveThresholdDetector.addBackgroundFrame(frame8()),
           "live-threshold detector establishes its background");
    liveThresholdDetector.setMinimumContourArea(481);
    FastEventResult suppressed;
    expect(liveThresholdDetector.processFrame(droplet8(), suppressed)
               && !suppressed.detected,
           "an immediate threshold above 480 suppresses the next frame");
    expect(liveThresholdDetector.isReady(),
           "an immediate threshold update preserves background readiness");
    liveThresholdDetector.setMinimumContourArea(480);
    FastEventResult restored;
    expect(liveThresholdDetector.processFrame(droplet8(), restored)
               && restored.detected,
           "an immediate threshold of 480 accepts the next frame without reset");

    std::atomic<bool> settersDone{false};
    std::thread setter([&] {
        for (int index = 0; index < 2000; ++index)
            liveThresholdDetector.setMinimumContourArea(
                index % 2 == 0 ? 100 : 1000);
        settersDone.store(true, std::memory_order_release);
    });
    for (int index = 0; index < 2000; ++index) {
        FastEventResult concurrent;
        expect(liveThresholdDetector.processFrame(droplet8(), concurrent),
               "concurrent threshold updates do not interrupt frame processing");
    }
    setter.join();
    expect(settersDone.load(std::memory_order_acquire)
               && liveThresholdDetector.isReady(),
           "concurrent threshold updates complete without rebuilding background");
}

} // namespace

int main() {
    characterizePreciseDetector();
    characterizeFastDetector();
    if (failures != 0) {
        std::cerr << failures << " detector characterization assertion(s) failed\n";
        return 1;
    }
    std::cout << "Detector characterization passed.\n";
    return 0;
}
