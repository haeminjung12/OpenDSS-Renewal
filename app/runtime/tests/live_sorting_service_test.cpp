#include "../v2/live/live_sorting_service.h"
#include "../v2/run/run_manifest_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../detection/droplet_detector.h"
#include "../desktop_app/frame_types.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
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
    std::function<void(int)> onProcess;

    void reset() override {
        index_ = 0;
        processed.store(0);
    }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        const int index = index_++;
        if (onProcess)
            onProcess(index);
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

run::DaqPulseStatus pulseStatus(bool outputEnabled) {
    return outputEnabled ? run::DaqPulseStatus::Issued
                         : run::DaqPulseStatus::SuppressedNotIssued;
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

QImage image(uchar value) {
    QImage result(8, 8, QImage::Format_Grayscale8);
    result.fill(value);
    return result;
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

QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read file bytes");
    return file.readAll();
}

QByteArray availableBytes(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

run::RunManifestData loadRun(const QString& output) {
    QString error;
    auto loaded = run::RunManifestV2::load(
        QDir(output).filePath(QStringLiteral("Live/run_summary.json")), &error);
    require(loaded.has_value(), qPrintable(error));
    return loaded->data();
}

run::RunManifestData loadPartialRun(const QString& output) {
    QString error;
    auto loaded = run::RunManifestV2::load(
        QDir(output).filePath(
            QStringLiteral("Live/run_summary.partial.json")),
        &error);
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

void testDaqOutputReadinessAndPersistence() {
    stage = "DAQ output readiness";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        auto held = operations.acquireMomentary(ResourceLock::Daq);
        require(held.acquired(), "hold DAQ lock");
        int readinessChecks = 0;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) {
                return pulseStatus(outputEnabled);
            },
            {}, {}, {},
            [&](QString*) {
                ++readinessChecks;
                return true;
            });
        QString error;
        auto value = request(temporary.path());
        require(service.start(value, &error), qPrintable(error));
        require(!operations.snapshot().locks.testFlag(ResourceLock::Daq),
                "DAQ Output OFF must not acquire the DAQ lock");
        require(service.stop(&error), qPrintable(error));
        value.daqOutputEnabled = true;
        require(!service.start(value, &error) && readinessChecks == 0,
                "DAQ Output ON must fail on the held DAQ lock before readiness");
        held.lease.release();
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        int readinessChecks = 0;
        int pulses = 0;
        bool pulseOutputEnabled = true;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                ++pulses;
                pulseOutputEnabled = outputEnabled;
                return outputEnabled
                    ? run::DaqPulseStatus::Issued
                    : run::DaqPulseStatus::SuppressedNotIssued;
            },
            {}, {}, {},
            [&](QString*) {
                ++readinessChecks;
                return false;
            });
        QString error;
        require(service.start(request(temporary.path()), &error),
                qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(waitFor([&] { return pulses == 1; }),
                "DAQ Output OFF finalization reached pulse callback");
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(readinessChecks == 0 && pulses == 1 && !pulseOutputEnabled
                    && !data.routing.physicalDaqOutputEnabled
                    && data.events.first().daqPulseStatus
                        == run::DaqPulseStatus::SuppressedNotIssued,
                "DAQ Output OFF skips readiness and persists suppressed Hit");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        int providers = 0;
        live::LiveSortingRequest value = request(temporary.path());
        value.daqOutputEnabled = true;
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool, QString*) { return run::DaqPulseStatus::Issued; },
            [&](QString*) -> std::optional<live::PreparedLiveModel> {
                ++providers;
                return model(2, {0.1, 0.9});
            },
            {}, {},
            [](QString* error) {
                *error = QStringLiteral("DAQ unavailable");
                return false;
            });
        QString error;
        require(!service.start(value, &error)
                    && error == QStringLiteral("DAQ unavailable")
                    && providers == 0
                    && !QFileInfo::exists(
                        QDir(temporary.path()).filePath(QStringLiteral("Live")))
                    && !operations.snapshot().kind,
                "DAQ Output ON readiness failure blocks before model and Run");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 2.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        bool pulseOutputEnabled = false;
        live::LiveSortingRequest value = request(temporary.path());
        value.daqOutputEnabled = true;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                pulseOutputEnabled = outputEnabled;
                return run::DaqPulseStatus::Issued;
            },
            {}, {}, {}, [](QString*) { return true; });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(pulseOutputEnabled && data.routing.physicalDaqOutputEnabled
                    && data.events.first().daqPulseStatus
                        == run::DaqPulseStatus::Issued,
                "DAQ Output ON issues and persists the physical-output fact");
    }
}

