#include "detection/droplet_detector_adapters.h"
#include "v2/camera/camera_controller.h"
#include "v2/camera/camera_preview_image_provider.h"
#include "v2/camera/camera_service.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTextStream>
#include <QWaitCondition>

#include <opencv2/core.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace desktop_app::v2;

namespace {

using SteadyClock = std::chrono::steady_clock;

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               SteadyClock::now().time_since_epoch())
        .count();
}

struct Profile {
    QString name;
    int width = 0;
    int height = 0;
    double sourceFps = 0.0;
    bool inferredSourceFps = false;
    int frameCount = 0;
    int burstSize = 1;
};

class PacedBurstCameraDevice final : public ICameraDevice
{
public:
    explicit PacedBurstCameraDevice(Profile profile)
        : profile_(std::move(profile))
        , pixels_(profile_.width * profile_.height, '\0')
    {
    }

    ~PacedBurstCameraDevice() override
    {
        stopSource();
    }

    QString deviceId() const override
    {
        return QStringLiteral("paced-burst-characterization-camera");
    }

    bool open(QString *error) override
    {
        if (error)
            error->clear();
        return true;
    }

    bool start(QString *error) override
    {
        if (error)
            error->clear();
        stopRequested_.store(false);
        producer_ = std::thread([this] { produce(); });
        return true;
    }

    bool stop(QString *error) override
    {
        stopSource();
        if (error)
            error->clear();
        return true;
    }

    bool close(QString *error) override
    {
        stopSource();
        if (error)
            error->clear();
        return true;
    }

    CameraFrameResult drainFrames(std::vector<CameraFrame> &frames,
                                  QString *error) override
    {
        QMutexLocker locker(&frameMutex_);
        if (pendingFrames_.empty())
            frameAvailable_.wait(&frameMutex_, 1);
        if (pendingFrames_.empty()) {
            if (error)
                error->clear();
            return CameraFrameResult::NoFrame;
        }
        frames.reserve(frames.size() + pendingFrames_.size());
        while (!pendingFrames_.empty()) {
            frames.push_back(std::move(pendingFrames_.front()));
            pendingFrames_.pop_front();
        }
        lastReadId_.store(frames.back().deliveryId);
        if (error)
            error->clear();
        return CameraFrameResult::Frame;
    }

    bool waitForSource(int timeoutMs)
    {
        std::unique_lock locker(sourceMutex_);
        return sourceFinished_.wait_for(
            locker, std::chrono::milliseconds(timeoutMs),
            [this] { return sourceDone_; });
    }

    int acquiredCount() const { return acquired_.load(); }
    quint64 lastReadId() const { return lastReadId_.load(); }
    int coalescedBeforeServiceCount() const { return 0; }
    qint64 sourceFirstNs() const { return sourceFirstNs_.load(); }
    qint64 sourceLastNs() const { return sourceLastNs_.load(); }

private:
    void stopSource()
    {
        stopRequested_.store(true);
        if (producer_.joinable())
            producer_.join();
    }

    void produce()
    {
        const auto frameInterval = std::chrono::duration<double>(1.0 / profile_.sourceFps);
        const auto groupInterval = frameInterval * profile_.burstSize;
        auto deadline = SteadyClock::now();
        for (int index = 1; index <= profile_.frameCount && !stopRequested_.load(); ++index) {
            if ((index - 1) % profile_.burstSize == 0 && index > 1) {
                deadline += std::chrono::duration_cast<SteadyClock::duration>(groupInterval);
                std::this_thread::sleep_until(deadline);
            }

            CameraFrame frame;
            frame.pixelFormat = CameraPixelFormat::Mono8;
            frame.width = profile_.width;
            frame.height = profile_.height;
            frame.rowBytes = profile_.width;
            frame.bitDepth = 8;
            frame.deliveryId = static_cast<quint64>(index);
            frame.bytes = pixels_;
            frame.bytes[0] = static_cast<char>(index & 0xff);
            frame.monotonicTimestampNs = nowNs();

            if (index == 1)
                sourceFirstNs_.store(frame.monotonicTimestampNs);
            sourceLastNs_.store(frame.monotonicTimestampNs);
            acquired_.fetch_add(1);
            {
                QMutexLocker locker(&frameMutex_);
                pendingFrames_.push_back(std::move(frame));
                frameAvailable_.wakeOne();
            }
        }
        {
            std::lock_guard locker(sourceMutex_);
            sourceDone_ = true;
        }
        sourceFinished_.notify_all();
    }

