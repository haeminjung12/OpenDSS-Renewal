#include "crops/crop_service.h"
#include "detection/droplet_detector_adapters.h"
#include "detection/droplet_frame_processor.h"
#include "fast_event_detector.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct TimingSamples {
    std::vector<double> milliseconds;

    QJsonObject json() const {
        if (milliseconds.empty())
            return {{QStringLiteral("count"), 0}};
        std::vector<double> ordered = milliseconds;
        std::sort(ordered.begin(), ordered.end());
        double total = 0.0;
        for (const double value : milliseconds)
            total += value;
        const auto percentile = [&ordered](double fraction) {
            const auto index = static_cast<std::size_t>(
                std::ceil(fraction * static_cast<double>(ordered.size()))) - 1;
            return ordered[(std::min)(index, ordered.size() - 1)];
        };
        return {
            {QStringLiteral("count"), static_cast<qint64>(milliseconds.size())},
            {QStringLiteral("total_ms"), total},
            {QStringLiteral("average_ms"), total / static_cast<double>(milliseconds.size())},
            {QStringLiteral("p50_ms"), percentile(0.50)},
            {QStringLiteral("p95_ms"), percentile(0.95)},
            {QStringLiteral("p99_ms"), percentile(0.99)},
            {QStringLiteral("max_ms"), percentile(1.00)},
        };
    }
};

constexpr int kPrototypeTrackCapacity = 3;
constexpr int kPrototypeCandidateCapacity = 16;
constexpr double kPrototypeEntryZoneFraction = 0.20;

struct PrototypeCandidate {
    int label = 0;
    double area = 0.0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
};

struct PrototypeTrack {
    bool active = false;
    int id = 0;
    int missedFrames = 0;
    cv::Rect bbox;
    cv::Point2f centroid = {0.0f, 0.0f};
};

struct PrototypeFrameResult {
    std::array<PrototypeCandidate, kPrototypeCandidateCapacity> candidates;
    int candidateCount = 0;
    bool candidateOverflow = false;
    QJsonArray entries;
    QJsonArray tracks;
    int activeTrackCount = 0;
};

QJsonObject rectJson(const cv::Rect& rect);
QJsonObject pointJson(const cv::Point2f& point);

cv::Rect scaledRect(const cv::Rect& rect, double scale) {
    return cv::Rect(static_cast<int>(std::lround(rect.x * scale)),
                    static_cast<int>(std::lround(rect.y * scale)),
                    static_cast<int>(std::lround(rect.width * scale)),
                    static_cast<int>(std::lround(rect.height * scale)));
}

bool rectInsideFrame(const cv::Rect& bbox, const cv::Size& size, int margin) {
    return bbox.x > margin && bbox.y > margin
        && bbox.x + bbox.width < size.width - margin
        && bbox.y + bbox.height < size.height - margin;
}

