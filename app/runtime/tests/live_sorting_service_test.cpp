#include "../v2/live/live_sorting_service.h"
#include "../v2/run/run_manifest_v2.h"
#include "../detection/droplet_detector.h"
#include "../desktop_app/frame_types.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

using namespace desktop_app::v2;

namespace {

const char* stage = "";
std::atomic_int liveWarnings{0};

void warningHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg && message.contains(QStringLiteral("Live Sorting")))
        liveWarnings.fetch_add(1);
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

class FakeDetector final : public IDropletDetector {
public:
    QVector<DropletDetectionFrame> results;
    std::atomic_int processed{0};

    void reset() override {
        index_ = 0;
        processed.store(0);
    }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        const int index = index_++;
        DropletDetectionFrame result =
            index < results.size() ? results.at(index) : DropletDetectionFrame{};
        processed.fetch_add(1);
        return result;
    }

private:
    int index_ = 0;
};

DropletDetectionFrame detection(bool detected, bool entered, float y) {
    DropletDetectionFrame value;
    value.detected = detected;
    value.eventEntered = entered;
    value.bbox = {1, 1, 4, 4};
    value.centroid = {3.0f, y};
    return value;
}

live::LiveSortingRequest request(const QString& output) {
    live::LiveSortingRequest value;
    value.outputRoot = output;
    value.runName = QStringLiteral("Live");
    value.hitBoundary = {4.0, run::HitSide::PositiveY, 8, 8};
    value.opendssVersion = QStringLiteral("2");
    value.detectorSettings = {{QStringLiteral("fixed"), true}};
    value.cropSettings = {{QStringLiteral("size"), 64}};
    value.timingSettings = {{QStringLiteral("fixed"), true}};
    value.cameraSettings = {{QStringLiteral("live"), true}};
    value.daqSettings = {{QStringLiteral("physical"), true}};
    return value;
}

QImage image() {
    QImage value(8, 8, QImage::Format_Grayscale8);
    value.fill(127);
    return value;
}

FrameMeta meta(qint64 index, qint64 delivered = -1) {
    FrameMeta value;
    value.width = 8;
    value.height = 8;
    value.bits = 8;
    value.frameIndex = index;
    value.delivered = delivered < 0 ? index : delivered;
    return value;
}

bool waitFor(const std::function<bool()>& condition, int milliseconds = 3000) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!condition() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    return condition();
}

run::RunManifestData loadRun(const QString& output) {
    QString error;
    auto loaded = run::RunManifestV2::load(
        QDir(output).filePath(QStringLiteral("Live/run_summary.json")), &error);
    require(loaded.has_value(), qPrintable(error));
    return loaded->data();
}

live::PreparedLiveModel model(int classes, QVector<double> scores) {
    live::PreparedLiveModel value;
    value.snapshot = {"model", "Model", QString(64, QLatin1Char('a')),
                      {{"c0", "Zero"}, {"c1", "One"}}};
    if (classes == 3)
        value.snapshot.classes.push_back({"c2", "Two"});
    value.classify = [scores = std::move(scores)](const cv::Mat&, QString*) {
        return std::optional<live::LiveInferenceResult>(
            live::LiveInferenceResult{scores});
    };
    return value;
}

void offerAndWait(live::LiveSortingService& service, FakeDetector& detector,
                  qint64 index, int expectedProcessed) {
    require(service.offerFrame(image(), meta(index), 100.0), "offer frame");
    require(waitFor([&] { return detector.processed.load() >= expectedProcessed; }),
            "frame consumed");
}

