#include "fast_event_detector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {
cv::Rect scaleRect(const cv::Rect& r, double scale) {
    return cv::Rect(static_cast<int>(std::lround(r.x * scale)), static_cast<int>(std::lround(r.y * scale)),
                    static_cast<int>(std::lround(r.width * scale)), static_cast<int>(std::lround(r.height * scale)));
}

bool isInsideFrame(const cv::Rect& bbox, const cv::Size& size, int margin) {
    return bbox.x > margin && bbox.y > margin && (bbox.x + bbox.width) < (size.width - margin) &&
           (bbox.y + bbox.height) < (size.height - margin);
}

cv::Mat computeMean8(const std::vector<cv::Mat>& frames) {
    if (frames.empty())
        return cv::Mat();
    cv::Mat sum = cv::Mat::zeros(frames[0].size(), CV_32S);
    int used = 0;
    for (const auto& f : frames) {
        if (f.empty() || f.size() != frames[0].size())
            continue;
        sum += f;
        used++;
    }
    if (used == 0)
        return cv::Mat();
    cv::Mat mean;
    sum.convertTo(mean, CV_8U, 1.0 / static_cast<double>(used));
    return mean;
}

struct Candidate {
    int label = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
};

struct CandidateFrame {
    cv::Mat mask;
    std::array<Candidate, kFastEventCandidateCapacity> candidates{};
    std::size_t count = 0;
    bool capacityExceeded = false;
};

CandidateFrame detectCandidatesFromDiffFast(const cv::Mat& diff8, int minArea,
                                            int minAreaByFrac, int maxArea, int margin,
                                            int diffThresh, int minBbox,
                                            const cv::Mat& morphKernel,
                                            std::vector<double>& rejectedAreas,
                                            double inverseScale, double areaScale,
                                            const cv::Size& sourceSize, int sourceMargin,
                                            int sourceMinBbox) {
    CandidateFrame result;
    if (diff8.empty())
        return result;

    cv::Mat mask;
    cv::threshold(diff8, mask, diffThresh, 255, cv::THRESH_BINARY);
    if (!morphKernel.empty()) {
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, morphKernel);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, morphKernel);
    }

    int nonZero = cv::countNonZero(mask);
    if (nonZero < minAreaByFrac) {
        return result;
    }
    result.mask = mask;

    cv::Mat labels, stats, centroids;
    int count = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    if (count <= 1)
        return result;
    for (int i = 1; i < count; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < minAreaByFrac || area > maxArea)
            continue;
        int x = stats.at<int>(i, cv::CC_STAT_LEFT);
        int y = stats.at<int>(i, cv::CC_STAT_TOP);
        int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        cv::Rect bbox(x, y, w, h);
        if (bbox.width < minBbox || bbox.height < minBbox)
            continue;
        if (!isInsideFrame(bbox, diff8.size(), margin))
            continue;
        if (area < minArea) {
            const cv::Rect sourceBbox = scaleRect(bbox, inverseScale);
            if (sourceBbox.width >= sourceMinBbox &&
                sourceBbox.height >= sourceMinBbox &&
                isInsideFrame(sourceBbox, sourceSize, sourceMargin)) {
                rejectedAreas.push_back(static_cast<double>(area) / areaScale);
            }
            continue;
        }
        const cv::Rect sourceBbox = scaleRect(bbox, inverseScale);
        if (sourceBbox.width < sourceMinBbox || sourceBbox.height < sourceMinBbox ||
            !isInsideFrame(sourceBbox, sourceSize, sourceMargin))
            continue;
        if (result.count == result.candidates.size()) {
            result.capacityExceeded = true;
            continue;
        }
        Candidate& candidate = result.candidates[result.count++];
        candidate.label = i;
        candidate.area = static_cast<double>(area) / areaScale;
        candidate.bbox = sourceBbox;
        candidate.centroid = cv::Point2f(
            static_cast<float>(centroids.at<double>(i, 0) * inverseScale),
            static_cast<float>(centroids.at<double>(i, 1) * inverseScale));
    }
    return result;
}

FastEventTrackObservation observation(int id, int missedFrames, const Candidate& candidate) {
    return {id, missedFrames, candidate.area, candidate.bbox, candidate.centroid};
}
} // namespace