int extractPrototypeCandidates(
    const FastEventResult& raw, const FastEventConfig& config,
    const cv::Size& sourceSize,
    std::array<PrototypeCandidate, kPrototypeCandidateCapacity>* output,
    bool* overflow) {
    *overflow = false;
    if (raw.mask.empty())
        return 0;

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int componentCount = cv::connectedComponentsWithStats(
        raw.mask, labels, stats, centroids, 8, CV_32S);
    const double areaScale = config.scale * config.scale;
    const double inverseScale = 1.0 / config.scale;
    const int scaledImageArea = raw.mask.cols * raw.mask.rows;
    const int minimumAreaByFraction = static_cast<int>(
        std::lround(config.minAreaFrac * static_cast<double>(scaledImageArea)));
    const int minimumArea = (std::max)(
        1, static_cast<int>(std::ceil((config.minArea <= 0.0 ? 100.0 : config.minArea)
                                     * areaScale)));
    const int maximumArea = (std::max)(
        minimumArea,
        static_cast<int>(std::lround(config.maxAreaFrac
                                     * static_cast<double>(scaledImageArea))));
    const int scaledMargin = (std::max)(
        1, static_cast<int>(std::lround(config.margin * config.scale)));
    const int scaledMinimumBbox = (std::max)(
        1, static_cast<int>(std::lround(config.minBbox * config.scale)));

    int accepted = 0;
    for (int label = 1; label < componentCount; ++label) {
        const int area = stats.at<int>(label, cv::CC_STAT_AREA);
        if (area < minimumAreaByFraction || area > maximumArea || area < minimumArea)
            continue;
        const cv::Rect scaledBbox(
            stats.at<int>(label, cv::CC_STAT_LEFT),
            stats.at<int>(label, cv::CC_STAT_TOP),
            stats.at<int>(label, cv::CC_STAT_WIDTH),
            stats.at<int>(label, cv::CC_STAT_HEIGHT));
        if (scaledBbox.width < scaledMinimumBbox
            || scaledBbox.height < scaledMinimumBbox
            || !rectInsideFrame(scaledBbox, raw.mask.size(), scaledMargin)) {
            continue;
        }
        const cv::Rect bbox = scaledRect(scaledBbox, inverseScale);
        if (bbox.width < config.minBbox || bbox.height < config.minBbox
            || !rectInsideFrame(bbox, sourceSize, config.margin)) {
            continue;
        }
        if (accepted >= kPrototypeCandidateCapacity) {
            *overflow = true;
            continue;
        }
        PrototypeCandidate& candidate = output->at(accepted++);
        candidate.label = label;
        candidate.area = static_cast<double>(area) / areaScale;
        candidate.bbox = bbox;
        candidate.centroid = cv::Point2f(
            static_cast<float>(centroids.at<double>(label, 0) * inverseScale),
            static_cast<float>(centroids.at<double>(label, 1) * inverseScale));
    }
    return accepted;
}

class PrototypeCentroidTracker {
  public:
    explicit PrototypeCentroidTracker(const FastEventConfig& config)
        : resetFrames_((std::max)(1, config.resetFrames)) {}