void testEveryDropletPulseRouteAndStopped() {
    stage = "every droplet";
    QTemporaryDir temporary;
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(true, false, 6.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    std::atomic_int pulses{0};
    std::atomic_bool pulseBeforeTrackEnd{false};
    detector.onProcess = [&](int index) {
        if ((index == 1 || index == 2) && pulses.load() != 0)
            pulseBeforeTrackEnd.store(true);
    };
    live::LiveSortingService service(
        operations, detector, nullptr,
        [&](bool outputEnabled, QString*) {
            pulses.fetch_add(1);
            return outputEnabled ? run::DaqPulseStatus::Issued
                                 : run::DaqPulseStatus::SuppressedNotIssued;
        });
    QString error;
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    require(!service.start(request(temporary.path()), &error), "duplicate start blocked");
    require(operations.snapshot().kind == OperationKind::LiveSorting &&
                operations.snapshot().locks.testFlag(ResourceLock::Camera) &&
                !operations.snapshot().locks.testFlag(ResourceLock::Daq) &&
                operations.snapshot().locks.testFlag(ResourceLock::Run) &&
                operations.snapshot().locks.testFlag(ResourceLock::Storage),
            "Live locks held");
    offerAndWait(service, detector, 1, 1);
    offerAndWait(service, detector, 2, 2);
    offerAndWait(service, detector, 3, 3);
    require(waitFor([&] { return pulses.load() == 1; }),
            "Hit pulse follows track-end finalization");
    require(service.stop(&error), qPrintable(error));
    const auto data = loadRun(temporary.path());
    require(data.status == run::RunStatus::Stopped && data.events.size() == 1 &&
                data.events.first().decision == run::Route::Hit &&
                data.events.first().observedRoute == run::Route::Hit &&
                data.events.first().daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued &&
                pulses.load() == 1 && !pulseBeforeTrackEnd.load(),
            "Every Droplet waits for track end and persists its facts");
    require(!operations.snapshot().kind, "locks released");
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    require(service.stop(&error), qPrintable(error));
    require(QFileInfo::exists(
                QDir(temporary.path()).filePath("Live-2/run_summary.json")),
            "service starts a new Run after terminal state");

    QTemporaryDir equalityTemporary;
    FakeDetector equalityDetector;
    equalityDetector.results = {detection(true, true, 4.0f),
                                detection(false, false, 0.0f)};
    OperationCoordinator equalityOperations;
    std::atomic_int equalityPulses{0};
    live::LiveSortingService equalityService(
        equalityOperations, equalityDetector, nullptr,
        [&](bool, QString*) {
            equalityPulses.fetch_add(1);
            return run::DaqPulseStatus::Issued;
        },
        {}, {}, {}, [](QString*) { return true; });
    auto equalityRequest = request(equalityTemporary.path());
    equalityRequest.daqOutputEnabled = true;
    require(equalityService.start(equalityRequest, &error), qPrintable(error));
    offerAndWait(equalityService, equalityDetector, 1, 1);
    offerAndWait(equalityService, equalityDetector, 2, 2);
    require(equalityService.stop(&error), qPrintable(error));
    const auto equalityData = loadRun(equalityTemporary.path());
    require(equalityPulses.load() == 0 && equalityData.events.size() == 1 &&
                equalityData.events.first().decision == run::Route::Hit &&
                equalityData.events.first().observedRoute ==
                    run::Route::Unresolved &&
                equalityData.events.first().daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued,
            "Exact final-Y equality emitted a Live Hit pulse or lost Unresolved");
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
            [&](bool, QString*) {
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
            [&](bool outputEnabled, QString*) {
                ++pulses;
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
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
            [](bool outputEnabled, QString*) {
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
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
            [](bool outputEnabled, QString*) {
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
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
        require(service.pause(&error), qPrintable(error));
        const auto partial = loadPartialRun(temporary.path());
        require(partial.integrity.sourceFrameGaps.count == 1 &&
                    partial.integrity.consumerFailures.count == 0,
                "Pause checkpoints factual source loss without manufacturing event loss");
        require(service.stop(&error), qPrintable(error));
        const auto data = loadRun(temporary.path());
        require(data.status == run::RunStatus::Interrupted &&
                    data.integrity.sourceFrameGaps.count == 1 &&
                    data.integrity.consumerFailures.count == 0 &&
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
            [](bool outputEnabled, QString*) {
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        require(service.pollDuration(&error), qPrintable(error));
        require(loadRun(temporary.path()).status == run::RunStatus::Completed,
                "duration completes Run");
    }
}

void testBacklogCancellationAtPauseAndStop() {
    stage = "backlog cancellation";
    for (bool pauseFirst : {true, false}) {
        QTemporaryDir temporary;
        FakeDetector detector;
        for (int index = 0; index < 12; ++index)
            detector.results.push_back(detection(true, true, 6.0f));
        std::mutex blockMutex;
        std::condition_variable blockReady;
        bool entered = false;
        bool release = false;
        detector.onProcess = [&](int index) {
            if (index != 0)
                return;
            std::unique_lock lock(blockMutex);
            entered = true;
            blockReady.notify_all();
            blockReady.wait(lock, [&] { return release; });
        };
        OperationCoordinator operations;
        std::atomic_int pulses{0};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                pulses.fetch_add(1);
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        require(service.offerFrame(image(), meta(1), 100.0), "offer blocked frame");
        {
            std::unique_lock lock(blockMutex);
            require(blockReady.wait_for(lock, std::chrono::seconds(2),
                                        [&] { return entered; }),
                    "detector entered");
        }
        for (int frame = 2; frame <= 8; ++frame)
            require(service.offerFrame(image(), meta(frame), 100.0),
                    "offer backlog frame");

        bool controlResult = false;
        QString controlError;
        std::thread control([&] {
            controlResult = pauseFirst ? service.pause(&controlError)
                                       : service.stop(&controlError);
        });
        qint64 probe = 9;
        require(waitFor([&] {
                    return !service.offerFrame(image(), meta(probe++), 100.0);
                }),
                "lifecycle stops accepting before quiesce");
        {
            std::lock_guard lock(blockMutex);
            release = true;
        }
        blockReady.notify_all();
        control.join();
        require(controlResult, qPrintable(controlError));
        require(detector.processed.load() == 1 && pulses.load() == 0 &&
                    service.snapshot().persistedEvents == 0,
                "queued frames cancelled before detector, pulse, and event");
        if (pauseFirst) {
            require(service.snapshot().lifecycle == OperationLifecycle::Paused,
                    "Pause published after quiesce");
            require(service.stop(&error), qPrintable(error));
        }
        require(loadRun(temporary.path()).events.isEmpty(),
                "cancelled backlog produced no persisted events");
    }
}

void testExternalCallbackQuiescence() {
    stage = "external callback quiescence";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        std::mutex detectorMutex;
        std::condition_variable detectorReady;
        bool detectorEntered = false;
        bool releaseDetector = false;
        detector.onProcess = [&](int) {
            std::unique_lock lock(detectorMutex);
            detectorEntered = true;
            detectorReady.notify_all();
            detectorReady.wait(lock, [&] { return releaseDetector; });
        };
        std::atomic_int classifications{0};
        auto prepared = model(2, {0.1, 0.9});
        prepared.classify =
            [&](const cv::Mat&, QString*)
                -> std::optional<live::LiveInferenceResult> {
            classifications.fetch_add(1);
            return live::LiveInferenceResult{{0.1, 0.9}};
        };
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) {
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            },
            [prepared](QString*) { return std::optional(prepared); });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(), meta(1), 100.0),
                "offer pre-classifier blocked frame");
        {
            std::unique_lock lock(detectorMutex);
            require(detectorReady.wait_for(lock, std::chrono::seconds(2),
                                           [&] { return detectorEntered; }),
                    "frame blocked immediately before classifier");
        }
        bool paused = false;
        QString pauseError;
        std::thread control([&] { paused = service.pause(&pauseError); });
        require(waitFor([&] {
                    return !service.offerFrame(image(), meta(2), 100.0);
                }),
                "Pause closes callback admission");
        {
            std::lock_guard lock(detectorMutex);
            releaseDetector = true;
        }
        detectorReady.notify_all();
        control.join();
        require(paused && classifications.load() == 0,
                "classifier cannot begin after Pause closes admission");
        require(service.stop(&error), qPrintable(error));
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f)};
        std::mutex callbackMutex;
        std::condition_variable callbackReady;
        bool callbackEntered = false;
        bool releaseCallback = false;
        std::atomic_int classifications{0};
        auto prepared = model(2, {0.1, 0.9});
        prepared.classify =
            [&](const cv::Mat&, QString*)
                -> std::optional<live::LiveInferenceResult> {
            classifications.fetch_add(1);
            std::unique_lock lock(callbackMutex);
            callbackEntered = true;
            callbackReady.notify_all();
            callbackReady.wait(lock, [&] { return releaseCallback; });
            return live::LiveInferenceResult{{0.1, 0.9}};
        };
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) {
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            },
            [prepared](QString*) { return std::optional(prepared); });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(), meta(1), 100.0),
                "offer classifier-blocked frame");
        {
            std::unique_lock lock(callbackMutex);
            require(callbackReady.wait_for(lock, std::chrono::seconds(2),
                                           [&] { return callbackEntered; }),
                    "classifier entered");
        }
        std::atomic_bool controlDone{false};
        bool paused = false;
        QString pauseError;
        std::thread control([&] {
            paused = service.pause(&pauseError);
            controlDone.store(true);
        });
        require(waitFor([&] {
                    return !service.offerFrame(image(), meta(2), 100.0);
                }),
                "Pause closes admission around reserved classifier");
        require(!controlDone.load(),
                "Pause waits for reserved classifier callback");
        {
            std::lock_guard lock(callbackMutex);
            releaseCallback = true;
        }
        callbackReady.notify_all();
        control.join();
        require(paused && controlDone.load() && classifications.load() == 1,
                "Pause returns after classifier quiesces exactly once");
        require(service.stop(&error), qPrintable(error));
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f)};
        std::mutex callbackMutex;
        std::condition_variable callbackReady;
        bool callbackEntered = false;
        bool releaseCallback = false;
        std::atomic_int pulses{0};
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                pulses.fetch_add(1);
                std::unique_lock lock(callbackMutex);
                callbackEntered = true;
                callbackReady.notify_all();
                callbackReady.wait(lock, [&] { return releaseCallback; });
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        require(service.offerFrame(image(), meta(1), 100.0),
                "offer pulse-blocked frame");
        require(service.offerFrame(image(), meta(2), 100.0),
                "offer pulse-finalizing frame");
        {
            std::unique_lock lock(callbackMutex);
            require(callbackReady.wait_for(lock, std::chrono::seconds(2),
                                           [&] { return callbackEntered; }),
                    "pulse callback entered");
        }
        std::atomic_bool controlDone{false};
        bool stopped = false;
        QString stopError;
        std::thread control([&] {
            stopped = service.stop(&stopError);
            controlDone.store(true);
        });
        require(waitFor([&] {
                    return !service.offerFrame(image(), meta(2), 100.0);
                }),
                "Stop closes admission around reserved pulse");
        require(!controlDone.load(), "Stop waits for reserved pulse callback");
        {
            std::lock_guard lock(callbackMutex);
            releaseCallback = true;
        }
        callbackReady.notify_all();
        control.join();
        require(stopped && controlDone.load() && pulses.load() == 1,
                "Stop returns after pulse quiesces exactly once");
        require(!service.offerFrame(image(), meta(3), 100.0),
                "no pulse callback begins after Stop acceptance");
    }
}