    Profile profile_;
    QByteArray pixels_;
    QMutex frameMutex_;
    QWaitCondition frameAvailable_;
    std::deque<CameraFrame> pendingFrames_;
    std::thread producer_;
    std::atomic_bool stopRequested_ = false;
    std::atomic_int acquired_ = 0;
    std::atomic<quint64> lastReadId_ = 0;
    std::atomic<qint64> sourceFirstNs_ = 0;
    std::atomic<qint64> sourceLastNs_ = 0;
    std::mutex sourceMutex_;
    std::condition_variable sourceFinished_;
    bool sourceDone_ = false;
};

struct SignalMetrics {
    int servicePublished = 0;
    int detectorEntered = 0;
    int detectorCompleted = 0;
    int outOfOrder = 0;
    quint64 missingIds = 0;
    int pixelMismatches = 0;
    quint64 previousId = 0;
    qint64 serviceFirstNs = 0;
    qint64 serviceLastNs = 0;
    qint64 completionFirstNs = 0;
    qint64 completionLastNs = 0;
    int maximumBacklog = 0;
    std::vector<qint64> latenciesNs;
};

class TimedDetector final
{
public:
    TimedDetector()
        : detector_(FastEventConfig{})
    {
    }

    void process(const CameraFrame &frame, int acquiredCount)
    {
        {
            QMutexLocker locker(&metricsMutex_);
            ++metrics_.detectorEntered;
            metrics_.maximumBacklog = std::max(
                metrics_.maximumBacklog,
                std::max(0, acquiredCount - metrics_.detectorCompleted));
        }

        cv::Mat image(frame.height, frame.width, CV_8UC1,
                      const_cast<char *>(frame.bytes.constData()), frame.rowBytes);
        detector_.processFrame(image);

        const qint64 completedNs = nowNs();
        QMutexLocker locker(&metricsMutex_);
        if (metrics_.previousId != 0) {
            if (frame.deliveryId <= metrics_.previousId)
                ++metrics_.outOfOrder;
            else
                metrics_.missingIds += frame.deliveryId - metrics_.previousId - 1;
        }
        metrics_.previousId = frame.deliveryId;
        if (frame.bytes.isEmpty()
            || static_cast<uchar>(frame.bytes.at(0))
                != static_cast<uchar>(frame.deliveryId & 0xff)) {
            ++metrics_.pixelMismatches;
        }
        ++metrics_.detectorCompleted;
        if (metrics_.completionFirstNs == 0)
            metrics_.completionFirstNs = completedNs;
        metrics_.completionLastNs = completedNs;
        metrics_.latenciesNs.push_back(completedNs - frame.monotonicTimestampNs);
    }

    void servicePublished(quint64 deliveryId, int acquiredCount)
    {
        Q_UNUSED(deliveryId);
        const qint64 publishedNs = nowNs();
        QMutexLocker locker(&metricsMutex_);
        ++metrics_.servicePublished;
        if (metrics_.serviceFirstNs == 0)
            metrics_.serviceFirstNs = publishedNs;
        metrics_.serviceLastNs = publishedNs;
        metrics_.maximumBacklog = std::max(
            metrics_.maximumBacklog,
            std::max(0, acquiredCount - metrics_.servicePublished));
    }

    SignalMetrics metrics() const
    {
        QMutexLocker locker(&metricsMutex_);
        return metrics_;
    }

private:
    FastEventDetectorAdapter detector_;
    mutable QMutex metricsMutex_;
    SignalMetrics metrics_;
};

bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() >= timeoutMs)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    return true;
}

double fps(int count, qint64 firstNs, qint64 lastNs)
{
    if (count < 2 || lastNs <= firstNs)
        return 0.0;
    return static_cast<double>(count - 1) * 1'000'000'000.0
        / static_cast<double>(lastNs - firstNs);
}

double percentileMs(std::vector<qint64> values, double percentile)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(values.size()))) - 1;
    return static_cast<double>(values[std::min(index, values.size() - 1)]) / 1'000'000.0;
}

