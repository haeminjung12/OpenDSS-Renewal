#include "../detection/droplet_detector_adapters.h"

#include <cmath>
#include <iostream>
#include <string>
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

cv::Mat droplet16() {
    cv::Mat frame = frame16();
    cv::rectangle(frame, kDropletRect, cv::Scalar(51200), cv::FILLED);
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

DropletDetectionFrame mapFast(const FastEventResult& result) {
    DropletDetectionFrame mapped;
    mapped.detected = result.detected;
    mapped.eventEntered = result.fired;
    mapped.area = result.area;
    mapped.bbox = result.bbox;
    mapped.centroid = result.centroid;
    mapped.mask = result.mask;
    return mapped;
}

bool sameDetection(const DropletDetectionFrame& lhs, const DropletDetectionFrame& rhs) {
    return lhs.detected == rhs.detected && lhs.eventEntered == rhs.eventEntered &&
           std::abs(lhs.area - rhs.area) < 0.001 && lhs.bbox == rhs.bbox && samePoint(lhs.centroid, rhs.centroid) &&
           sameMat(lhs.mask, rhs.mask);
}

struct PreciseCallerState {
    bool eventActive = false;
    int noDetectionFrames = 0;
};

DropletDetectionFrame runPreciseDirect(EventDetector& detector, const cv::Mat& frame, bool includeMask,
                                       double minAreaFrac, int minBbox, int resetFrames,
                                       PreciseCallerState& state) {
    const EventResult result = detector.detect(frame, includeMask);
    bool detected = result.detected;
    if (detected) {
        const double imageArea = static_cast<double>(frame.rows) * static_cast<double>(frame.cols);
        if (result.area < (minAreaFrac * imageArea) || result.bbox.width < minBbox || result.bbox.height < minBbox)
            detected = false;
    }

    bool eventEntered = false;
    if (detected) {
        state.noDetectionFrames = 0;
        if (!state.eventActive) {
            eventEntered = true;
            state.eventActive = true;
        }
    } else if (state.eventActive) {
        state.noDetectionFrames++;
        if (state.noDetectionFrames >= resetFrames) {
            state.eventActive = false;
            state.noDetectionFrames = 0;
        }
    }

    DropletDetectionFrame mapped;
    mapped.detected = detected;
    mapped.eventEntered = eventEntered;
    mapped.area = result.area;
    mapped.bbox = result.bbox;
    mapped.centroid = result.centroid;
    mapped.mask = result.mask;
    return mapped;
}

void verifyFastParity() {
    const FastEventConfig cfg = fastConfig();
    FastEventDetector pipelineDirect(cfg);
    FastEventDetectorAdapter pipelineAdapter(cfg);
    for (int i = 0; i < cfg.bgFrames; ++i) {
        FastEventResult directResult;
        pipelineDirect.processFrame(frame8(), directResult);
        expect(sameDetection(mapFast(directResult), pipelineAdapter.processFrame(frame8())),
               "fast direct and wrapper match while processFrame builds background " + std::to_string(i));
        expect(pipelineDirect.backgroundFramesRemaining() == pipelineAdapter.backgroundFramesRemaining(),
               "fast direct and wrapper background countdowns match during processFrame startup");
    }
    FastEventResult pipelineDirectResult;
    pipelineDirect.processFrame(droplet8(), pipelineDirectResult);
    expect(sameDetection(mapFast(pipelineDirectResult), pipelineAdapter.processFrame(droplet8())),
           "fast desktop processFrame startup path preserves first event");

    FastEventDetector direct(cfg);
    FastEventDetectorAdapter adapter(cfg);

    expect(direct.backgroundFramesRemaining() == adapter.backgroundFramesRemaining(),
           "fast direct and wrapper start with the same background countdown");
    expect(direct.addBackgroundFrame(frame8()) == adapter.addBackgroundFrame(frame8()),
           "fast direct and wrapper agree on first background readiness");
    expect(direct.addBackgroundFrame(frame8()) == adapter.addBackgroundFrame(frame8()),
           "fast direct and wrapper agree on final background readiness");
    expect(direct.isReady() == adapter.isReady(), "fast direct and wrapper report the same readiness");
    expect(sameMat(direct.background(), adapter.background()), "fast direct and wrapper backgrounds match");

    const std::vector<cv::Mat> frames{
        cv::Mat(),
        frame8(),
        droplet8(),
        droplet8(),
        frame8(),
        droplet8(),
        frame8(),
        droplet8(cv::Rect(kDropletRect.x + 20, kDropletRect.y, kDropletRect.width, kDropletRect.height)),
        frame8(),
        frame8(),
        droplet8(),
        droplet16(),
    };
    for (std::size_t i = 0; i < frames.size(); ++i) {
        FastEventResult directResult;
        direct.processFrame(frames[i], directResult);
        const DropletDetectionFrame wrappedResult = adapter.processFrame(frames[i]);
        expect(sameDetection(mapFast(directResult), wrappedResult),
               "fast direct-versus-wrapper parity frame " + std::to_string(i));
    }

    direct.reset();
    adapter.reset();
    expect(direct.backgroundFramesRemaining() == adapter.backgroundFramesRemaining(),
           "fast direct and wrapper reset to the same background countdown");
    expect(direct.addBackgroundFrame(frame16()) == adapter.addBackgroundFrame(frame16()),
           "fast direct and wrapper agree on first 16-bit background frame");
    expect(direct.addBackgroundFrame(frame16()) == adapter.addBackgroundFrame(frame16()),
           "fast direct and wrapper agree on final 16-bit background frame");
    FastEventResult direct16;
    direct.processFrame(droplet16(), direct16);
    expect(sameDetection(mapFast(direct16), adapter.processFrame(droplet16())),
           "fast direct and wrapper preserve 16-bit conversion behavior");
}

void verifyPreciseParity() {
    constexpr int resetFrames = 2;
    constexpr double minAreaFrac = 0.0;
    constexpr int minBbox = 12;
    constexpr bool includeMask = true;

    const EventDetectorConfig cfg = preciseConfig();
    EventDetector direct(cfg);
    EventDetectorAdapter adapter(cfg, resetFrames, minAreaFrac, minBbox, includeMask);
    const std::vector<cv::Mat> backgrounds{frame8(), frame8()};
    std::string directError;
    std::string adapterError;
    expect(direct.buildBackground(backgrounds, directError) == adapter.buildBackground(backgrounds, adapterError),
           "precise direct and wrapper agree on background build success");
    expect(directError == adapterError, "precise direct and wrapper background errors match");
    expect(sameMat(direct.background(), adapter.background()), "precise direct and wrapper backgrounds match");
    expect(adapter.backgroundFramesRemaining() == 0, "precise wrapper has no internal background countdown");

    PreciseCallerState directState;
    cv::Mat filteredDroplet = frame8();
    cv::rectangle(filteredDroplet, cv::Rect(34, 28, 10, 10), cv::Scalar(200), cv::FILLED);
    const std::vector<cv::Mat> frames{
        cv::Mat(), filteredDroplet, frame8(), droplet8(), droplet8(), frame8(), droplet8(), frame8(), frame8(),
        droplet8(), droplet16()};
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const DropletDetectionFrame directResult =
            runPreciseDirect(direct, frames[i], includeMask, minAreaFrac, minBbox, resetFrames, directState);
        const DropletDetectionFrame wrappedResult = adapter.processFrame(frames[i]);
        expect(sameDetection(directResult, wrappedResult),
               "precise direct-versus-wrapper parity frame " + std::to_string(i));
    }

    directState = PreciseCallerState{};
    adapter.reset();
    expect(sameDetection(runPreciseDirect(direct, droplet8(), includeMask, minAreaFrac, minBbox, resetFrames,
                                          directState),
                         adapter.processFrame(droplet8())),
           "precise direct caller state and wrapper reset preserve event-entry behavior");
}

} // namespace

int main() {
    verifyFastParity();
    verifyPreciseParity();
    if (failures != 0) {
        std::cerr << failures << " detector contract parity assertion(s) failed\n";
        return 1;
    }
    std::cout << "Detector direct-versus-wrapper parity passed.\n";
    return 0;
}
