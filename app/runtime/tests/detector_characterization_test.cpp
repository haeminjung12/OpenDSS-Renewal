#include "../event_detector.h"
#include "../fast_event_detector.h"
#include "../detection/droplet_frame_processor.h"

#include <cmath>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

constexpr int kWidth = 96;
constexpr int kHeight = 80;
const cv::Rect kDropletRect(10, 24, 24, 20);

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
    if (lhs.detected != rhs.detected || lhs.fired != rhs.fired ||
        lhs.lifecycleEnded != rhs.lifecycleEnded ||
        lhs.rejectedCount != rhs.rejectedCount ||
        std::abs(lhs.area - rhs.area) >= 0.001 ||
        lhs.bbox != rhs.bbox || !samePoint(lhs.centroid, rhs.centroid) ||
        !sameMat(lhs.mask, rhs.mask))
        return false;
    if (lhs.visibleTrackCount != rhs.visibleTrackCount ||
        lhs.enteredTrackCount != rhs.enteredTrackCount ||
        lhs.endedTrackCount != rhs.endedTrackCount ||
        lhs.capacityExceeded != rhs.capacityExceeded)
        return false;
    for (std::size_t index = 0; index < lhs.visibleTrackCount; ++index) {
        const auto& left = lhs.visibleTracks[index];
        const auto& right = rhs.visibleTracks[index];
        if (left.trackId != right.trackId || left.missedFrames != right.missedFrames ||
            std::abs(left.area - right.area) >= 0.001 || left.bbox != right.bbox ||
            !samePoint(left.centroid, right.centroid))
            return false;
    }
    for (std::size_t index = 0; index < lhs.enteredTrackCount; ++index) {
        const auto& left = lhs.enteredTracks[index];
        const auto& right = rhs.enteredTracks[index];
        if (left.trackId != right.trackId || left.bbox != right.bbox)
            return false;
    }
    for (std::size_t index = 0; index < lhs.endedTrackCount; ++index)
        if (lhs.endedTrackIds[index] != rhs.endedTrackIds[index])
            return false;
    for (std::size_t index = 0; index < lhs.rejectedCount; ++index) {
        if (std::abs(lhs.rejectedAreas[index] - rhs.rejectedAreas[index]) >= 0.001)
            return false;
    }
    return true;
}

cv::Mat frame8(int value = 100) {
    return cv::Mat(kHeight, kWidth, CV_8UC1, cv::Scalar(value)).clone();
}

cv::Mat droplet8(const cv::Rect& rect = kDropletRect) {
    cv::Mat frame = frame8();
    cv::rectangle(frame, rect, cv::Scalar(200), cv::FILLED);
    return frame;
}

cv::Mat mixedCandidates8() {
    cv::Mat frame = frame8();
    cv::rectangle(frame, cv::Rect(8, 8, 5, 6), cv::Scalar(200), cv::FILLED);
    cv::rectangle(frame, cv::Rect(16, 20, 20, 15), cv::Scalar(200), cv::FILLED);
    cv::rectangle(frame, cv::Rect(75, 50, 7, 8), cv::Scalar(200), cv::FILLED);
    return frame;
}

class ScriptedDetector final : public IDropletDetector {
  public:
    DropletDetectionFrame next;
    int calls = 0;

    void reset() override {}
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        ++calls;
        return next;
    }
};