struct ProfileResult {
    Profile profile;
    int acquired = 0;
    int servicePublished = 0;
    int detectorEntered = 0;
    int detectorCompleted = 0;
    int sourceCoalesced = 0;
    int maximumBacklog = 0;
    int outOfOrder = 0;
    quint64 missingIds = 0;
    int pixelMismatches = 0;
    double sourceFps = 0.0;
    double publishedFps = 0.0;
    double completionFps = 0.0;
    double latencyP50Ms = 0.0;
    double latencyP95Ms = 0.0;
    double latencyP99Ms = 0.0;
    double latencyMaxMs = 0.0;
    QStringList failures;
};

ProfileResult runProfile(const Profile &profile)
{
    ProfileResult result;
    result.profile = profile;

    ApplicationStateStore stateStore;
    auto device = std::make_unique<PacedBurstCameraDevice>(profile);
    PacedBurstCameraDevice *fake = device.get();
    auto *service = new CameraService(std::move(device), stateStore);
    CameraPreviewImageProvider previewProvider;
    CameraController controller(*service, previewProvider);
    TimedDetector detector;
    QThread worker;
    service->moveToThread(&worker);
    QObject::connect(service, &CameraService::frameReady, &controller,
                     [&detector, fake](const CameraFrame &frame) {
                         detector.servicePublished(frame.deliveryId, fake->acquiredCount());
                     }, Qt::DirectConnection);
    QObject::connect(&controller, &CameraController::frameReady, &controller,
                     [&detector, fake](const CameraFrame &frame) {
                         detector.process(frame, fake->acquiredCount());
                     }, Qt::DirectConnection);
    worker.start();

    const bool opened = controller.open()
        && waitUntil([&] { return !controller.busy(); }, 1000)
        && service->state().status == CameraStatus::Ready;
    const bool started = opened && controller.start()
        && waitUntil([&] { return !controller.busy(); }, 1000)
        && service->state().status == CameraStatus::Streaming;
    const bool sourceFinished = started && fake->waitForSource(8000);
    const bool finalFrameRead = sourceFinished && waitUntil(
        [&] { return fake->lastReadId() >= static_cast<quint64>(fake->acquiredCount()); }, 3000);

    controller.stop();
    const bool stopped = waitUntil([&] { return !controller.busy(); }, 1000)
        && service->state().status == CameraStatus::Ready;

    const SignalMetrics metrics = detector.metrics();
    result.acquired = fake->acquiredCount();
    result.servicePublished = metrics.servicePublished;
    result.detectorEntered = metrics.detectorEntered;
    result.detectorCompleted = metrics.detectorCompleted;
    result.sourceCoalesced = fake->coalescedBeforeServiceCount();
    result.maximumBacklog = metrics.maximumBacklog;
    result.outOfOrder = metrics.outOfOrder;
    result.missingIds = metrics.missingIds;
    result.pixelMismatches = metrics.pixelMismatches;
    result.sourceFps = fps(result.acquired, fake->sourceFirstNs(), fake->sourceLastNs());
    result.publishedFps = fps(result.servicePublished,
                              metrics.serviceFirstNs, metrics.serviceLastNs);
    result.completionFps = fps(result.detectorCompleted,
                               metrics.completionFirstNs, metrics.completionLastNs);
    result.latencyP50Ms = percentileMs(metrics.latenciesNs, 0.50);
    result.latencyP95Ms = percentileMs(metrics.latenciesNs, 0.95);
    result.latencyP99Ms = percentileMs(metrics.latenciesNs, 0.99);
    result.latencyMaxMs = percentileMs(metrics.latenciesNs, 1.00);
    if (!opened)
        result.failures.append(QStringLiteral("camera open did not reach Ready"));
    if (!started)
        result.failures.append(QStringLiteral("camera start did not reach Streaming"));
    if (!sourceFinished)
        result.failures.append(QStringLiteral("source did not finish within 8 s"));
    if (!finalFrameRead)
        result.failures.append(QStringLiteral("service did not read the final source frame"));
    if (!stopped)
        result.failures.append(QStringLiteral("camera stop did not reach Ready"));
    if (result.acquired != result.detectorCompleted) {
        result.failures.append(QStringLiteral("detector completed %1 of %2 acquired frames")
                                   .arg(result.detectorCompleted)
                                   .arg(result.acquired));
    }
    if (result.servicePublished != result.detectorEntered
        || result.detectorEntered != result.detectorCompleted) {
        result.failures.append(QStringLiteral("service/controller/detector counts diverged"));
    }
    if (result.outOfOrder != 0)
        result.failures.append(QStringLiteral("detector delivery IDs were not ordered"));
    if (result.missingIds != 0) {
        result.failures.append(QStringLiteral("detector delivery IDs contained %1 gaps")
                                   .arg(result.missingIds));
    }
    if (result.pixelMismatches != 0) {
        result.failures.append(QStringLiteral("%1 frame pixel identifiers did not match delivery IDs")
                                   .arg(result.pixelMismatches));
    }
    QMetaObject::invokeMethod(service, [service] { delete service; },
                              Qt::BlockingQueuedConnection);
    worker.quit();
    worker.wait();
    return result;
}

