#include "event_detector.h"

#include <algorithm>
#include <cctype>
#include <opencv2/imgproc.hpp>

namespace {
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

bool isLocalMode(const std::string& mode) {
    return mode == "local" || mode == "self";
}

bool computeClipRange(const cv::Mat& src, double clip, float& low, float& high) {
    double minv = 0.0, maxv = 0.0;
    cv::minMaxLoc(src, &minv, &maxv);
    low = static_cast<float>(minv);
    high = static_cast<float>(maxv);
    if (clip <= 0.0 || clip >= 0.5) {
        return high > low;
    }

    std::vector<float> values;
    values.reserve(src.total());
    for (int r = 0; r < src.rows; ++r) {
        const float* row = src.ptr<float>(r);
        values.insert(values.end(), row, row + src.cols);
    }
    if (values.empty()) {
        return false;
    }

    size_t total = values.size();
    size_t lowIdx = static_cast<size_t>(clip * static_cast<double>(total - 1));
    size_t highIdx = static_cast<size_t>((1.0 - clip) * static_cast<double>(total - 1));
    if (lowIdx >= highIdx) {
        return high > low;
    }

    std::nth_element(values.begin(), values.begin() + static_cast<long long>(lowIdx), values.end());
    low = values[lowIdx];
    std::nth_element(values.begin(), values.begin() + static_cast<long long>(highIdx), values.end());
    high = values[highIdx];
    if (high <= low) {
        low = static_cast<float>(minv);
        high = static_cast<float>(maxv);
    }
    return high > low;
}

cv::Mat normalizeForOtsu(const cv::Mat& src, double clip) {
    if (src.empty()) return cv::Mat();
    float low = 0.0f;
    float high = 0.0f;
    if (!computeClipRange(src, clip, low, high) || high <= low) {
        return cv::Mat::zeros(src.size(), CV_8U);
    }
    float scale = 1.0f / (high - low);
    cv::Mat norm = (src - low) * scale;
    cv::min(norm, 1.0, norm);
    cv::max(norm, 0.0, norm);
    cv::Mat out;
    norm.convertTo(out, CV_8U, 255.0);
    return out;
}
} // namespace

EventDetector::EventDetector(const EventDetectorConfig& cfg)
    : cfg_(cfg) {}

bool EventDetector::buildBackground(const std::vector<cv::Mat>& frames, std::string& err) {
    std::string mode = toLower(cfg_.bgMode);
    if (frames.empty()) {
        if (isLocalMode(mode)) {
            background_.release();
            return true;
        }
        err = "no frames provided for background";
        return false;
    }

    cv::Mat first = frames.front();
    if (first.empty()) {
        err = "background frame is empty";
        return false;
    }

    cv::Mat first8;
    if (first.type() != CV_8UC1) {
        first.convertTo(first8, CV_8U);
    } else {
        first8 = first;
    }

    std::vector<cv::Mat> validFrames;
    validFrames.reserve(frames.size());
    for (const auto& f : frames) {
        if (f.empty()) continue;
        if (f.size() != first8.size()) continue;
        cv::Mat f8;
        if (f.type() != CV_8UC1) {
            f.convertTo(f8, CV_8U);
        } else {
            f8 = f;
        }
        validFrames.push_back(f8);
    }
    if (validFrames.empty()) {
        err = "no background frames matched size";
        return false;
    }

    if (isLocalMode(mode)) {
        background_.release();
        return true;
    }
    if (mode == "max" || mode == "min") {
        cv::Mat agg = validFrames[0].clone();
        for (size_t i = 1; i < validFrames.size(); ++i) {
            if (mode == "max") {
                cv::max(agg, validFrames[i], agg);
            } else {
                cv::min(agg, validFrames[i], agg);
            }
        }
        agg.convertTo(background_, CV_32F, 1.0 / 255.0);
        return true;
    }
    if (mode == "median") {
        size_t totalElems = static_cast<size_t>(validFrames.size()) * first8.total();
        if (totalElems > cfg_.medianMaxElements) {
            mode = "mean";
        } else {
            cv::Mat stacked(static_cast<int>(validFrames.size()), static_cast<int>(first8.total()), CV_8U);
            for (size_t i = 0; i < validFrames.size(); ++i) {
                const cv::Mat& f8 = validFrames[i];
                cv::Mat row = stacked.row(static_cast<int>(i));
                cv::Mat flat = f8.reshape(1, 1);
                flat.copyTo(row);
            }
            cv::sort(stacked, stacked, cv::SORT_EVERY_COLUMN + cv::SORT_ASCENDING);
            cv::Mat medianRow = stacked.row(static_cast<int>(validFrames.size() / 2));
            cv::Mat median = medianRow.reshape(1, first8.rows).clone();
            median.convertTo(background_, CV_32F, 1.0 / 255.0);
            return true;
        }
    }

    cv::Mat sum = cv::Mat::zeros(first8.size(), CV_32F);
    for (const auto& f8 : validFrames) {
        cv::accumulate(f8, sum);
    }
    sum /= static_cast<float>(validFrames.size());
    sum.convertTo(background_, CV_32F, 1.0 / 255.0);
    return true;
}