cv::Mat twoCandidates8(const cv::Rect& first, const cv::Rect& second) {
    cv::Mat frame = frame8();
    cv::rectangle(frame, first, cv::Scalar(200), cv::FILLED);
    cv::rectangle(frame, second, cv::Scalar(200), cv::FILLED);
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

void verifyFrameProcessor() {
    ScriptedDetector detector;
    DropletFrameProcessor processor(detector);
    detector.next.detected = true;
    detector.next.visibleTrackCount = 1;
    detector.next.visibleTracks[0] = {7, 0, 480.0, kDropletRect, {40.0f, 34.0f}};
    detector.next.enteredTrackCount = 1;
    detector.next.enteredTracks[0] = detector.next.visibleTracks[0];
    const DropletFrameProcessingResult entered = processor.process(droplet8());
    expect(detector.calls == 1 && entered.enteredCropCount == 1 && !entered.cropFailed &&
               entered.enteredCrops[0].trackId == 7,
           "frame processor invokes the detector once and creates one crop for one entry");

    detector.next.enteredTrackCount = 0;
    detector.next.endedTrackCount = 1;
    detector.next.endedTrackIds[0] = 3;
    const DropletFrameProcessingResult continuing = processor.process(droplet8());
    expect(detector.calls == 2 && continuing.detection.visibleTrackCount == 1 &&
               continuing.detection.visibleTracks[0].trackId == 7 &&
               continuing.detection.endedTrackCount == 1 &&
               continuing.detection.endedTrackIds[0] == 3,
           "frame processor preserves independent visible and ended track observations");

    detector.next.enteredTrackCount = 1;
    detector.next.enteredTracks[0] = {8, 0, 1.0, cv::Rect(), {0.0f, 0.0f}};
    detector.next.capacityExceeded = true;
    const DropletFrameProcessingResult failed = processor.process(frame8());
    expect(detector.calls == 3 && failed.cropFailed && failed.detection.capacityExceeded,
           "frame processor propagates crop failure and detector capacity state");
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
    expect(samePoint(first.centroid, cv::Point2f(21.5f, 33.5f)),
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
    expect(samePoint(first.centroid, cv::Point2f(21.5f, 33.5f)),
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
    expect(shifted.detected && !shifted.fired && shifted.visibleTrackCount == 1,
           "fast short-gap recovery retains the existing track without a duplicate entry");

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
               && !suppressed.detected && !suppressed.fired &&
               suppressed.rejectedCount == 1 &&
               suppressed.rejectedAreas != nullptr &&
               suppressed.rejectedAreas[0] == 480.0 &&
               suppressed.area == 0.0 && suppressed.bbox.empty() &&
               suppressed.mask.empty(),
           "an immediate threshold above 480 surfaces only the factual rejected candidate");
    expect(liveThresholdDetector.isReady(),
           "an immediate threshold update preserves background readiness");
    liveThresholdDetector.setMinimumContourArea(480);
    FastEventResult restored;
    expect(liveThresholdDetector.processFrame(droplet8(), restored)
               && restored.detected && restored.fired && restored.rejectedCount == 0,
           "a rejected candidate does not become a track before the qualified frame");

    FastEventConfig mixedConfig = cfg;
    mixedConfig.minArea = 100.0;
    FastEventDetector mixedDetector(mixedConfig);
    expect(!mixedDetector.addBackgroundFrame(frame8()) &&
               mixedDetector.addBackgroundFrame(frame8()),
           "mixed-candidate detector establishes its background");
    FastEventResult mixed;
    expect(mixedDetector.processFrame(mixedCandidates8(), mixed) &&
               mixed.detected && mixed.fired && mixed.area == 300.0 &&
               mixed.bbox == cv::Rect(16, 20, 20, 15) &&
               mixed.rejectedCount == 2 && mixed.rejectedAreas != nullptr &&
               mixed.rejectedAreas[0] == 30.0 && mixed.rejectedAreas[1] == 56.0,
           "one accepted result coexists with every ordered undersized candidate");
    const double* rejectedStorage = mixed.rejectedAreas;
    FastEventResult mixedRepeated;
    expect(mixedDetector.processFrame(mixedCandidates8(), mixedRepeated) &&
               mixedRepeated.rejectedCount == 2 &&
               mixedRepeated.rejectedAreas == rejectedStorage,
           "the rejected-candidate buffer is reused without steady-state frame allocation");
    const auto replayStarted = std::chrono::steady_clock::now();
    for (int index = 0; index < 2000; ++index) {
        FastEventResult replay;
        expect(mixedDetector.processFrame(mixedCandidates8(), replay) &&
                   replay.rejectedCount == 2 &&
                   replay.rejectedAreas == rejectedStorage,
               "bounded mixed-candidate replay preserves reusable storage");
    }
    const auto replayElapsed =
        std::chrono::steady_clock::now() - replayStarted;
    expect(replayElapsed < std::chrono::seconds(5),
           "bounded mixed-candidate replay has no material hot-path regression");

    FastEventConfig aggregateConfig = cfg;
    aggregateConfig.maxAreaFrac = 0.1;
    FastEventDetector aggregateDetector(aggregateConfig);
    expect(!aggregateDetector.addBackgroundFrame(frame8()) &&
               aggregateDetector.addBackgroundFrame(frame8()),
           "aggregate-area detector establishes its background");
    const cv::Rect aggregateFirst(10, 20, 24, 20);
    const cv::Rect aggregateSecond(60, 20, 24, 20);
    FastEventResult aggregate;
    expect(aggregateDetector.processFrame(twoCandidates8(aggregateFirst, aggregateSecond), aggregate) &&
               aggregate.detected && aggregate.fired && aggregate.area == 480.0 &&
               aggregate.bbox == aggregateFirst,
           "two individually valid components survive an aggregate mask area above the per-component cap");

    FastEventDetector rankSwapDetector(cfg);
    expect(!rankSwapDetector.addBackgroundFrame(frame8()) &&
               rankSwapDetector.addBackgroundFrame(frame8()),
           "rank-swap detector establishes its background");
    const cv::Rect trackedLarge(10, 24, 20, 15);
    const cv::Rect remoteSmall(60, 24, 20, 14);
    const cv::Rect trackedSmall(11, 24, 20, 14);
    const cv::Rect remoteLarge(60, 24, 20, 15);
    FastEventResult rankStart;
    FastEventResult rankSwap;
    expect(rankSwapDetector.processFrame(twoCandidates8(trackedLarge, remoteSmall), rankStart) &&
               rankStart.detected && rankStart.fired && rankStart.bbox == trackedLarge &&
               rankSwapDetector.processFrame(twoCandidates8(trackedSmall, remoteLarge), rankSwap) &&
               rankSwap.detected && !rankSwap.fired && rankSwap.area == 280.0 &&
               rankSwap.bbox == trackedSmall,
           "an uninterrupted active track keeps the nearest component across a size-rank swap");

    FastEventDetector gapSelectionDetector(cfg);
    expect(!gapSelectionDetector.addBackgroundFrame(frame8()) &&
               gapSelectionDetector.addBackgroundFrame(frame8()),
           "gap-selection detector establishes its background");
    FastEventResult gapSelectionStart;
    FastEventResult gapSelectionMiss;
    FastEventResult gapSelectionReentry;
    expect(gapSelectionDetector.processFrame(droplet8(trackedLarge), gapSelectionStart) &&
               gapSelectionStart.detected && gapSelectionStart.fired &&
               gapSelectionDetector.processFrame(frame8(), gapSelectionMiss) &&
               !gapSelectionMiss.detected && !gapSelectionMiss.lifecycleEnded &&
               gapSelectionDetector.processFrame(twoCandidates8(trackedSmall, remoteLarge), gapSelectionReentry) &&
               gapSelectionReentry.detected && !gapSelectionReentry.fired &&
               gapSelectionReentry.visibleTrackCount == 1 &&
               gapSelectionReentry.bbox == trackedSmall,
           "after a short gap, an existing track resumes without switching to a remote candidate");

    FastEventDetector tieDetector(cfg);
    expect(!tieDetector.addBackgroundFrame(frame8()) && tieDetector.addBackgroundFrame(frame8()),
           "tie detector establishes its background");
    FastEventResult tieStart;
    FastEventResult tie;
    const cv::Rect tieStartRect(10, 24, 8, 10);
    const cv::Rect tieFirst(28, 24, 8, 10);
    const cv::Rect tieSecond(60, 24, 8, 10);
    expect(tieDetector.processFrame(droplet8(tieStartRect), tieStart) && tieStart.detected &&
               tieDetector.processFrame(twoCandidates8(tieFirst, tieSecond), tie) &&
               tie.detected && tie.bbox == tieFirst,
           "equal-distance equal-area active-track candidates retain the first component label");

    FastEventConfig mergedConfig = cfg;
    mergedConfig.maxAreaFrac = 0.1;
    mergedConfig.morphRadius = 1;
    FastEventDetector mergedDetector(mergedConfig);
    expect(!mergedDetector.addBackgroundFrame(frame8()) && mergedDetector.addBackgroundFrame(frame8()),
           "merged-component detector establishes its background");
    FastEventResult merged;
    expect(mergedDetector.processFrame(twoCandidates8(cv::Rect(10, 20, 20, 20),
                                                       cv::Rect(31, 20, 20, 20)), merged) &&
               !merged.detected && !merged.fired,
           "a morphologically merged component still fails its own maximum-area rule");

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
    verifyFrameProcessor();
    characterizePreciseDetector();
    characterizeFastDetector();
    if (failures != 0) {
        std::cerr << failures << " detector characterization assertion(s) failed\n";
        return 1;
    }
    std::cout << "Detector characterization passed.\n";
    return 0;
}