    PrototypeFrameResult process(
        const FastEventResult& raw, const FastEventConfig& config,
        const cv::Size& sourceSize, const cv::Mat& sourceImage,
        QJsonArray* failures) {
        PrototypeFrameResult result;
        result.candidateCount = extractPrototypeCandidates(
            raw, config, sourceSize, &result.candidates,
            &result.candidateOverflow);
        std::array<bool, kPrototypeCandidateCapacity> candidateUsed{};
        std::array<bool, kPrototypeTrackCapacity> trackMatched{};

        // The apparatus has one-way left-to-right flow. An existing track may
        // move forward freely, but it cannot claim a candidate that reappears
        // behind it by more than half of the larger bounding-box width.
        for (int trackIndex = 0; trackIndex < kPrototypeTrackCapacity; ++trackIndex) {
            PrototypeTrack& track = tracks_[trackIndex];
            if (!track.active)
                continue;
            int bestCandidate = -1;
            double bestDistanceSquared = std::numeric_limits<double>::max();
            for (int candidateIndex = 0; candidateIndex < result.candidateCount;
                 ++candidateIndex) {
                if (candidateUsed[candidateIndex])
                    continue;
                const PrototypeCandidate& candidate = result.candidates[candidateIndex];
                const double backwardAllowance = 0.5 * static_cast<double>(
                    (std::max)(track.bbox.width, candidate.bbox.width));
                if (candidate.centroid.x + backwardAllowance < track.centroid.x)
                    continue;
                const double dx = static_cast<double>(candidate.centroid.x - track.centroid.x);
                const double dy = static_cast<double>(candidate.centroid.y - track.centroid.y);
                const double distanceSquared = dx * dx + dy * dy;
                if (bestCandidate < 0 || distanceSquared < bestDistanceSquared
                    || (distanceSquared == bestDistanceSquared
                        && candidate.label < result.candidates[bestCandidate].label)) {
                    bestCandidate = candidateIndex;
                    bestDistanceSquared = distanceSquared;
                }
            }
            if (bestCandidate >= 0) {
                const PrototypeCandidate& candidate = result.candidates[bestCandidate];
                candidateUsed[bestCandidate] = true;
                trackMatched[trackIndex] = true;
                track.missedFrames = 0;
                track.bbox = candidate.bbox;
                track.centroid = candidate.centroid;
            }
        }

        for (int trackIndex = 0; trackIndex < kPrototypeTrackCapacity; ++trackIndex) {
            PrototypeTrack& track = tracks_[trackIndex];
            if (!track.active || trackMatched[trackIndex])
                continue;
            ++track.missedFrames;
            if (track.missedFrames >= resetFrames_)
                track = PrototypeTrack{};
        }

        const double entryLimit = kPrototypeEntryZoneFraction
            * static_cast<double>(sourceSize.width);
        for (int candidateIndex = 0; candidateIndex < result.candidateCount;
             ++candidateIndex) {
            if (candidateUsed[candidateIndex])
                continue;
            const PrototypeCandidate& candidate = result.candidates[candidateIndex];
            if (candidate.centroid.x > entryLimit)
                continue;
            int freeTrack = -1;
            for (int trackIndex = 0; trackIndex < kPrototypeTrackCapacity; ++trackIndex) {
                if (!tracks_[trackIndex].active) {
                    freeTrack = trackIndex;
                    break;
                }
            }
            if (freeTrack < 0) {
                failures->append(QStringLiteral(
                    "Prototype track capacity exhausted for an entry-zone candidate."));
                continue;
            }
            PrototypeTrack& track = tracks_[freeTrack];
            track.active = true;
            track.id = nextTrackId_++;
            track.bbox = candidate.bbox;
            track.centroid = candidate.centroid;
            candidateUsed[candidateIndex] = true;

            QJsonObject entry{{QStringLiteral("track_id"), track.id},
                              {QStringLiteral("candidate_label"), candidate.label},
                              {QStringLiteral("area"), candidate.area},
                              {QStringLiteral("bbox"), rectJson(candidate.bbox)},
                              {QStringLiteral("centroid"), pointJson(candidate.centroid)}};
            desktop_app::DatasetCrop crop;
            QString cropError;
            if (desktop_app::CropService::makeDatasetCrop(
                    sourceImage, candidate.bbox, &crop, &cropError)) {
                entry.insert(QStringLiteral("crop_source_rect"), rectJson(crop.sourceRect));
            } else {
                const QString failure = QStringLiteral(
                    "Prototype crop derivation failed for track %1: %2")
                                            .arg(track.id)
                                            .arg(cropError);
                entry.insert(QStringLiteral("crop_error"), failure);
                failures->append(failure);
            }
            result.entries.append(entry);
        }

        for (const PrototypeTrack& track : tracks_) {
            if (!track.active)
                continue;
            ++result.activeTrackCount;
            result.tracks.append(
                QJsonObject{{QStringLiteral("track_id"), track.id},
                            {QStringLiteral("missed_frames"), track.missedFrames},
                            {QStringLiteral("bbox"), rectJson(track.bbox)},
                            {QStringLiteral("centroid"), pointJson(track.centroid)}});
        }
        return result;
    }

  private:
    int resetFrames_ = 2;
    int nextTrackId_ = 1;
    std::array<PrototypeTrack, kPrototypeTrackCapacity> tracks_{};
};

