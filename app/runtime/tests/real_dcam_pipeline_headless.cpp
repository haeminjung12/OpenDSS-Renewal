#include "detection/droplet_detector_adapters.h"
#include "v2/camera/dcam_camera_device.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>

#include <opencv2/core.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

using namespace desktop_app::v2;

namespace {

using Clock = std::chrono::steady_clock;

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch())
        .count();
}

double percentileMs(std::vector<qint64> values, double percentile)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(values.size()))) - 1;
    return static_cast<double>(values[(std::min)(index, values.size() - 1)])
        / 1'000'000.0;
}

QJsonObject settingsJson(const CameraAppliedSettings &settings)
{
    return {
        {"width", settings.width},
        {"height", settings.height},
        {"bit_depth", settings.bitDepth},
        {"pixel_type", settings.pixelType == CameraPixelType::Mono8 ? "Mono8" : "Mono16"},
        {"exposure_ms", settings.exposureMs},
        {"readout_mode",
         settings.readoutMode == CameraReadoutMode::Fast ? "Fast" : "Slow"},
    };
}

bool sameSettings(const CameraAppliedSettings &left,
                  const CameraAppliedSettings &right)
{
    return left.width == right.width
        && left.height == right.height
        && left.bitDepth == right.bitDepth
        && left.pixelType == right.pixelType
        && std::abs(left.exposureMs - right.exposureMs) <= 0.01
        && left.readoutMode == right.readoutMode;
}

CameraAppliedSettings requestedProfile(const QString &profile,
                                       const CameraAppliedSettings &current,
                                       QString *error)
{
    CameraAppliedSettings requested = current;
    if (profile == "current")
        return requested;
    if (profile == "roi-fast") {
        requested.width = 1152;
        requested.height = 288;
    } else if (profile == "full-fast") {
        requested.width = 2304;
        requested.height = 2304;
    } else {
        if (error)
            *error = "Profile must be current, roi-fast, or full-fast.";
        return {};
    }
    requested.bitDepth = 8;
    requested.pixelType = CameraPixelType::Mono8;
    requested.exposureMs = 1.0;
    requested.readoutMode = CameraReadoutMode::Fast;
    return requested;
}