QJsonObject jsonFor(const ProfileResult &result)
{
    QJsonArray failures;
    for (const QString &failure : result.failures)
        failures.append(failure);
    return {
        {QStringLiteral("profile"), result.profile.name},
        {QStringLiteral("width"), result.profile.width},
        {QStringLiteral("height"), result.profile.height},
        {QStringLiteral("pixel_format"), QStringLiteral("Mono8")},
        {QStringLiteral("readout_mode"), QStringLiteral("Fast")},
        {QStringLiteral("source_fps_target"), result.profile.sourceFps},
        {QStringLiteral("source_fps_inferred"), result.profile.inferredSourceFps},
        {QStringLiteral("source_fps"), result.sourceFps},
        {QStringLiteral("published_fps"), result.publishedFps},
        {QStringLiteral("completion_fps"), result.completionFps},
        {QStringLiteral("acquired"), result.acquired},
        {QStringLiteral("service_published"), result.servicePublished},
        {QStringLiteral("detector_entered"), result.detectorEntered},
        {QStringLiteral("detector_completed"), result.detectorCompleted},
        {QStringLiteral("source_coalesced_before_service"), result.sourceCoalesced},
        {QStringLiteral("persistence_dispatcher"), QStringLiteral("not_measured")},
        {QStringLiteral("maximum_backlog"), result.maximumBacklog},
        {QStringLiteral("out_of_order"), result.outOfOrder},
        {QStringLiteral("missing_ids"), static_cast<qint64>(result.missingIds)},
        {QStringLiteral("pixel_mismatches"), result.pixelMismatches},
        {QStringLiteral("latency_p50_ms"), result.latencyP50Ms},
        {QStringLiteral("latency_p95_ms"), result.latencyP95Ms},
        {QStringLiteral("latency_p99_ms"), result.latencyP99Ms},
        {QStringLiteral("latency_max_ms"), result.latencyMaxMs},
        {QStringLiteral("failures"), failures},
    };
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const std::vector<Profile> profiles = {
        // Default FastEventConfig warms up after 50 rolling frames; each source runs
        // for more than four seconds, leaving more than three seconds after warm-up.
        {QStringLiteral("2304x2304 Mono8 Fast"), 2304, 2304, 63.33, false, 270, 1},
        {QStringLiteral("1152x288 Mono8 Fast"), 1152, 288, 662.0, true, 2800, 4},
    };

    QJsonArray profileResults;
    bool passed = true;
    for (const Profile &profile : profiles) {
        const ProfileResult result = runProfile(profile);
        passed = passed && result.failures.isEmpty();
        profileResults.append(jsonFor(result));
    }

    const QJsonObject output = {
        {QStringLiteral("test"), QStringLiteral("opendss_camera_pipeline_characterization_test")},
        {QStringLiteral("pass"), passed},
        {QStringLiteral("invariant"),
         QStringLiteral("every acquired frame reaches the pre-preview detector in order")},
        {QStringLiteral("profiles"), profileResults},
    };
    QTextStream(stdout) << QJsonDocument(output).toJson(QJsonDocument::Compact) << Qt::endl;
    return passed ? 0 : 1;
}