void testSingleFinishOwnerAndRepeatedStop() {
    stage = "single finish";
    QTemporaryDir temporary;
    FakeDetector detector;
    OperationCoordinator operations;
    live::LiveSortingRequest value = request(temporary.path());
    value.requestedDurationSeconds = 0.001;
    live::LiveSortingService service(
        operations, detector, nullptr,
        [](bool outputEnabled, QString*) {
            return outputEnabled ? run::DaqPulseStatus::Issued
                                 : run::DaqPulseStatus::SuppressedNotIssued;
        });
    QString error;
    require(service.start(value, &error), qPrintable(error));
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    std::atomic_bool go{false};
    bool stopped = false;
    bool duration = false;
    std::thread first([&] {
        while (!go.load())
            std::this_thread::yield();
        stopped = service.stop();
    });
    std::thread second([&] {
        while (!go.load())
            std::this_thread::yield();
        duration = service.pollDuration();
    });
    go.store(true);
    first.join();
    second.join();
    require(stopped && duration, "concurrent finish callers share one result");
    require(!service.stop(), "repeated Stop does not finalize twice");
    require(QFileInfo::exists(
                QDir(temporary.path()).filePath("Live/run_summary.json")),
            "single canonical Run summary");
}

void testReentrantCallbacksFailPromptly() {
    stage = "reentrant callbacks";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        live::LiveSortingService* servicePointer = nullptr;
        std::atomic_bool reentrantResult{true};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                reentrantResult.store(servicePointer->stop());
                return outputEnabled ? run::DaqPulseStatus::Issued
                                     : run::DaqPulseStatus::SuppressedNotIssued;
            });
        servicePointer = &service;
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(waitFor([&] { return !reentrantResult.load(); }),
                "pulse callback lifecycle call rejected");
        require(service.stop(&error), qPrintable(error));
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        live::LiveSortingService* servicePointer = nullptr;
        std::atomic_bool reentrantResult{true};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); }, {},
            [&](QString*) {
                reentrantResult.store(servicePointer->stop());
                return true;
            });
        servicePointer = &service;
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(waitFor([&] { return !reentrantResult.load(); }),
                "persistence callback lifecycle call rejected");
        require(service.stop(&error), qPrintable(error));
    }
}