FastEventDetector::FastEventDetector(const FastEventConfig& cfg) : cfg_(cfg) {
    setMinimumContourArea(
        cfg.minArea <= 0.0 ? 100 : static_cast<int>(std::lround(cfg.minArea)));
    reset();
}

int FastEventDetector::minimumContourArea() const noexcept {
    return minimumContourArea_.load(std::memory_order_acquire);
}

void FastEventDetector::setMinimumContourArea(int area) noexcept {
    minimumContourArea_.store(area <= 0 ? 100 : area,
                              std::memory_order_release);
}

void FastEventDetector::reset() {
    ready_ = false;
    collected_ = 0;
    fullSize_ = cv::Size();
    backgroundScaled_.release();
    rolling_.frames.clear();
    rolling_.sum.release();
    bgStack_.clear();
    tracks_ = {};
    nextTrackId_ = 1;

    if (cfg_.scale <= 0.0 || cfg_.scale > 1.0) {
        cfg_.scale = 1.0;
    }
    cfg_.minAreaFrac = std::max(0.0, std::min(cfg_.minAreaFrac, 1.0));
    cfg_.maxAreaFrac = std::max(0.0, std::min(cfg_.maxAreaFrac, 1.0));
    cfg_.bgFrames = std::max(1, cfg_.bgFrames);
    cfg_.resetFrames = std::max(1, cfg_.resetFrames);

    if (cfg_.bgUpdateFrames < 0)
        cfg_.bgUpdateFrames = 0;
    initFrames_ = cfg_.bgFrames;
    if (cfg_.bgUpdateFrames > 0) {
        initFrames_ = std::min(cfg_.bgFrames, cfg_.bgUpdateFrames);
        rolling_.maxFrames = cfg_.bgUpdateFrames;
    }

    if (cfg_.morphRadius > 0) {
        int k = 2 * cfg_.morphRadius + 1;
        morphKernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k));
    } else {
        morphKernel_.release();
    }
}

bool FastEventDetector::isReady() const {
    return ready_;
}

int FastEventDetector::backgroundFramesRemaining() const {
    if (ready_)
        return 0;
    return std::max(0, initFrames_ - collected_);
}

const cv::Mat& FastEventDetector::background() const {
    return backgroundScaled_;
}

cv::Mat FastEventDetector::toGray8Fast(const cv::Mat& src) const {
    if (src.empty())
        return cv::Mat();
    if (src.type() == CV_8UC1)
        return src;
    cv::Mat gray = src;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }
    cv::Mat out;
    if (gray.type() == CV_16UC1) {
        gray.convertTo(out, CV_8U, 1.0 / 256.0);
    } else {
        gray.convertTo(out, CV_8U);
    }
    return out;
}

bool FastEventDetector::updateRollingBackground(const cv::Mat& gray8Scaled) {
    if (gray8Scaled.empty())
        return false;
    if (rolling_.sum.empty()) {
        rolling_.sum = cv::Mat::zeros(gray8Scaled.size(), CV_32S);
    } else if (rolling_.sum.size() != gray8Scaled.size()) {
        return false;
    }
    rolling_.frames.push_back(gray8Scaled);
    rolling_.sum += gray8Scaled;
    if (static_cast<int>(rolling_.frames.size()) > rolling_.maxFrames) {
        rolling_.sum -= rolling_.frames.front();
        rolling_.frames.pop_front();
    }
    rolling_.sum.convertTo(backgroundScaled_, CV_8U, 1.0 / static_cast<double>(rolling_.frames.size()));
    return true;
}

void FastEventDetector::updateDerivedParams(const cv::Size& fullSize, const cv::Size& scaledSize) {
    if (fullSize.area() <= 0 || scaledSize.area() <= 0)
        return;

    areaScale_ = cfg_.scale * cfg_.scale;

    int imgAreaScaled = scaledSize.width * scaledSize.height;
    minAreaByFracScaled_ = static_cast<int>(std::lround(cfg_.minAreaFrac * static_cast<double>(imgAreaScaled)));
    maxAreaScaled_ = static_cast<int>(std::lround(cfg_.maxAreaFrac * static_cast<double>(imgAreaScaled)));
    if (minAreaByFracScaled_ < 0)
        minAreaByFracScaled_ = 0;
    marginScaled_ = std::max(1, static_cast<int>(std::lround(cfg_.margin * cfg_.scale)));
    minBboxScaled_ = std::max(1, static_cast<int>(std::lround(cfg_.minBbox * cfg_.scale)));

    gapFireShift_ = cfg_.gapFireShift;
    if (gapFireShift_ <= 0 && fullSize.area() > 0) {
        int minDim = std::min(fullSize.width, fullSize.height);
        gapFireShift_ = std::max(cfg_.minBbox * 2, static_cast<int>(std::lround(0.1 * static_cast<double>(minDim))));
    }
}

