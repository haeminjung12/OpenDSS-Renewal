#include "fast_event_detector.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {
constexpr double kAutoMinAreaFracFast = 0.006;

cv::Rect scaleRect(const cv::Rect& r, double scale) {
    return cv::Rect(
        static_cast<int>(std::lround(r.x * scale)),
        static_cast<int>(std::lround(r.y * scale)),
        static_cast<int>(std::lround(r.width * scale)),
        static_cast<int>(std::lround(r.height * scale))
    );
}

bool isInsideFrame(const cv::Rect& bbox, const cv::Size& size, int margin) {
    return bbox.x > margin &&
           bbox.y > margin &&
           (bbox.x + bbox.width) < (size.width - margin) &&
           (bbox.y + bbox.height) < (size.height - margin);
}

cv::Mat computeMean8(const std::vector<cv::Mat>& frames) {
    if (frames.empty()) return cv::Mat();
    cv::Mat sum = cv::Mat::zeros(frames[0].size(), CV_32S);
    int used = 0;
    for (const auto& f : frames) {
        if (f.empty() || f.size() != frames[0].size()) continue;
        sum += f;
        used++;
    }
    if (used == 0) return cv::Mat();
    cv::Mat mean;
    sum.convertTo(mean, CV_8U, 1.0 / static_cast<double>(used));
    return mean;
}

FastEventResult detectFromDiffFast(const cv::Mat& diff8,
                                   int minArea,
                                   int minAreaByFrac,
                                   int maxArea,
                                   int margin,
                                   int diffThresh,
                                   int minBbox,
                                   const cv::Mat& morphKernel) {
    FastEventResult det;
    if (diff8.empty()) return det;

    cv::Mat mask;
    cv::threshold(diff8, mask, diffThresh, 255, cv::THRESH_BINARY);
    if (!morphKernel.empty()) {
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, morphKernel);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, morphKernel);
    }

    int nonZero = cv::countNonZero(mask);
    if (nonZero < minArea || nonZero < minAreaByFrac || nonZero > maxArea) {
        return det;
    }
    det.mask = mask;

    cv::Mat labels, stats, centroids;
    int count = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
    if (count <= 1) return det;

    int bestIdx = -1;
    int bestArea = 0;
    for (int i = 1; i < count; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < minArea || area < minAreaByFrac || area > maxArea) continue;
        int x = stats.at<int>(i, cv::CC_STAT_LEFT);
        int y = stats.at<int>(i, cv::CC_STAT_TOP);
        int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        cv::Rect bbox(x, y, w, h);
        if (bbox.width < minBbox || bbox.height < minBbox) continue;
        if (!isInsideFrame(bbox, diff8.size(), margin)) continue;
        if (area > bestArea) {
            bestArea = area;
            bestIdx = i;
            det.bbox = bbox;
        }
    }

    if (bestIdx < 0) return det;
    det.detected = true;
    det.area = static_cast<double>(bestArea);
    det.centroid.x = static_cast<float>(centroids.at<double>(bestIdx, 0));
    det.centroid.y = static_cast<float>(centroids.at<double>(bestIdx, 1));
    return det;
}
} // namespace

FastEventDetector::FastEventDetector(const FastEventConfig& cfg)
    : cfg_(cfg) {
    reset();
}

void FastEventDetector::reset() {
    ready_ = false;
    collected_ = 0;
    fullSize_ = cv::Size();
    backgroundScaled_.release();
    rolling_.frames.clear();
    rolling_.sum.release();
    bgStack_.clear();
    triggered_ = false;
    noDetectCount_ = 0;
    hasLastDet_ = false;
    lastCentroid_ = cv::Point2f(0.0f, 0.0f);

    if (cfg_.scale <= 0.0 || cfg_.scale > 1.0) {
        cfg_.scale = 1.0;
    }
    cfg_.minAreaFrac = std::max(0.0, std::min(cfg_.minAreaFrac, 1.0));
    cfg_.maxAreaFrac = std::max(0.0, std::min(cfg_.maxAreaFrac, 1.0));
    cfg_.bgFrames = std::max(1, cfg_.bgFrames);

    if (cfg_.bgUpdateFrames < 0) cfg_.bgUpdateFrames = 0;
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
    if (ready_) return 0;
    return std::max(0, initFrames_ - collected_);
}

const cv::Mat& FastEventDetector::background() const {
    return backgroundScaled_;
}

cv::Mat FastEventDetector::toGray8Fast(const cv::Mat& src) const {
    if (src.empty()) return cv::Mat();
    if (src.type() == CV_8UC1) return src;
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
    if (gray8Scaled.empty()) return false;
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
    rolling_.sum.convertTo(backgroundScaled_, CV_8U,
                           1.0 / static_cast<double>(rolling_.frames.size()));
    return true;
}