void testEveryDropletPulseRouteAndStopped() {
    stage = "every droplet";
    QTemporaryDir temporary;
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(true, false, 6.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    int pulses = 0;
    live::LiveSortingService service(
        operations, detector, nullptr,
        [&](QString*) {
            ++pulses;
            return run::DaqPulseStatus::Issued;
        });
    QString error;
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    require(!service.start(request(temporary.path()), &error), "duplicate start blocked");
    require(operations.snapshot().kind == OperationKind::LiveSorting &&
                operations.snapshot().locks.testFlag(ResourceLock::Camera) &&
                operations.snapshot().locks.testFlag(ResourceLock::Daq) &&
                operations.snapshot().locks.testFlag(ResourceLock::Run) &&
                operations.snapshot().locks.testFlag(ResourceLock::Storage),
            "Live locks held");
    offerAndWait(service, detector, 1, 1);
    offerAndWait(service, detector, 2, 2);
    offerAndWait(service, detector, 3, 3);
    require(service.stop(&error), qPrintable(error));
    const auto data = loadRun(temporary.path());
    require(data.status == run::RunStatus::Stopped && data.events.size() == 1 &&
                data.events.first().decision == run::Route::Hit &&
                data.events.first().observedRoute == run::Route::Hit &&
                data.events.first().daqPulseStatus == run::DaqPulseStatus::Issued &&
                pulses == 1,
            "Every Droplet facts persisted");
    require(!operations.snapshot().kind, "locks released");
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    require(service.stop(&error), qPrintable(error));
    require(QFileInfo::exists(
                QDir(temporary.path()).filePath("Live-2/run_summary.json")),
            "service starts a new Run after terminal state");
}

void testClassBasedTwoAndThreeClass() {
    stage = "class based";
    for (int classes : {2, 3}) {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        QVector<double> scores = classes == 2 ? QVector<double>{0.1, 0.9}
                                              : QVector<double>{0.1, 0.2, 0.9};
        int pulses = 0;
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = classes == 2 ? QStringLiteral("c1")
                                        : QStringLiteral("c2");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](QString*) {
                ++pulses;
                return run::DaqPulseStatus::SuppressedNotIssued;
            },
            [prepared = model(classes, scores)](QString*) mutable {
                return std::optional<live::PreparedLiveModel>(std::move(prepared));
            });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        require(operations.snapshot().locks.testFlag(ResourceLock::Model),
                "global Model lock held");
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.events.size() == 1 &&
                    data.events.first().predictedClassId == value.hitClassId &&
                    data.events.first().scores == scores &&
                    data.events.first().decision == run::Route::Hit &&
                    data.events.first().daqPulseStatus ==
                        run::DaqPulseStatus::SuppressedNotIssued &&
                    pulses == 1,
                "class argmax and no-issued pulse fact");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 4.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        int pulses = 0;
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](QString*) {
                ++pulses;
                return run::DaqPulseStatus::Issued;
            },
            [prepared = model(2, {0.9, 0.1})](QString*) mutable {
                return std::optional<live::PreparedLiveModel>(std::move(prepared));
            });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.events.first().decision == run::Route::Waste &&
                    data.events.first().observedRoute == run::Route::Unresolved &&
                    data.events.first().daqPulseStatus ==
                        run::DaqPulseStatus::NotRequested &&
                    pulses == 0,
                "Waste decision emits no pulse and preserves Unresolved route");
    }
}

void testPauseResumeSourceGapAndDuration() {
    stage = "lifecycle";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString*) { return run::DaqPulseStatus::Issued; });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        require(service.pause(&error), qPrintable(error));
        require(!service.offerFrame(image(), meta(2), 100.0),
                "paused offer rejected without integrity loss");
        require(service.resume(&error), qPrintable(error));
        offerAndWait(service, detector, 3, 2);
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.status == run::RunStatus::Stopped &&
                    data.integrity.sourceFrameGaps.count == 0 &&
                    data.integrity.queueRejections.count == 0,
                "paused preview frames are not counted as losses");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString*) { return run::DaqPulseStatus::Issued; });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        require(service.offerFrame(image(), meta(10, 1), 100.0),
                "offer first source frame");
        require(waitFor([&] { return detector.processed.load() == 1; }),
                "first source frame consumed");
        require(service.offerFrame(image(), meta(13, 3), 100.0),
                "offer gapped source frame");
        require(waitFor([&] { return detector.processed.load() == 2; }),
                "gapped frame consumed");
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.status == run::RunStatus::Interrupted &&
                    data.integrity.sourceFrameGaps.count == 1 &&
                    data.integrity.sourceFrameGaps.ranges.first().first == 12 &&
                    data.integrity.sourceFrameGaps.ranges.first().last == 12,
                "delivery gap is keyed by source frame index");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingRequest value = request(temporary.path());
        value.requestedDurationSeconds = 0.001;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString*) { return run::DaqPulseStatus::Issued; });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        require(service.pollDuration(&error), qPrintable(error));
        require(loadRun(temporary.path()).status == run::RunStatus::Completed,
                "duration completes Run");
    }
}