void testInitializationExceptionsReleaseLocks() {
    stage = "initialization exceptions";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        bool throwProvider = true;
        const auto prepared = model(2, {0.1, 0.9});
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); },
            [&](QString*) -> std::optional<live::PreparedLiveModel> {
                if (throwProvider) {
                    throwProvider = false;
                    throw std::runtime_error("injected provider failure");
                }
                return prepared;
            });
        QString error;
        require(!service.start(value, &error) && !operations.snapshot().kind,
                "throwing provider releases operation lease");
        require(service.start(value, &error), qPrintable(error));
        require(service.stop(&error), qPrintable(error));
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        bool failDispatcher = true;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); }, {}, {},
            [&] {
                if (failDispatcher) {
                    failDispatcher = false;
                    return false;
                }
                return true;
            });
        QString error;
        require(!service.start(request(temporary.path()), &error) &&
                    !operations.snapshot().kind,
                "dispatcher start failure releases operation lease");
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        require(service.stop(&error), qPrintable(error));
    }
}

void testPeriodicCheckpoint() {
    stage = "periodic checkpoint";
    QTemporaryDir temporary;
    FakeDetector detector;
    detector.results = {detection(true, true, 6.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    live::LiveSortingService service(
        operations, detector, nullptr,
        [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); });
    QString error;
    require(service.start(request(temporary.path()), &error), qPrintable(error));
    const QString partial =
        QDir(temporary.path()).filePath("Live/run_summary.partial.json");
    const QByteArray initial = bytes(partial);
    offerAndWait(service, detector, 1, 1);
    offerAndWait(service, detector, 2, 2);
    require(waitFor([&] { return service.snapshot().persistedEvents == 1; }),
            "event persisted");
    require(waitFor([&] {
                const QByteArray current = availableBytes(partial);
                return !current.isEmpty() && current != initial;
            },
            2000),
            "500 ms checkpoint updates partial summary");
    require(service.stop(&error), qPrintable(error));
}

void testThrowingPersistenceAndPromptFatalOffer() {
    stage = "throwing persistence";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); }, {},
            [](QString*) -> bool {
                throw std::runtime_error("injected gate exception");
            });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                }),
                "throwing gate records exact consumer failure");
        require(service.stop(&error), qPrintable(error));
        require(loadRun(temporary.path()).status == run::RunStatus::Failed,
                "throwing gate finalizes Failed");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f),
                            detection(true, true, 6.0f)};
        OperationCoordinator operations;
        std::mutex gateMutex;
        std::condition_variable gateReady;
        bool entered = false;
        bool release = false;
        int classifications = 0;
        auto prepared = model(2, {0.1, 0.9});
        prepared.classify =
            [&](const cv::Mat&, QString* error)
                -> std::optional<live::LiveInferenceResult> {
            ++classifications;
            if (classifications == 2) {
                *error = QStringLiteral("injected second inference failure");
                return std::nullopt;
            }
            return live::LiveInferenceResult{{0.1, 0.9}};
        };
        live::LiveSortingRequest value = request(temporary.path());
        value.triggerMode = run::TriggerMode::ClassBased;
        value.useActiveModel = true;
        value.hitClassId = QStringLiteral("c1");
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); },
            [prepared](QString*) { return std::optional(prepared); },
            [&](QString*) {
                std::unique_lock lock(gateMutex);
                entered = true;
                gateReady.notify_all();
                gateReady.wait(lock, [&] { return release; });
                return true;
            });
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        {
            std::unique_lock lock(gateMutex);
            require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return entered; }),
                    "persistence blocked");
        }
        offerAndWait(service, detector, 3, 3);
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                }),
                "fatal inference failure observed");
        const auto before = std::chrono::steady_clock::now();
        require(!service.offerFrame(image(), meta(4), 100.0),
                "post-fault frame rejected");
        require(std::chrono::steady_clock::now() - before <
                    std::chrono::milliseconds(100),
                "post-fault offer does not drain blocked persistence");
        {
            std::lock_guard lock(gateMutex);
            release = true;
        }
        gateReady.notify_all();
        require(service.stop(&error), qPrintable(error));
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f),
                            detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        std::mutex gateMutex;
        std::condition_variable gateReady;
        bool persistenceEntered = false;
        bool releasePersistence = false;
        bool disappearanceEntered = false;
        bool releaseDisappearance = false;
        detector.onProcess = [&](int index) {
            if (index != 3)
                return;
            std::unique_lock lock(gateMutex);
            disappearanceEntered = true;
            gateReady.notify_all();
            gateReady.wait(lock, [&] { return releaseDisappearance; });
        };
        std::atomic_int pulses{0};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                pulses.fetch_add(1);
                return pulseStatus(outputEnabled);
            },
            {},
            [&](QString*) -> bool {
                std::unique_lock lock(gateMutex);
                persistenceEntered = true;
                gateReady.notify_all();
                gateReady.wait(lock, [&] { return releasePersistence; });
                throw std::runtime_error("injected concurrent persistence failure");
            },
            {},
            [](QString*) { return true; });
        auto value = request(temporary.path());
        value.daqOutputEnabled = true;
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        {
            std::unique_lock lock(gateMutex);
            require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return persistenceEntered; }),
                    "first event persistence blocked");
        }
        offerAndWait(service, detector, 3, 3);
        require(service.offerFrame(image(), meta(4), 100.0),
                "offer disappearance frame");
        {
            std::unique_lock lock(gateMutex);
            require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return disappearanceEntered; }),
                    "disappearance processing blocked");
            releasePersistence = true;
        }
        gateReady.notify_all();
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                }),
                "concurrent persistence failure observed");
        {
            std::lock_guard lock(gateMutex);
            releaseDisappearance = true;
        }
        gateReady.notify_all();
        require(waitFor([&] { return detector.processed.load() >= 4; }),
                "disappearance frame completed");
        require(pulses.load() == 1,
                "concurrent persistence failure suppresses later finalization pulse");
        service.stop(&error);
        require(!operations.snapshot().kind,
                "concurrent persistence failure releases operation lease");
    }
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        detector.results = {detection(true, true, 6.0f),
                            detection(false, false, 0.0f),
                            detection(true, true, 6.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        std::mutex gateMutex;
        std::condition_variable gateReady;
        bool persistenceEntered = false;
        bool releasePersistence = false;
        bool secondPulseEntered = false;
        bool releaseSecondPulse = false;
        std::atomic_int pulses{0};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [&](bool outputEnabled, QString*) {
                const int pulseNumber = pulses.fetch_add(1) + 1;
                if (pulseNumber == 2) {
                    std::unique_lock lock(gateMutex);
                    secondPulseEntered = true;
                    gateReady.notify_all();
                    gateReady.wait(lock, [&] { return releaseSecondPulse; });
                }
                return pulseStatus(outputEnabled);
            },
            {},
            [&](QString*) -> bool {
                std::unique_lock lock(gateMutex);
                persistenceEntered = true;
                gateReady.notify_all();
                gateReady.wait(lock, [&] { return releasePersistence; });
                throw std::runtime_error("injected overlapping persistence failure");
            },
            {},
            [](QString*) { return true; });
        auto value = request(temporary.path());
        value.daqOutputEnabled = true;
        QString error;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
        {
            std::unique_lock lock(gateMutex);
            require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return persistenceEntered; }),
                    "overlap persistence blocked");
        }
        offerAndWait(service, detector, 3, 3);
        require(service.offerFrame(image(), meta(4), 100.0),
                "offer overlapping disappearance frame");
        {
            std::unique_lock lock(gateMutex);
            require(gateReady.wait_for(lock, std::chrono::seconds(2),
                                       [&] { return secondPulseEntered; }),
                    "second pulse callback entered");
            releasePersistence = true;
        }
        gateReady.notify_all();
        require(!waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                },
                100),
                "persistence fault cannot linearize during pulse callback");
        {
            std::lock_guard lock(gateMutex);
            releaseSecondPulse = true;
        }
        gateReady.notify_all();
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count == 1;
                }),
                "overlapping persistence failure observed after pulse callback");
        require(pulses.load() == 2,
                "overlapping fault cannot create a post-fault pulse");
        require(!service.offerFrame(image(), meta(5), 100.0),
                "post-fault frame rejected after serialized pulse");
        service.stop(&error);
        require(!operations.snapshot().kind,
                "overlapping persistence failure releases operation lease");
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
        [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); }, {},
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
        detector.results = {detection(true, true, 2.0f),
                            detection(false, false, 0.0f)};
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool, QString* error) {
                *error = QStringLiteral("injected pulse failure");
                return run::DaqPulseStatus::Failed;
            },
            {}, {}, {}, [](QString*) { return true; });
        QString error;
        auto value = request(temporary.path());
        value.daqOutputEnabled = true;
        require(service.start(value, &error), qPrintable(error));
        offerAndWait(service, detector, 1, 1);
        offerAndWait(service, detector, 2, 2);
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
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); }, {},
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
            [](bool outputEnabled, QString*) { return pulseStatus(outputEnabled); },
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