void writeResult(const QJsonObject &result)
{
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact)
                        << Qt::endl;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("opendss_real_dcam_pipeline_headless");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Real DCAM acquisition-to-detector headless characterization.");
    parser.addHelpOption();
    QCommandLineOption profileOption(
        {"p", "profile"},
        "Camera profile: current, roi-fast, or full-fast.",
        "profile", "current");
    QCommandLineOption durationOption(
        {"d", "duration"},
        "Capture duration in seconds.",
        "seconds", "5");
    parser.addOption(profileOption);
    parser.addOption(durationOption);
    parser.process(app);

    bool durationOk = false;
    const double durationSeconds = parser.value(durationOption).toDouble(&durationOk);
    const QString profile = parser.value(profileOption).trimmed().toLower();

    QJsonObject output{
        {"program", "opendss_real_dcam_pipeline_headless"},
        {"device", "DCAM:0"},
        {"source", "REAL_DCAM"},
        {"profile_requested", profile},
        {"persistence", QJsonObject{{"status", "not_measured"}}},
        {"pass", false},
    };
    QJsonArray failures;
    auto fail = [&](const QString &message) {
        failures.append(message);
    };

    if (!durationOk || !std::isfinite(durationSeconds)
        || durationSeconds <= 0.0 || durationSeconds > 300.0) {
        fail("Duration must be greater than 0 and no more than 300 seconds.");
        output.insert("failures", failures);
        writeResult(output);
        return 2;
    }

    DcamCameraDevice device;
    QString error;
    bool opened = false;
    bool started = false;
    bool settingsChanged = false;
    CameraAppliedSettings originalSettings;
    CameraAppliedSettings appliedSettings;

    if (!device.open(&error)) {
        fail(QString("DCAM open failed: %1").arg(error));
        output.insert("failures", failures);
        writeResult(output);
        return 3;
    }
    opened = true;

    if (!device.readConfiguration(originalSettings, &error)) {
        fail(QString("DCAM settings readback failed: %1").arg(error));
    } else {
        output.insert("profile_before", settingsJson(originalSettings));
        QString profileError;
        const CameraAppliedSettings requested =
            requestedProfile(profile, originalSettings, &profileError);
        if (!profileError.isEmpty()) {
            fail(profileError);
        } else if (profile == "current") {
            appliedSettings = originalSettings;
        } else {
            const CameraConfigurationResult configurationResult =
                device.applyConfiguration(requested, appliedSettings, &error);
            if (configurationResult != CameraConfigurationResult::Applied) {
                fail(QString("DCAM profile application failed: %1").arg(error));
            } else if (!sameSettings(requested, appliedSettings)) {
                fail("DCAM profile readback did not exactly match the requested profile.");
            } else {
                settingsChanged = !sameSettings(originalSettings, appliedSettings);
            }
        }
    }

    output.insert("profile_applied", settingsJson(appliedSettings));
    if (appliedSettings.bitDepth != 8
        || appliedSettings.pixelType != CameraPixelType::Mono8) {
        fail("The qualified event detector headless path requires a Mono8 camera profile.");
    }

    FastEventConfig detectorConfig{};
    FastEventDetectorAdapter detector(detectorConfig);
    detector.reset();

    quint64 firstId = 0;
    quint64 lastId = 0;
    quint64 submitted = 0;
    quint64 completed = 0;
    quint64 gaps = 0;
    quint64 duplicates = 0;
    quint64 outOfOrder = 0;
    quint64 detectedFrames = 0;
    quint64 eventEntries = 0;
    std::size_t maximumBatch = 0;
    std::size_t maximumQueueDepth = 0;
    qint64 firstFrameTimestampNs = 0;
    qint64 lastFrameTimestampNs = 0;
    qint64 firstDetectorStartNs = 0;
    qint64 lastDetectorCompleteNs = 0;
    qint64 totalDetectorLatencyNs = 0;
    std::vector<qint64> detectorLatenciesNs;

    auto processFrame = [&](CameraFrame &frame) {
        if (frame.pixelFormat != CameraPixelFormat::Mono8
            || frame.bitDepth != 8) {
            fail("DCAM returned a non-Mono8 frame.");
            return false;
        }
        if (submitted == 0) {
            firstId = frame.deliveryId;
            firstFrameTimestampNs = frame.monotonicTimestampNs;
        } else if (frame.deliveryId == lastId) {
            ++duplicates;
            fail(QString("Duplicate DCAM delivery ID %1.").arg(frame.deliveryId));
            return false;
        } else if (frame.deliveryId < lastId) {
            ++outOfOrder;
            fail(QString("Out-of-order DCAM delivery ID %1 after %2.")
                     .arg(frame.deliveryId)
                     .arg(lastId));
            return false;
        } else if (frame.deliveryId > lastId + 1) {
            gaps += frame.deliveryId - lastId - 1;
            fail(QString("DCAM delivery gap %1-%2.")
                     .arg(lastId + 1)
                     .arg(frame.deliveryId - 1));
            return false;
        }

        lastId = frame.deliveryId;
        lastFrameTimestampNs = frame.monotonicTimestampNs;
        ++submitted;
        const qint64 detectorStartNs = nowNs();
        if (firstDetectorStartNs == 0)
            firstDetectorStartNs = detectorStartNs;
        cv::Mat image(frame.height, frame.width, CV_8UC1,
                      frame.bytes.data(), frame.rowBytes);
        const DropletDetectionFrame detection = detector.processFrame(image);
        const qint64 detectorCompleteNs = nowNs();
        lastDetectorCompleteNs = detectorCompleteNs;
        const qint64 detectorLatencyNs = detectorCompleteNs - detectorStartNs;
        totalDetectorLatencyNs += detectorLatencyNs;
        detectorLatenciesNs.push_back(detectorLatencyNs);
        ++completed;
        if (detection.detected)
            ++detectedFrames;
        if (detection.eventEntered)
            ++eventEntries;
        return true;
    };

    if (failures.isEmpty()) {
        if (!device.start(&error)) {
            fail(QString("DCAM start failed: %1").arg(error));
        } else {
            started = true;
            const qint64 captureStartNs = nowNs();
            const qint64 captureDeadlineNs =
                captureStartNs + static_cast<qint64>(durationSeconds * 1'000'000'000.0);
            std::mutex queueMutex;
            std::condition_variable queueReady;
            std::deque<CameraFrame> queuedFrames;
            bool acquisitionDone = false;
            QString acquisitionFailure;

            std::thread acquisitionThread([&] {
                while (nowNs() < captureDeadlineNs) {
                    std::vector<CameraFrame> frames;
                    QString drainError;
                    const CameraFrameResult result =
                        device.drainFrames(frames, &drainError);
                    if (result == CameraFrameResult::Error) {
                        std::lock_guard lock(queueMutex);
                        acquisitionFailure =
                            QString("DCAM drain failed: %1").arg(drainError);
                        break;
                    }
                    if (result == CameraFrameResult::NoFrame) {
                        QThread::yieldCurrentThread();
                        continue;
                    }

                    std::lock_guard lock(queueMutex);
                    maximumBatch = (std::max)(maximumBatch, frames.size());
                    for (CameraFrame &frame : frames)
                        queuedFrames.push_back(std::move(frame));
                    maximumQueueDepth =
                        (std::max)(maximumQueueDepth, queuedFrames.size());
                    queueReady.notify_one();
                }
                {
                    std::lock_guard lock(queueMutex);
                    acquisitionDone = true;
                }
                queueReady.notify_one();
            });

            bool captureStopped = false;
            bool processing = true;
            while (processing) {
                std::deque<CameraFrame> localFrames;
                bool done = false;
                QString producerError;
                {
                    std::unique_lock lock(queueMutex);
                    queueReady.wait(lock, [&] {
                        return acquisitionDone || !queuedFrames.empty();
                    });
                    localFrames.swap(queuedFrames);
                    done = acquisitionDone;
                    producerError = acquisitionFailure;
                }

                if (done && !captureStopped) {
                    QString stopError;
                    if (!device.stop(&stopError))
                        fail(QString("DCAM stop failed: %1").arg(stopError));
                    captureStopped = true;
                    started = false;
                }
                if (!producerError.isEmpty())
                    fail(producerError);

                for (CameraFrame &frame : localFrames) {
                    if (!processFrame(frame)) {
                        processing = false;
                        break;
                    }
                }
                if (done)
                    processing = false;
            }
            acquisitionThread.join();
        }
    }

    if (started) {
        QString stopError;
        if (!device.stop(&stopError))
            fail(QString("DCAM stop failed: %1").arg(stopError));
        started = false;
    }

    if (settingsChanged) {
        CameraAppliedSettings restored;
        const CameraConfigurationResult restoreResult =
            device.applyConfiguration(originalSettings, restored, &error);
        if (restoreResult != CameraConfigurationResult::Applied
            || !sameSettings(originalSettings, restored)) {
            fail(QString("DCAM settings restoration failed: %1").arg(error));
        }
    }
    if (opened) {
        QString closeError;
        if (!device.close(&closeError))
            fail(QString("DCAM close failed: %1").arg(closeError));
    }

    const quint64 deliverySpan =
        submitted ? (lastId - firstId + 1) : 0;
    const double sourceElapsedSeconds =
        lastFrameTimestampNs > firstFrameTimestampNs
        ? static_cast<double>(lastFrameTimestampNs - firstFrameTimestampNs)
              / 1'000'000'000.0
        : 0.0;
    const double detectorElapsedSeconds =
        lastDetectorCompleteNs > firstDetectorStartNs
        ? static_cast<double>(lastDetectorCompleteNs - firstDetectorStartNs)
              / 1'000'000'000.0
        : 0.0;
    const double averageDetectorLatencyMs =
        completed > 0
        ? static_cast<double>(totalDetectorLatencyNs)
              / static_cast<double>(completed) / 1'000'000.0
        : 0.0;
    const double detectorServiceFps =
        totalDetectorLatencyNs > 0
        ? static_cast<double>(completed) * 1'000'000'000.0
              / static_cast<double>(totalDetectorLatencyNs)
        : 0.0;

    if (submitted == 0)
        fail("DCAM produced zero frames.");
    if (submitted != completed)
        fail("Detector submitted/completed counts differ.");
    if (deliverySpan != submitted)
        fail("DCAM delivery span differs from drained frame count.");

    output.insert("first_delivery_id", static_cast<qint64>(firstId));
    output.insert("last_delivery_id", static_cast<qint64>(lastId));
    output.insert("dcam_delivery_span", static_cast<qint64>(deliverySpan));
    output.insert("drained_frames", static_cast<qint64>(submitted));
    output.insert("detector_submitted", static_cast<qint64>(submitted));
    output.insert("detector_completed", static_cast<qint64>(completed));
    output.insert("delivery_gaps", static_cast<qint64>(gaps));
    output.insert("duplicates", static_cast<qint64>(duplicates));
    output.insert("out_of_order", static_cast<qint64>(outOfOrder));
    output.insert("maximum_drain_batch", static_cast<qint64>(maximumBatch));
    output.insert("maximum_detector_queue_depth",
                  static_cast<qint64>(maximumQueueDepth));
    output.insert("detected_frames", static_cast<qint64>(detectedFrames));
    output.insert("event_entries", static_cast<qint64>(eventEntries));
    output.insert("source_fps",
                  sourceElapsedSeconds > 0.0
                      ? static_cast<double>(deliverySpan - 1) / sourceElapsedSeconds
                      : 0.0);
    output.insert("detector_completion_fps",
                  detectorElapsedSeconds > 0.0
                      ? static_cast<double>(completed) / detectorElapsedSeconds
                      : 0.0);
    output.insert("detector_average_latency_ms", averageDetectorLatencyMs);
    output.insert("detector_service_fps", detectorServiceFps);
    output.insert("coax_minimum_fps", 89.1);
    output.insert("coax_capacity_math_pass", detectorServiceFps >= 89.1);
    output.insert("headroom_target_fps", 100.0);
    output.insert("headroom_100fps_math_pass", detectorServiceFps >= 100.0);
    output.insert("detector_latency_p50_ms",
                  percentileMs(detectorLatenciesNs, 0.50));
    output.insert("detector_latency_p95_ms",
                  percentileMs(detectorLatenciesNs, 0.95));
    output.insert("detector_latency_p99_ms",
                  percentileMs(detectorLatenciesNs, 0.99));
    output.insert("detector_latency_max_ms",
                  percentileMs(detectorLatenciesNs, 1.00));
    output.insert("dcam_overrun",
                  std::any_of(failures.begin(), failures.end(), [](const QJsonValue &value) {
                      return value.toString().contains("overrun", Qt::CaseInsensitive);
                  }));
    output.insert("failures", failures);
    output.insert("pass", failures.isEmpty());
    writeResult(output);
    return failures.isEmpty() ? 0 : 1;
}