void FastEventDetector::updateDerivedParams(const cv::Size& fullSize, const cv::Size& scaledSize) {
    if (fullSize.area() <= 0 || scaledSize.area() <= 0) return;

    double minArea = cfg_.minArea;
    if (minArea <= 0.0) {
        minArea = kAutoMinAreaFracFast * static_cast<double>(fullSize.area());
    }

    areaScale_ = cfg_.scale * cfg_.scale;
    double minAreaScaled = std::max(1.0, minArea * areaScale_);
    minAreaScaled_ = static_cast<int>(std::ceil(minAreaScaled));

    int imgAreaScaled = scaledSize.width * scaledSize.height;
    minAreaByFracScaled_ = static_cast<int>(std::lround(cfg_.minAreaFrac * static_cast<double>(imgAreaScaled)));
    maxAreaScaled_ = static_cast<int>(std::lround(cfg_.maxAreaFrac * static_cast<double>(imgAreaScaled)));
    if (minAreaByFracScaled_ < 0) minAreaByFracScaled_ = 0;
    if (maxAreaScaled_ < minAreaScaled_) maxAreaScaled_ = minAreaScaled_;

    marginScaled_ = std::max(1, static_cast<int>(std::lround(cfg_.margin * cfg_.scale)));
    minBboxScaled_ = std::max(1, static_cast<int>(std::lround(cfg_.minBbox * cfg_.scale)));

    gapFireShift_ = cfg_.gapFireShift;
    if (gapFireShift_ <= 0 && fullSize.area() > 0) {
        int minDim = std::min(fullSize.width, fullSize.height);
        gapFireShift_ = std::max(cfg_.minBbox * 2,
                                 static_cast<int>(std::lround(0.1 * static_cast<double>(minDim))));
    }
}

bool FastEventDetector::addBackgroundFrame(const cv::Mat& gray8In) {
    if (ready_) return true;
    if (gray8In.empty()) return false;

    cv::Mat gray8 = toGray8Fast(gray8In);
    if (gray8.empty()) return false;

    if (fullSize_.area() == 0) {
        fullSize_ = gray8.size();
    }

    cv::Mat gray8Scaled = gray8;
    if (cfg_.scale != 1.0) {
        cv::resize(gray8, gray8Scaled, cv::Size(), cfg_.scale, cfg_.scale, cv::INTER_AREA);
    }

    if (cfg_.bgUpdateFrames > 0) {
        if (!updateRollingBackground(gray8Scaled)) return false;
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
    if (gray8In.empty()) return false;
    if (!ready_) {
        addBackgroundFrame(gray8In);
        return false;
    }

    cv::Mat gray8 = toGray8Fast(gray8In);
    if (gray8.empty()) return false;

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

    FastEventResult det = detectFromDiffFast(
        diff8,
        minAreaScaled_,
        minAreaByFracScaled_,
        maxAreaScaled_,
        marginScaled_,
        cfg_.diffThresh,
        minBboxScaled_,
        morphKernel_);

    if (det.detected && cfg_.scale != 1.0) {
        det.bbox = scaleRect(det.bbox, 1.0 / cfg_.scale);
        det.area = det.area / areaScale_;
        det.centroid.x = static_cast<float>(det.centroid.x / cfg_.scale);
        det.centroid.y = static_cast<float>(det.centroid.y / cfg_.scale);
        if (det.bbox.width < cfg_.minBbox || det.bbox.height < cfg_.minBbox) {
            det = FastEventResult{};
        } else if (!isInsideFrame(det.bbox, gray8.size(), cfg_.margin)) {
            det = FastEventResult{};
        }
    }

    bool fired = false;
    if (det.detected) {
        bool gapReentry = (noDetectCount_ > 0);
        bool gapFire = false;
        if (triggered_ && gapReentry && hasLastDet_ && gapFireShift_ > 0) {
            double dx = static_cast<double>(det.centroid.x - lastCentroid_.x);
            double dy = static_cast<double>(det.centroid.y - lastCentroid_.y);
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= static_cast<double>(gapFireShift_)) {
                gapFire = true;
            }
        }

        noDetectCount_ = 0;
        if (!triggered_ || gapFire) {
            fired = true;
            triggered_ = true;
        }
        lastCentroid_ = det.centroid;
        hasLastDet_ = true;
    } else if (triggered_) {
        noDetectCount_++;
        if (noDetectCount_ >= cfg_.resetFrames) {
            triggered_ = false;
            noDetectCount_ = 0;
        }
    }

    if (cfg_.bgUpdateFrames > 0 && !triggered_ && !det.detected) {
        updateRollingBackground(gray8Scaled);
    }

    det.fired = fired;
    out = det;
    return true;
}