void testFullSequenceDisabledEnabledPauseGapsAndFaults() {
    stage = "full sequence disabled";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        auto value = request(temporary.path());
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(10), meta(1), 25.0), "offer disabled frame");
        require(waitFor([&] { return detector.processed.load() == 1; }),
                "disabled frame consumed");
        require(service.stop(&error), qPrintable(error));
        const auto runData = loadRun(temporary.path());
        require(!runData.files.sequencePath.has_value(),
                "disabled recording must not publish a sequence path");
        require(!QFileInfo::exists(
                    QDir(temporary.path())
                        .filePath(QStringLiteral("Live/sequence/sequence.json"))),
                "disabled recording must not publish sequence files");
    }

    const auto verifyDisabledBacklogDiscard = [](bool pauseFirst) {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        std::mutex detectorMutex;
        std::condition_variable detectorCondition;
        bool detectorEntered = false;
        bool releaseDetector = false;
        detector.onProcess = [&](int index) {
            if (index != 0)
                return;
            std::unique_lock lock(detectorMutex);
            detectorEntered = true;
            detectorCondition.notify_all();
            detectorCondition.wait(lock, [&] { return releaseDetector; });
        };
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        require(service.start(request(temporary.path()), &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 25.0),
                "offer blocking disabled frame");
        {
            std::unique_lock lock(detectorMutex);
            require(detectorCondition.wait_for(
                        lock, std::chrono::seconds(2),
                        [&] { return detectorEntered; }),
                    "disabled detector gate entered");
        }
        require(service.offerFrame({}, meta(2), 25.0),
                "invalid disabled frame accepted before lifecycle action");

        bool actionResult = false;
        std::thread action([&] {
            QString actionError;
            actionResult = pauseFirst ? service.pause(&actionError)
                                      : service.stop(&actionError);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        {
            std::lock_guard lock(detectorMutex);
            releaseDetector = true;
            detectorCondition.notify_all();
        }
        action.join();
        require(actionResult, "disabled lifecycle action must succeed");
        require(service.snapshot().integrity.consumerFailures.count == 0 &&
                    detector.processed.load() == 1,
                "disabled Pause/Stop drain must discard queued invalid frames");
        if (pauseFirst)
            require(service.stop(&error), qPrintable(error));
    };

    stage = "disabled invalid accepted backlog pause discard";
    verifyDisabledBacklogDiscard(true);
    stage = "disabled invalid accepted backlog stop discard";
    verifyDisabledBacklogDiscard(false);

    stage = "full sequence accepted backlog pause drain";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        std::mutex detectorMutex;
        std::condition_variable detectorCondition;
        bool detectorEntered = false;
        bool releaseDetector = false;
        detector.onProcess = [&](int index) {
            if (index != 0)
                return;
            std::unique_lock lock(detectorMutex);
            detectorEntered = true;
            detectorCondition.notify_all();
            detectorCondition.wait(lock, [&] { return releaseDetector; });
        };
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 25.0),
                "offer blocking accepted frame");
        {
            std::unique_lock lock(detectorMutex);
            require(detectorCondition.wait_for(
                        lock, std::chrono::seconds(2),
                        [&] { return detectorEntered; }),
                    "blocking accepted frame entered detector");
        }
        require(service.offerFrame(image(2), meta(2), 25.0) &&
                    service.offerFrame(image(3), meta(3), 25.0),
                "offer accepted frames queued before pause");

        std::atomic_bool pauseFinished{false};
        bool pauseResult = false;
        std::thread pauser([&] {
            QString pauseError;
            pauseResult = service.pause(&pauseError);
            pauseFinished.store(true, std::memory_order_release);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(!pauseFinished.load(std::memory_order_acquire),
                "Pause must drain accepted dispatcher frames");
        {
            std::lock_guard lock(detectorMutex);
            releaseDetector = true;
            detectorCondition.notify_all();
        }
        pauser.join();
        require(pauseResult &&
                    service.snapshot().lifecycle == OperationLifecycle::Paused,
                "accepted frame drain must complete into Paused");
        require(service.stop(&error), qPrintable(error));
        auto manifest = sequence::SequenceManifestV2::load(
            QDir(temporary.path())
                .filePath(QStringLiteral("Live/sequence/sequence.json")),
            &error);
        require(manifest.has_value() &&
                    manifest->data().frameCount == 3 &&
                    manifest->data().integrity.queueRejections.count == 0 &&
                    manifest->data().integrity.consumerFailures.count == 0,
                "all frames accepted before Pause must persist in source order");
    }

    stage = "full sequence enabled pause resume gap";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(11), meta(1), 25.0), "offer sequence frame 1");
        require(waitFor([&] { return detector.processed.load() == 1; }),
                "first sequence frame consumed");
        require(service.pause(&error),
                qPrintable(QStringLiteral("enabled pause failed: %1").arg(error)));
        require(service.snapshot().lifecycle == OperationLifecycle::Paused,
                qPrintable(QStringLiteral("enabled pause finalized early: %1")
                               .arg(service.snapshot().diagnostic)));
        require(!service.offerFrame(image(99), meta(2), 25.0),
                "paused sequence must reject new frames");
        require(service.resume(&error),
                qPrintable(QStringLiteral("enabled resume failed: %1").arg(error)));
        require(service.offerFrame(image(22), meta(4), 25.0), "offer resumed frame");
        require(service.offerFrame(image(33), meta(6), 25.0), "offer gapped frame");
        require(waitFor([&] { return detector.processed.load() == 3; }),
                "resumed sequence frames consumed");
        require(service.stop(&error),
                qPrintable(QStringLiteral("enabled stop failed: %1").arg(error)));

        const auto runData = loadRun(temporary.path());
        require(runData.status == run::RunStatus::Interrupted &&
                    runData.files.sequencePath ==
                        std::optional<QString>(
                            QStringLiteral("sequence/sequence.json")),
                "source gap must interrupt the Run and retain its sequence path");
        const QString manifestPath =
            QDir(temporary.path())
                .filePath(QStringLiteral("Live/sequence/sequence.json"));
        auto manifest =
            sequence::SequenceManifestV2::load(manifestPath, &error);
        require(manifest.has_value(),
                qPrintable(QStringLiteral("enabled manifest load failed: %1")
                               .arg(error)));
        const auto& sequenceData = manifest->data();
        require(sequenceData.frameCount == 3 &&
                    sequenceData.status == QStringLiteral("interrupted") &&
                    sequenceData.integrity.sourceFrameGaps.count == 1 &&
                    sequenceData.integrity.sourceFrameGaps.ranges.size() == 1 &&
                    sequenceData.integrity.sourceFrameGaps.ranges.first().first == 5 &&
                    sequenceData.integrity.sourceFrameGaps.ranges.first().last == 5,
                "sequence manifest must preserve frame count and source gap truth");
        for (int index = 1; index <= 3; ++index) {
            const QString framePath =
                QDir(temporary.path())
                    .filePath(QStringLiteral("Live/sequence/frames/frame_%1.tif")
                                  .arg(index, 8, 10, QLatin1Char('0')));
            QImage saved(framePath);
            require(!saved.isNull(), "saved full frame must be readable");
            const int expected = index == 1 ? 11 : index == 2 ? 22 : 33;
            require(qGray(saved.pixel(0, 0)) == expected,
                    "saved full frames must retain accepted source order");
        }
    }

    stage = "full sequence write fault";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        std::atomic_int persistenceCalls{0};
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); },
            {}, [&](QString* error) {
                if (persistenceCalls.fetch_add(1) == 1) {
                    *error = QStringLiteral("injected sequence write fault");
                    return false;
                }
                return true;
            });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 20.0), "offer first fault frame");
        require(service.offerFrame(image(2), meta(2), 20.0), "offer second fault frame");
        require(waitFor([&] {
                    return service.snapshot().integrity.consumerFailures.count > 0;
                }),
                "sequence write failure must become integrity truth");
        require(service.stop(&error), "failed sequence Run must still finalize");
        const auto runData = loadRun(temporary.path());
        require(runData.status == run::RunStatus::Failed &&
                    runData.integrity.consumerFailures.count > 0,
                "sequence write loss must fail the final Run");
        auto manifest = sequence::SequenceManifestV2::load(
            QDir(temporary.path())
                .filePath(QStringLiteral("Live/sequence/sequence.json")),
            &error);
        require(manifest.has_value() &&
                    manifest->data().status == QStringLiteral("failed") &&
                    manifest->data().integrity.consumerFailures.count > 0,
                "recoverable sequence frames must receive a truthful failed manifest");
    }

    stage = "sequence publication failure aborts final Run summary";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 25.0),
                "offer sequence-publication fixture frame");
        const QString runFolder = service.snapshot().runFolder;
        const QString framePath =
            QDir(runFolder)
                .filePath(QStringLiteral("sequence/frames/frame_00000001.tif"));
        require(waitFor([&] {
                    return detector.processed.load() == 1
                        && QFileInfo::exists(framePath);
                }),
                "sequence-publication fixture frame persisted");
        const QString sequencePath =
            QDir(runFolder)
                .filePath(QStringLiteral("sequence/sequence.json"));
        require(QDir().mkpath(QFileInfo(sequencePath).absolutePath()),
                "create sequence publication fixture folder");
        QFile sequenceCollision(sequencePath);
        require(sequenceCollision.open(QIODevice::WriteOnly) &&
                    sequenceCollision.write("collision") == 9,
                "inject sequence no-replace publication failure");
        sequenceCollision.close();
        const bool stopResult = service.stop(&error);
        require(!stopResult &&
                    service.snapshot().lifecycle == OperationLifecycle::Failed,
                qPrintable(QStringLiteral(
                               "sequence publication failure must fail "
                               "finalization: result=%1 error=%2 diagnostic=%3")
                               .arg(stopResult)
                               .arg(error, service.snapshot().diagnostic)));
        require(!QFileInfo::exists(
                    QDir(runFolder)
                        .filePath(QStringLiteral("run_summary.json"))) &&
                    !QFileInfo::exists(
                        QDir(runFolder)
                            .filePath(
                                QStringLiteral("sequence/sequence.staged.json"))),
                "failed sequence publication must leave no final Run summary or stage");
    }

    stage = "late Run summary failure leaves only orphan sequence";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 25.0),
                "offer late-finalization fixture frame");
        require(waitFor([&] { return detector.processed.load() == 1; }),
                "late-finalization fixture frame consumed");
        const QString runFolder = service.snapshot().runFolder;
        require(QDir().mkpath(
                    QDir(runFolder).filePath(QStringLiteral("run_summary.json"))),
                "inject late Run manifest publication failure");
        require(!service.stop(&error) &&
                    service.snapshot().lifecycle == OperationLifecycle::Failed,
                "late Run finalization failure must fail the service");
        auto sequenceManifest = sequence::SequenceManifestV2::load(
            QDir(runFolder)
                .filePath(QStringLiteral("sequence/sequence.json")),
            &error);
        auto finalRun = run::RunManifestV2::load(
            QDir(runFolder).filePath(QStringLiteral("run_summary.json")),
            nullptr);
        require(sequenceManifest.has_value() &&
                    sequenceManifest->data().status ==
                        QStringLiteral("stopped") &&
                    !finalRun.has_value(),
                "Run summary failure may leave a readable orphan sequence but no "
                "authoritative Run reference");
    }

    stage = "full sequence queue overflow and stop drain";
    {
        QTemporaryDir temporary;
        FakeDetector detector;
        OperationCoordinator operations;
        std::mutex gateMutex;
        std::condition_variable gateCondition;
        bool gateEntered = false;
        bool releaseGate = false;
        live::LiveSortingService service(
            operations, detector, nullptr,
            [](bool enabled, QString*) { return pulseStatus(enabled); },
            {}, [&](QString*) {
                std::unique_lock lock(gateMutex);
                gateEntered = true;
                gateCondition.notify_all();
                gateCondition.wait(lock, [&] { return releaseGate; });
                return true;
            });
        QString error;
        auto value = request(temporary.path());
        value.recordFullImageSequence = true;
        require(service.start(value, &error), qPrintable(error));
        require(service.offerFrame(image(1), meta(1), 30.0),
                "offer blocked sequence frame");
        {
            std::unique_lock lock(gateMutex);
            require(gateCondition.wait_for(
                        lock, std::chrono::seconds(2),
                        [&] { return gateEntered; }),
                    "sequence persistence gate entered");
        }
        for (qint64 index = 2; index <= 80; ++index)
            service.offerFrame(image(static_cast<uchar>(index)), meta(index), 30.0);
        require(waitFor([&] {
                    return service.snapshot().integrity.queueRejections.count > 0;
                }),
                "bounded sequence persistence must report overflow truth");

        std::atomic_bool stopFinished{false};
        bool stopResult = false;
        std::thread stopper([&] {
            QString stopError;
            stopResult = service.stop(&stopError);
            stopFinished.store(true, std::memory_order_release);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        require(!stopFinished.load(std::memory_order_acquire),
                "Stop must wait for the existing persistence worker to drain");
        {
            std::lock_guard lock(gateMutex);
            releaseGate = true;
            gateCondition.notify_all();
        }
        stopper.join();
        require(stopResult, "overflowed sequence Run must finalize truthfully");
        const auto runData = loadRun(temporary.path());
        require(runData.status == run::RunStatus::Failed &&
                    runData.integrity.queueRejections.count > 0,
                "sequence queue overflow must fail the Run with retained integrity");
        auto manifest = sequence::SequenceManifestV2::load(
            QDir(temporary.path())
                .filePath(QStringLiteral("Live/sequence/sequence.json")),
            &error);
        require(manifest.has_value() &&
                    manifest->data().integrity.queueRejections.count > 0,
                "final sequence manifest must retain queue overflow truth");
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testDaqOutputReadinessAndPersistence();
    testEveryDropletPulseRouteAndStopped();
    testClassBasedTwoAndThreeClass();
    testPauseResumeSourceGapAndDuration();
    testBacklogCancellationAtPauseAndStop();
    testExternalCallbackQuiescence();
    testSingleFinishOwnerAndRepeatedStop();
    testReentrantCallbacksFailPromptly();
    testInitializationExceptionsReleaseLocks();
    testPeriodicCheckpoint();
    testThrowingPersistenceAndPromptFatalOffer();
    testBoundedPersistenceLossAndContinuation();
    testPersistenceAndClassificationFailures();
    testFullSequenceDisabledEnabledPauseGapsAndFaults();
    return 0;
}