double elapsedMs(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

QJsonObject rectJson(const cv::Rect& rect) {
    return {
        {QStringLiteral("x"), rect.x},
        {QStringLiteral("y"), rect.y},
        {QStringLiteral("width"), rect.width},
        {QStringLiteral("height"), rect.height},
    };
}

QJsonObject pointJson(const cv::Point2f& point) {
    return {
        {QStringLiteral("x"), point.x},
        {QStringLiteral("y"), point.y},
    };
}

QJsonObject trackJson(const DropletTrackObservation& track) {
    return {{QStringLiteral("track_id"), track.trackId},
            {QStringLiteral("missed_frames"), track.missedFrames},
            {QStringLiteral("area"), track.area},
            {QStringLiteral("bbox"), rectJson(track.bbox)},
            {QStringLiteral("centroid"), pointJson(track.centroid)}};
}

QJsonObject configJson(const FastEventConfig& config) {
    return {
        {QStringLiteral("bg_frames"), config.bgFrames},
        {QStringLiteral("bg_update_frames"), config.bgUpdateFrames},
        {QStringLiteral("reset_frames"), config.resetFrames},
        {QStringLiteral("min_area"), config.minArea},
        {QStringLiteral("min_area_fraction"), config.minAreaFrac},
        {QStringLiteral("max_area_fraction"), config.maxAreaFrac},
        {QStringLiteral("min_bbox"), config.minBbox},
        {QStringLiteral("margin"), config.margin},
        {QStringLiteral("diff_threshold"), config.diffThresh},
        {QStringLiteral("blur_radius"), config.blurRadius},
        {QStringLiteral("morph_radius"), config.morphRadius},
        {QStringLiteral("use_contour_extraction"), config.useContourExtraction},
        {QStringLiteral("scale"), config.scale},
        {QStringLiteral("gap_fire_shift"), config.gapFireShift},
    };
}

QString mappedEvent(int sourceIndex) {
    switch (sourceIndex) {
    case 1576:
        return QStringLiteral("17");
    case 1853:
        return QStringLiteral("26");
    case 1977:
        return QStringLiteral("29");
    default:
        return {};
    }
}

bool parseBoundedIndex(const QCommandLineParser& parser, const QString& name, int* value,
                       QString* error) {
    bool valid = false;
    const int parsed = parser.value(name).toInt(&valid);
    if (!valid || parsed < 0) {
        *error = QStringLiteral("--%1 must be a nonnegative integer.").arg(name);
        return false;
    }
    *value = parsed;
    return true;
}

int fail(const QString& error) {
    QTextStream(stderr) << "DBG-021 replay failed: " << error << Qt::endl;
    return 2;
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Bounded DBG-021 TIFF replay through FastEventDetectorAdapter."));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("dataset-root"),
                      QStringLiteral("Dataset root containing sequence/frame_%08d.tif."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("first"), QStringLiteral("First source frame index."),
                      QStringLiteral("index"), QStringLiteral("1400")});
    parser.addOption({QStringLiteral("last"), QStringLiteral("Last source frame index."),
                      QStringLiteral("index"), QStringLiteral("2100")});
    parser.addOption({QStringLiteral("background-first"),
                      QStringLiteral("Optional first source frame used only for background establishment."),
                      QStringLiteral("index")});
    parser.addOption({QStringLiteral("background-last"),
                      QStringLiteral("Optional last source frame used only for background establishment."),
                      QStringLiteral("index")});
    parser.addOption({QStringLiteral("report"), QStringLiteral("Required JSON report path."),
                      QStringLiteral("path")});
    parser.process(application);

    if (!parser.isSet(QStringLiteral("dataset-root")) || !parser.isSet(QStringLiteral("report")))
        return fail(QStringLiteral("--dataset-root and --report are required."));

    QString parseError;
    int first = 0;
    int last = 0;
    if (!parseBoundedIndex(parser, QStringLiteral("first"), &first, &parseError)
        || !parseBoundedIndex(parser, QStringLiteral("last"), &last, &parseError))
        return fail(parseError);
    if (last < first || last - first + 1 > 701)
        return fail(QStringLiteral("The ordered replay window must contain 1 through 701 frames."));
    const bool hasBackgroundFirst = parser.isSet(QStringLiteral("background-first"));
    const bool hasBackgroundLast = parser.isSet(QStringLiteral("background-last"));
    if (hasBackgroundFirst != hasBackgroundLast) {
        return fail(QStringLiteral(
            "--background-first and --background-last must be supplied together."));
    }
    int backgroundFirst = 0;
    int backgroundLast = -1;
    if (hasBackgroundFirst) {
        if (!parseBoundedIndex(parser, QStringLiteral("background-first"),
                               &backgroundFirst, &parseError)
            || !parseBoundedIndex(parser, QStringLiteral("background-last"),
                                  &backgroundLast, &parseError)) {
            return fail(parseError);
        }
        if (backgroundLast < backgroundFirst
            || backgroundLast - backgroundFirst + 1 > 100) {
            return fail(QStringLiteral(
                "The optional background window must contain 1 through 100 frames."));
        }
        if (backgroundLast >= first) {
            return fail(QStringLiteral(
                "The optional background window must end before the replay window begins."));
        }
    }

    const QDir datasetRoot(parser.value(QStringLiteral("dataset-root")));
    if (!datasetRoot.exists())
        return fail(QStringLiteral("Dataset root does not exist: %1").arg(datasetRoot.path()));
    const QString reportPath = QFileInfo(parser.value(QStringLiteral("report"))).absoluteFilePath();
    if (QFileInfo(reportPath).absoluteDir().exists() == false)
        return fail(QStringLiteral("Report directory does not exist: %1")
                        .arg(QFileInfo(reportPath).absolutePath()));

    const FastEventConfig config{};
    FastEventDetectorAdapter detector(config);
    DropletFrameProcessor processor(detector);
    QJsonArray frames;
    TimingSamples decodeTimings;
    TimingSamples detectorTimings;
    QJsonArray failures;
    int decodedCount = 0;
    int detectedCount = 0;
    int eventEnteredCount = 0;
    int backgroundDecodedCount = 0;

    if (hasBackgroundFirst) {
        for (int sourceIndex = backgroundFirst;
             sourceIndex <= backgroundLast; ++sourceIndex) {
            const QString sourcePath = datasetRoot.filePath(
                QStringLiteral("sequence/frame_%1.tif")
                    .arg(sourceIndex, 8, 10, QLatin1Char('0')));
            const cv::Mat image = cv::imread(
                QFile::encodeName(sourcePath).toStdString(),
                cv::IMREAD_GRAYSCALE);
            if (image.empty()) {
                failures.append(QStringLiteral(
                    "Could not decode background source frame %1: %2")
                                    .arg(sourceIndex)
                                    .arg(sourcePath));
                continue;
            }
            ++backgroundDecodedCount;
            detector.addBackgroundFrame(image);
        }
        if (detector.backgroundFramesRemaining() > 0) {
            failures.append(QStringLiteral(
                "The explicit background window did not establish the production detector."));
        }
    }

    for (int sourceIndex = first; sourceIndex <= last; ++sourceIndex) {
        const QString relativePath = QStringLiteral("sequence/frame_%1.tif")
                                         .arg(sourceIndex, 8, 10, QLatin1Char('0'));
        const QString sourcePath = datasetRoot.filePath(relativePath);
        QJsonObject frame{{QStringLiteral("source_index"), sourceIndex},
                          {QStringLiteral("source_path"), relativePath}};
        const auto decodeStart = Clock::now();
        const cv::Mat image = cv::imread(QFile::encodeName(sourcePath).toStdString(),
                                         cv::IMREAD_GRAYSCALE);
        const double decodeMs = elapsedMs(decodeStart);
        decodeTimings.milliseconds.push_back(decodeMs);
        frame.insert(QStringLiteral("decode_ms"), decodeMs);
        if (image.empty()) {
            const QString failure = QStringLiteral("Could not decode source frame %1: %2")
                                        .arg(sourceIndex)
                                        .arg(sourcePath);
            frame.insert(QStringLiteral("decode_error"), failure);
            failures.append(failure);
            frames.append(frame);
            continue;
        }
        ++decodedCount;

        const auto detectorStart = Clock::now();
        const DropletFrameProcessingResult processed = processor.process(image);
        const DropletDetectionFrame& result = processed.detection;
        const double detectorMs = elapsedMs(detectorStart);
        detectorTimings.milliseconds.push_back(detectorMs);
        frame.insert(QStringLiteral("detector_ms"), detectorMs);
        frame.insert(QStringLiteral("detected"), result.detected);
        frame.insert(QStringLiteral("event_entered"), result.eventEntered);
        frame.insert(QStringLiteral("lifecycle_ended"), result.lifecycleEnded);
        frame.insert(QStringLiteral("area"), result.area);
        frame.insert(QStringLiteral("bbox"), result.detected ? QJsonValue(rectJson(result.bbox))
                                                               : QJsonValue(QJsonValue::Null));
        frame.insert(QStringLiteral("centroid"), result.detected ? QJsonValue(pointJson(result.centroid))
                                                                   : QJsonValue(QJsonValue::Null));
        frame.insert(QStringLiteral("background_frames_remaining"),
                     detector.backgroundFramesRemaining());
        const QString event = mappedEvent(sourceIndex);
        if (!event.isEmpty())
            frame.insert(QStringLiteral("mapped_event_id"), event);

        if (result.detected)
            ++detectedCount;
        eventEnteredCount += static_cast<int>(result.enteredTrackCount);
        if (processed.cropFailed) {
            const QString failure = QStringLiteral("Crop derivation failed at source frame %1: %2")
                                        .arg(sourceIndex)
                                        .arg(processed.cropError);
            frame.insert(QStringLiteral("crop_error"), failure);
            failures.append(failure);
        }
        QJsonArray crops;
        for (std::size_t index = 0; index < processed.enteredCropCount; ++index) {
            const DropletEnteredCrop& crop = processed.enteredCrops[index];
            crops.append(QJsonObject{{QStringLiteral("track_id"), crop.trackId},
                                     {QStringLiteral("source_rect"), rectJson(crop.crop.sourceRect)}});
        }
        frame.insert(QStringLiteral("entered_crops"), crops);

        QJsonArray tracks;
        for (std::size_t index = 0; index < result.visibleTrackCount; ++index)
            tracks.append(trackJson(result.visibleTracks[index]));
        QJsonArray entries;
        for (std::size_t index = 0; index < result.enteredTrackCount; ++index)
            entries.append(trackJson(result.enteredTracks[index]));
        QJsonArray endedTrackIds;
        for (std::size_t index = 0; index < result.endedTrackCount; ++index)
            endedTrackIds.append(result.endedTrackIds[index]);
        frame.insert(QStringLiteral("tracks"), tracks);
        frame.insert(QStringLiteral("entries"), entries);
        frame.insert(QStringLiteral("ended_track_ids"), endedTrackIds);
        frame.insert(QStringLiteral("capacity_exceeded"), result.capacityExceeded);
        frames.append(frame);
    }

    QJsonObject report{
        {QStringLiteral("schema"), QStringLiteral("opendss.dbg021.bounded-detector-replay.v4")},
        {QStringLiteral("input"),
         QJsonObject{{QStringLiteral("dataset_root"), datasetRoot.absolutePath()},
                     {QStringLiteral("frame_filename_pattern"),
                      QStringLiteral("sequence/frame_%08d.tif")},
                     {QStringLiteral("first_source_index"), first},
                     {QStringLiteral("last_source_index"), last},
                     {QStringLiteral("requested_frame_count"), last - first + 1},
                     {QStringLiteral("background_first_source_index"),
                      hasBackgroundFirst ? QJsonValue(backgroundFirst)
                                         : QJsonValue(QJsonValue::Null)},
                     {QStringLiteral("background_last_source_index"),
                      hasBackgroundFirst ? QJsonValue(backgroundLast)
                                         : QJsonValue(QJsonValue::Null)},
                     {QStringLiteral("background_requested_frame_count"),
                      hasBackgroundFirst
                          ? QJsonValue(backgroundLast - backgroundFirst + 1)
                          : QJsonValue(0)}}},
        {QStringLiteral("configuration_source"),
         QJsonObject{{QStringLiteral("dataset_json"),
                      QStringLiteral("capture.detection_settings is an empty object")},
                     {QStringLiteral("production_path"),
                      QStringLiteral("Desktop_app_v2/App/main.cpp constructs FastEventConfig{} and FastEventDetectorAdapter")}}},
        {QStringLiteral("fast_event_config"), configJson(config)},
        {QStringLiteral("summary"),
         QJsonObject{{QStringLiteral("decoded_frame_count"), decodedCount},
                     {QStringLiteral("background_decoded_frame_count"),
                      backgroundDecodedCount},
                     {QStringLiteral("detected_frame_count"), detectedCount},
                     {QStringLiteral("event_entered_count"), eventEnteredCount},
                     {QStringLiteral("failure_count"), failures.size()}}},
        {QStringLiteral("timing"),
         QJsonObject{{QStringLiteral("decode"), decodeTimings.json()},
                     {QStringLiteral("detector"), detectorTimings.json()}}},
        {QStringLiteral("frames"), frames},
        {QStringLiteral("failures"), failures},
        {QStringLiteral("pass"), failures.isEmpty() && decodedCount == last - first + 1},
    };

    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("Could not write report: %1").arg(reportFile.errorString()));
    const QByteArray serialized = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (reportFile.write(serialized) != serialized.size())
        return fail(QStringLiteral("Could not write the complete report: %1").arg(reportFile.errorString()));
    reportFile.close();

    QTextStream(stdout) << "DBG-021 bounded replay report: " << reportPath << Qt::endl;
    return report.value(QStringLiteral("pass")).toBool() ? 0 : 1;
}