bool EventDetector::hasBackground() const {
    return !background_.empty();
}

const cv::Mat& EventDetector::background() const {
    return background_;
}

EventResult EventDetector::detect(const cv::Mat& gray8, bool includeMask) {
    EventResult result;
    if (gray8.empty()) return result;

    cv::Mat gray;
    if (gray8.type() != CV_8UC1) {
        gray8.convertTo(gray, CV_8U);
    } else {
        gray = gray8;
    }

    cv::Mat grayF;
    gray.convertTo(grayF, CV_32F, 1.0 / 255.0);
    std::string mode = toLower(cfg_.bgMode);
    cv::Mat diff;
    if (isLocalMode(mode)) {
        cv::Mat blur;
        cv::GaussianBlur(grayF, blur, cv::Size(0, 0), cfg_.sigma);
        diff = grayF - blur;
    } else {
        if (background_.empty()) return result;
        if (background_.size() != grayF.size()) return result;
        diff = cv::abs(grayF - background_);
        cv::GaussianBlur(diff, diff, cv::Size(0, 0), cfg_.sigma);
    }

    cv::Mat diffNorm = normalizeForOtsu(diff, cfg_.contrastClip);

    cv::Mat mask;
    cv::threshold(diffNorm, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    int radius = std::max(1, cfg_.morphRadius);
    cv::Mat k = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2 * radius + 1, 2 * radius + 1));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, k);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, k);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::Mat cleaned = cv::Mat::zeros(mask.size(), CV_8U);
    std::vector<std::vector<cv::Point>> filtered;
    filtered.reserve(contours.size());
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= cfg_.minMaskArea) {
            filtered.push_back(c);
            cv::drawContours(cleaned, std::vector<std::vector<cv::Point>>{c}, -1, cv::Scalar(255), cv::FILLED);
        }
    }

    if (includeMask) {
        result.mask = cleaned;
    }
    if (filtered.empty()) {
        return result;
    }

    double imgArea = static_cast<double>(gray.rows) * static_cast<double>(gray.cols);
    double bestArea = 0.0;
    int bestIdx = -1;
    cv::Rect bestBbox;
    for (size_t i = 0; i < filtered.size(); ++i) {
        double area = cv::contourArea(filtered[i]);
        if (area < cfg_.minArea || area > cfg_.maxAreaFrac * imgArea) {
            continue;
        }
        cv::Rect bbox = cv::boundingRect(filtered[i]);
        if (bbox.x <= cfg_.borderMargin ||
            bbox.y <= cfg_.borderMargin ||
            (bbox.x + bbox.width) >= (gray.cols - cfg_.borderMargin) ||
            (bbox.y + bbox.height) >= (gray.rows - cfg_.borderMargin)) {
            continue;
        }
        if (area > bestArea) {
            bestArea = area;
            bestIdx = static_cast<int>(i);
            bestBbox = bbox;
        }
    }

    if (bestIdx < 0) {
        return result;
    }

    cv::Moments m = cv::moments(filtered[bestIdx]);
    cv::Point2f centroid(0.0f, 0.0f);
    if (m.m00 != 0.0) {
        centroid.x = static_cast<float>(m.m10 / m.m00);
        centroid.y = static_cast<float>(m.m01 / m.m00);
    }

    result.detected = true;
    result.area = bestArea;
    result.bbox = bestBbox;
    result.centroid = centroid;
    return result;
}