bool FastEventDetector::addBackgroundFrame(const cv::Mat& gray8In) {
    if (ready_)
        return true;
    if (gray8In.empty())
        return false;

    cv::Mat gray8 = toGray8Fast(gray8In);
    if (gray8.empty())
        return false;

    if (fullSize_.area() == 0) {
        fullSize_ = gray8.size();
    }

    cv::Mat gray8Scaled = gray8;
    if (cfg_.scale != 1.0) {
        cv::resize(gray8, gray8Scaled, cv::Size(), cfg_.scale, cfg_.scale, cv::INTER_AREA);
    }

    if (cfg_.bgUpdateFrames > 0) {
        if (!updateRollingBackground(gray8Scaled))
            return false;
    } else {
        bgStack_.push_back(gray8Scaled);
    }

    collected_++;
    if (collected_ >= initFrames_) {
        if (cfg_.bgUpdateFrames == 0) {
            backgroundScaled_ = computeMean8(bgStack_);
        }
        if (!backgroundScaled_.empty()) {
            updateDerivedParams(fullSize_, backgroundScaled_.size());
            ready_ = true;
            bgStack_.clear();
        }
    }
    return ready_;
}

bool FastEventDetector::processFrame(const cv::Mat& gray8In, FastEventResult& out) {
    out = FastEventResult{};
    rejectedAreas_.clear();
    if (gray8In.empty())
        return false;
    if (!ready_) {
        addBackgroundFrame(gray8In);
        return false;
    }

    cv::Mat gray8 = toGray8Fast(gray8In);
    if (gray8.empty())
        return false;

    cv::Mat gray8Scaled = gray8;
    if (cfg_.scale != 1.0) {
        cv::resize(gray8, gray8Scaled, cv::Size(), cfg_.scale, cfg_.scale, cv::INTER_AREA);
    }
    if (gray8Scaled.size() != backgroundScaled_.size()) {
        return false;
    }

    cv::Mat diff8;
    cv::absdiff(gray8Scaled, backgroundScaled_, diff8);
    if (cfg_.blurRadius > 0) {
        int k = 2 * cfg_.blurRadius + 1;
        cv::blur(diff8, diff8, cv::Size(k, k));
    }

    const int minAreaScaled = std::max(
        1, static_cast<int>(std::ceil(
               minimumContourArea_.load(std::memory_order_acquire) * areaScale_)));
    CandidateFrame candidates = detectCandidatesFromDiffFast(
        diff8, minAreaScaled, minAreaByFracScaled_,
        std::max(maxAreaScaled_, minAreaScaled), marginScaled_,
        cfg_.diffThresh, minBboxScaled_, morphKernel_, rejectedAreas_,
        1.0 / cfg_.scale, areaScale_, gray8.size(), cfg_.margin, cfg_.minBbox);

    std::array<bool, kFastEventCandidateCapacity> matched{};
    for (TrackState& track : tracks_) {
        if (!track.active)
            continue;
        int selected = -1;
        double selectedDistanceSquared = 0.0;
        for (std::size_t candidateIndex = 0; candidateIndex < candidates.count;
             ++candidateIndex) {
            if (matched[candidateIndex])
                continue;
            const Candidate& candidate = candidates.candidates[candidateIndex];
            const int extent = (std::max)(
                (std::max)(track.bbox.width, track.bbox.height),
                (std::max)(candidate.bbox.width, candidate.bbox.height));
            const double backwardAllowance = static_cast<double>(extent) * 0.5;
            if (candidate.centroid.x < track.centroid.x - backwardAllowance)
                continue;
            const double maximumDisplacement = static_cast<double>(extent) * 2.0;
            const double dx = static_cast<double>(candidate.centroid.x - track.centroid.x);
            const double dy = static_cast<double>(candidate.centroid.y - track.centroid.y);
            const double distanceSquared = dx * dx + dy * dy;
            if (distanceSquared > maximumDisplacement * maximumDisplacement)
                continue;
            if (selected < 0 || distanceSquared < selectedDistanceSquared ||
                (distanceSquared == selectedDistanceSquared &&
                 candidate.label < candidates.candidates[selected].label)) {
                selected = static_cast<int>(candidateIndex);
                selectedDistanceSquared = distanceSquared;
            }
        }
        if (selected >= 0) {
            const Candidate& candidate = candidates.candidates[selected];
            matched[static_cast<std::size_t>(selected)] = true;
            track.area = candidate.area;
            track.bbox = candidate.bbox;
            track.centroid = candidate.centroid;
            track.missedFrames = 0;
        } else if (++track.missedFrames >= cfg_.resetFrames) {
            if (out.endedTrackCount < out.endedTrackIds.size())
                out.endedTrackIds[out.endedTrackCount++] = track.id;
            track = TrackState{};
        }
    }

    const int entryRight = static_cast<int>(std::ceil(gray8.cols * 0.20));
    for (std::size_t candidateIndex = 0; candidateIndex < candidates.count; ++candidateIndex) {
        if (matched[candidateIndex])
            continue;
        const Candidate& candidate = candidates.candidates[candidateIndex];
        if (candidate.bbox.x >= entryRight || candidate.bbox.x + candidate.bbox.width <= 0)
            continue;
        TrackState* slot = nullptr;
        for (TrackState& track : tracks_) {
            if (!track.active) {
                slot = &track;
                break;
            }
        }
        if (!slot) {
            candidates.capacityExceeded = true;
            continue;
        }
        slot->active = true;
        slot->id = nextTrackId_++;
        slot->missedFrames = 0;
        slot->area = candidate.area;
        slot->bbox = candidate.bbox;
        slot->centroid = candidate.centroid;
        if (out.enteredTrackCount < out.enteredTracks.size())
            out.enteredTracks[out.enteredTrackCount++] = observation(slot->id, 0, candidate);
    }

    for (const TrackState& track : tracks_) {
        if (!track.active || track.missedFrames != 0)
            continue;
        if (out.visibleTrackCount < out.visibleTracks.size()) {
            out.visibleTracks[out.visibleTrackCount++] =
                {track.id, track.missedFrames, track.area, track.bbox, track.centroid};
        }
    }

    FastEventResult det;
    det.mask = candidates.mask;
    det.visibleTracks = out.visibleTracks;
    det.visibleTrackCount = out.visibleTrackCount;
    det.enteredTracks = out.enteredTracks;
    det.enteredTrackCount = out.enteredTrackCount;
    det.endedTrackIds = out.endedTrackIds;
    det.endedTrackCount = out.endedTrackCount;
    det.capacityExceeded = candidates.capacityExceeded;
    det.detected = det.visibleTrackCount > 0;
    det.fired = det.enteredTrackCount > 0;
    det.lifecycleEnded = det.endedTrackCount > 0;
    if (det.enteredTrackCount > 0) {
        const FastEventTrackObservation& primary = det.enteredTracks[0];
        det.area = primary.area;
        det.bbox = primary.bbox;
        det.centroid = primary.centroid;
    } else if (det.visibleTrackCount > 0) {
        const FastEventTrackObservation& primary = det.visibleTracks[0];
        det.area = primary.area;
        det.bbox = primary.bbox;
        det.centroid = primary.centroid;
    }
    if (!det.detected)
        det.mask.release();

    bool anyTrackActive = false;
    for (const TrackState& track : tracks_) {
        if (track.active) {
            anyTrackActive = true;
            break;
        }
    }
    if (cfg_.bgUpdateFrames > 0 && !anyTrackActive && candidates.count == 0) {
        updateRollingBackground(gray8Scaled);
    }

    det.rejectedAreas = rejectedAreas_.empty() ? nullptr : rejectedAreas_.data();
    det.rejectedCount = rejectedAreas_.size();
    out = det;
    return true;
}