void testBoundedPersistenceLossAndContinuation() {
    stage = "bounded persistence";
    QTemporaryDir temporary;
    liveWarnings.store(0);
    const auto oldHandler = qInstallMessageHandler(warningHandler);
    FakeDetector detector;
    for (int event = 0; event < 18; ++event) {
        detector.results.push_back(detection(true, true, 6.0f));
        detector.results.push_back(detection(false, false, 0.0f));
    }
    OperationCoordinator operations;
    std::mutex gateMutex;
    std::condition_variable gateReady;
    bool release = false;
    bool entered = false;
    live::LiveSortingService service(
        operations, detector, nullptr,
        [](QString*) { return run::DaqPulseStatus::Issued; }, {},
        [&](QString*) {
            std::unique_lock lock(gateMutex);
            entered = true;
            gateReady.notify_all();
            gateReady.wait(lock, [&] { return release; });
            return true;
        });
    QString error;
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    offerAndWait(service, detector, 1, 1);
    offerAndWait(service, detector, 2, 2);
    {
        std::unique_lock lock(gateMutex);
        require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                   [&] { return entered; }),
                "persistence gate entered");
    }
    for (int frame = 3; frame <= 36; ++frame)
        offerAndWait(service, detector, frame, frame);
    require(service.snapshot().persistedEvents == 0,
            "blocked persistence does not block frame consumer");
    {
        std::lock_guard lock(gateMutex);
        release = true;
    }
    gateReady.notify_all();
    require(service.stop(&error), qPrintable(error));
    const auto data = loadRun(temporary.path());
    require(data.status == run::RunStatus::Failed &&
                data.integrity.queueRejections.count == 1 &&
                data.integrity.queueRejections.ranges.first().first == 35 &&
                data.integrity.queueRejections.ranges.first().last == 35 &&
                data.events.size() == 17,
            "capacity 16 drop-newest loss persisted exactly");
    qInstallMessageHandler(oldHandler);
    require(liveWarnings.load() >= 1, "persistence loss warning captured");
}

void testPersistenceAndClassificationFailures() {
    stage = "faults";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 2.0f)};
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString* error) {
                *error = QStringLiteral("injected pulse failure");
                return run::DaqPulseStatus::Failed;
            });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        require(waitFor([&] {
                    return service.snapshot().diagnostic.contains(
                        QStringLiteral("pulse failure"));
                }),
                "pulse failure observed");
        require(service.pollDuration(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.status == run::RunStatus::Failed &&
                    data.events.size() == 1 &&
                    data.events.first().daqPulseStatus ==
                        run::DaqPulseStatus::Failed,
                "failed pulse fact is persisted before fault finalization");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 2.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        bool first = true;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString*) { return run::DaqPulseStatus::Issued; }, {},
            [&](QString* error) {
                if (!first)
                    return true;
                first = false;
                *error = QStringLiteral("injected persistence loss");
                return false;
            });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.status == run::RunStatus::Failed &&
                    data.integrity.consumerFailures.count == 1 &&
                    data.integrity.consumerFailures.ranges.first().first == 1 &&
                    data.events.isEmpty(),
                "persistence failure exact loss");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 2.0f)};
        OperationCoordinator operations;
        auto broken = model(2, {0.1, 0.9});
        broken.classify = [](const cv::Mat&, QString* error)
            -> std::optional<live::LiveInferenceResult> {
            *error = QStringLiteral("injected inference failure");
            return std::nullopt;
        };
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](QString*) { return run::DaqPulseStatus::Issued; },
            [broken = std::move(broken)](QString*) mutable {
                return std::optional<live::PreparedLiveModel>(std::move(broken));
            });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                }),
                "classification failure observed");
        require(service.pollDuration(&error), qPrintable(error));
        require(loadRun(temporary.path()).status == run::RunStatus::Failed,
                "classification failure finalizes Failed");
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testEveryDropletPulseRouteAndStopped();
    testClassBasedTwoAndThreeClass();
    testPauseResumeSourceGapAndDuration();
    testBoundedPersistenceLossAndContinuation();
    testPersistenceAndClassificationFailures();
    return 0;
}
