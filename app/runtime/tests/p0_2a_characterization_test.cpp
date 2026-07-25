#include "../daq_trigger.h"
#include "../dataset_capture_session.h"
#include "../desktop_app/background_task_registry.h"
#include "../desktop_app/json_persistence.h"
#include "../desktop_app/live_data_collection_writer.h"
#include "../desktop_app/live_frame_dispatcher.h"
#include "../desktop_app/live_log_writer.h"
#include "../desktop_app/sequence_summary_writer.h"

#include <NIDAQmx.h>

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QTemporaryDir>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct FakeDaq {
    std::vector<std::string> calls;
    std::vector<double> timingRates;
    std::vector<double> waitTimeouts;
    std::vector<double> waveform;
    std::vector<int> writeSampleCounts;
    std::vector<bool32> writeAutoStarts;
    std::vector<double> writeTimeouts;
    std::vector<bool32> writeDataLayouts;
    int timingFailures = 0;
    int waitStatus = 0;
    double deviceMaxRate = 0.0;

    void reset() { *this = FakeDaq{}; }
};

FakeDaq fakeDaq;

QString readText(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QJsonObject readObject(const QString& path) {
    return QJsonDocument::fromJson(readText(path).toUtf8()).object();
}

bool waitForTaskEntry(const std::atomic_bool& entered) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!entered.load()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::yield();
    }
    return true;
}

void characterizeDaqTrigger() {
    fakeDaq.reset();
    DaqConfig config;
    config.channel = "Dev9/ao1";
    config.frequencyHz = 100.0;
    config.durationMs = 5.0;
    config.delayMs = 25.0;

    std::string error;
    {
        DaqTrigger trigger;
        expect(trigger.init(config, error), "DAQ initialization succeeds through the fake NI boundary");
        expect(error.empty(), "successful DAQ initialization clears diagnostics");
        expect(trigger.sampleRateHz() == 10000.0, "DAQ uses the qualified 10 kHz minimum sample rate");
        expect(trigger.finiteSampleCount() == 51, "DAQ appends one final zero sample after 50 generated samples");
        expect(trigger.finalSampleValue() == 0.0, "DAQ waveform retains a final zero sample");
        expect(fakeDaq.calls == std::vector<std::string>{"create", "channel", "max_rate", "timing", "buffer"},
               "DAQ initialization preserves NI creation, channel, rate, timing, and buffer ordering");
        expect(fakeDaq.timingRates == std::vector<double>{10000.0}, "DAQ configures the expected first timing rate");

        expect(trigger.fire(error), "DAQ fire succeeds through the fake NI boundary");
        expect(fakeDaq.calls == std::vector<std::string>{"create", "channel", "max_rate", "timing", "buffer", "stop",
                                                          "write", "wait", "stop"},
               "DAQ fire preserves stop, write, wait, final-stop ordering");
        expect(fakeDaq.waveform.size() == 51 && fakeDaq.waveform.back() == 0.0,
               "DAQ writes the complete waveform including the final zero");
        expect(fakeDaq.writeSampleCounts == std::vector<int>{51} && fakeDaq.writeAutoStarts == std::vector<bool32>{1} &&
                   fakeDaq.writeTimeouts == std::vector<double>{10.0} &&
                   fakeDaq.writeDataLayouts == std::vector<bool32>{DAQmx_Val_GroupByChannel},
               "DAQ write preserves sample count, auto-start, timeout, and group-by-channel arguments");
        expect(fakeDaq.waitTimeouts == std::vector<double>{5.03}, "DAQ wait timeout includes duration and delay");
    }

    fakeDaq.reset();
    fakeDaq.timingFailures = 1;
    {
        DaqTrigger fallback;
        expect(fallback.init(config, error), "DAQ retries timing with a lower sample rate");
        expect(fallback.sampleRateHz() == 5000.0 && fallback.finiteSampleCount() == 26,
               "DAQ fallback halves the sample rate and recomputes the finite sample count");
        expect(fakeDaq.timingRates == std::vector<double>{10000.0, 5000.0}, "DAQ attempts the qualified rate fallback order");
        expect(fallback.fire(error), "DAQ fires after timing fallback");
        expect(fakeDaq.writeSampleCounts == std::vector<int>{26} && fakeDaq.waveform.size() == 26 &&
                   fakeDaq.waveform.back() == 0.0,
               "DAQ fallback writes its recomputed sample count and final zero");
    }

    fakeDaq.reset();
    {
        DaqTrigger waitFailure;
        expect(waitFailure.init(config, error), "DAQ initializes before wait failure characterization");
        fakeDaq.waitStatus = -1;
        expect(!waitFailure.fire(error), "DAQ reports a wait failure");
        expect(fakeDaq.calls.back() == "stop", "DAQ stops the task after a wait failure");
        waitFailure.shutdown();
        expect(fakeDaq.calls.size() >= 2 && fakeDaq.calls[fakeDaq.calls.size() - 2] == "stop" &&
                   fakeDaq.calls.back() == "clear",
               "DAQ shutdown stops then clears the NI task after failure");
    }

    fakeDaq.reset();
    {
        DaqTrigger destructorShutdown;
        expect(destructorShutdown.init(config, error), "DAQ initializes before destructor shutdown characterization");
    }
    expect(fakeDaq.calls == std::vector<std::string>{"create", "channel", "max_rate", "timing", "buffer", "stop", "clear"},
           "DAQ destructor preserves production stop-then-clear shutdown order");
}

void characterizePersistence() {
    QTemporaryDir temp;
    expect(temp.isValid(), "temporary persistence directory is available");
    const QString root = temp.path();

    QString jsonError;
    const QString jsonPath = QDir(root).filePath("nested/state.json");
    expect(desktop_app::writeJsonObjectAtomically(jsonPath, QJsonObject{{"count", 2}, {"state", "ready"}}, &jsonError),
           "atomic JSON writer finalizes a JSON object");
    const QJsonObject state = readObject(jsonPath);
    expect(state.value("count").toInt() == 2 && state.value("state").toString() == "ready",
           "atomic JSON preserves stable keys and values");

    LiveDataCollectionWriter collection;
    std::string error;
    expect(collection.start(root, error), "collection writer starts in a temporary directory");
    PipelineEvent event;
    event.detected = true;
    event.fired = true;
    event.area = 12.5;
    event.bbox = cv::Rect(1, 2, 3, 4);
    event.centroid = cv::Point2f(2.5f, 4.0f);
    event.cropPath = "crops/raw.png";
    event.frameNumber = 42;
    QImage image(2, 1, QImage::Format_Grayscale8);
    image.fill(7);
    const bool frameWritten = collection.writeFrame(image, event, true, 42, error);
    expect(frameWritten, "collection writer saves one frame and detection row: " + error);
    const QImage decodedFrame(QDir(collection.sessionDir()).filePath("stream/frame_000001.tiff"));
    expect(!decodedFrame.isNull() && decodedFrame.size() == image.size() && decodedFrame.pixelColor(0, 0).red() == 7,
           "collection TIFF retains its stable filename and decoded pixel content");
    LiveDataCollectionWriter::Integrity integrity;
    integrity.handoffAccepted = 4;
    integrity.sourceGapCount = 2;
    integrity.queueRejectedCount = 2;
    integrity.consumerFailureCount = 1;
    integrity.sourceGaps.push_back({101, 102});
    integrity.queueRejected.push_back({18, 19});
    integrity.consumerFailures.push_back({20, 20});
    collection.setIntegrity(integrity);
    expect(collection.finish("stopped", error), "collection writer finalizes its metadata");
    expect(collection.framesSaved() == 1 && collection.rowsLogged() == 1, "collection writer final counters match row order");
    const QString csv = readText(QDir(collection.sessionDir()).filePath("detections.csv"));
    expect(csv.startsWith("image,event_detected,crop_id,timestamp_utc,frame_number,x,y,width,height,centroid_x,centroid_y,area,"),
           "collection CSV retains its stable header order");
    expect(csv.contains("\"stream/frame_000001.tiff\",1,,") && csv.contains(",42,1,2,3,4,2.500,4.000,12.5,"),
           "collection CSV retains frame ordering and event values");
    const QJsonObject metadata = readObject(QDir(collection.sessionDir()).filePath("collection_metadata.json"));
    expect(metadata.value("mode").toString() == "live_data_collection" &&
               metadata.value("frames_saved").toString() == "1" && metadata.value("stop_reason").toString() == "stopped",
           "collection metadata retains stable finalized keys and values");
    const QJsonObject persistedIntegrity = metadata.value("integrity").toObject();
    expect(persistedIntegrity.value("handoff_accepted_count").toString() == "4" &&
               persistedIntegrity.value("source_gap_count").toString() == "2" &&
               persistedIntegrity.value("queue_rejection_count").toString() == "2" &&
               persistedIntegrity.value("consumer_failure_count").toString() == "1",
           "collection metadata adds exact handoff integrity counts without changing existing key types");
    const auto hasSingleRange = [](const QJsonValue& value, const QString& first, const QString& last) {
        const QJsonArray ranges = value.toArray();
        if (ranges.size() != 1)
            return false;
        const QJsonArray range = ranges.at(0).toArray();
        return range.size() == 2 && range.at(0).toString() == first && range.at(1).toString() == last;
    };
    expect(hasSingleRange(persistedIntegrity.value("source_gaps"), "101", "102") &&
               hasSingleRange(persistedIntegrity.value("queue_rejections"), "18", "19") &&
               hasSingleRange(persistedIntegrity.value("consumer_failures"), "20", "20"),
           "collection metadata preserves exact coalesced integrity ranges");

    LiveLogRecord row;
    row.wallTime = "2026-07-23T12:00:00.000Z";
    row.frameIndex = 4;
    row.delivered = 4;
    row.fps = 12.5;
    row.area = 7.25;
    row.processed = true;
    row.detected = true;
    row.label = "Target";
    const QString logPath = writeLiveLogCsv(root, "run", {row});
    const QString log = readText(logPath);
    expect(logPath.endsWith("run_live_log.csv") && log.startsWith("wall_time,frame_index,delivered,dropped,fps,cam_fps,proc_ms,"),
           "live log retains its filename and CSV header order");
    expect(log.contains("\"2026-07-23T12:00:00.000Z\",4,4,0,12.50,0.00,0.000,1,") && log.contains("\"Target\""),
           "live log retains row values in order");
    expect(log.contains(",\"\",1,0,7.3,0,0,0,0,"),
           "live log retains the production one-decimal area column precision");

    SequenceEventRecord sequenceEvent;
    sequenceEvent.eventId = 7;
    sequenceEvent.label = "Target";
    sequenceEvent.startFrame = 10;
    sequenceEvent.decisionFrame = 11;
    sequenceEvent.decisionDir = "Hit";
    sequenceEvent.firedFrame = 10;
    sequenceEvent.framesTracked = 2;
    sequenceEvent.startY = 1.0;
    sequenceEvent.endY = 3.0;
    sequenceEvent.cumulativeDy = 2.0;
    sequenceEvent.pathLength = 2.0;
    SequenceSummaryMetadata summary;
    summary.targetLabel = "Target";
    summary.totalFrames = 12;
    summary.fps = 20.0;
    const QString summaryPath = writeSequenceSummaryCsv(root, "summary.csv", {sequenceEvent}, summary);
    const QString summaryText = readText(summaryPath);
    expect(summaryText.startsWith("metric,value\nsummary_schema,sequence_summary.classified_and_went_to.v3\n"),
           "sequence summary retains its stable schema header");
    expect(summaryText.contains("events_detected,1\n") && summaryText.contains("target_motion_hit_count,1\n") &&
               summaryText.contains("class,\"Target\",1\n"),
           "sequence summary retains finalized counts and class ordering");
}

void characterizeBackgroundTasks() {
    std::atomic_bool entered{false};
    std::atomic_bool stopped{false};
    {
        BackgroundTaskRegistry tasks;
        tasks.launch("cooperative", [&](const BackgroundTaskRegistry::StopFlag& stop) {
            entered.store(true);
            while (!stop->load())
                std::this_thread::yield();
            stopped.store(true);
        });
        expect(waitForTaskEntry(entered), "BackgroundTaskRegistry cooperative task enters before stop");
        tasks.requestStop();
        tasks.waitAll();
        expect(stopped.load(), "BackgroundTaskRegistry waitAll joins a cooperatively stopped task");
    }

    entered.store(false);
    stopped.store(false);
    {
        BackgroundTaskRegistry tasks;
        tasks.launch("destructor", [&](const BackgroundTaskRegistry::StopFlag& stop) {
            entered.store(true);
            while (!stop->load())
                std::this_thread::yield();
            stopped.store(true);
        });
        expect(waitForTaskEntry(entered), "BackgroundTaskRegistry destructor task enters before scope exit");
    }
    expect(stopped.load(), "BackgroundTaskRegistry destructor requests stop and joins its task");
}

void characterizeLiveFrameDispatcher() {
    auto makeCollectionMembership = [](bool collection) {
        LiveFrameDispatcher::Membership membership;
        membership.collection = collection;
        return membership;
    };
    {
        std::vector<std::uint64_t> ids;
        std::vector<int> pixels;
        LiveFrameDispatcher dispatcher(
            [&](const QImage& image, const FrameMeta&, double, std::uint64_t id,
                LiveFrameDispatcher::Membership) {
                ids.push_back(id);
                pixels.push_back(image.pixelColor(0, 0).red());
            });
        FrameMeta meta;
        meta.delivered = 1;
        QImage image(1, 1, QImage::Format_Grayscale8);
        image.fill(3);
        expect(dispatcher.offer(image, meta, 1.0, makeCollectionMembership(true)).accepted,
               "dispatcher accepts an incoming frame");
        image.fill(9);
        dispatcher.waitThrough(1);
        expect(ids == std::vector<std::uint64_t>{1} && pixels == std::vector<int>{3},
               "dispatcher deep-owns the image and preserves FIFO delivery");
    }

    {
        std::mutex gateMutex;
        std::condition_variable gateChanged;
        bool consumerEntered = false;
        bool releaseConsumer = false;
        std::vector<std::uint64_t> consumedIds;
        LiveFrameDispatcher full([&](const QImage&, const FrameMeta&, double, std::uint64_t id,
                                     LiveFrameDispatcher::Membership) {
            {
                std::unique_lock<std::mutex> gateLock(gateMutex);
                if (!consumerEntered) {
                    consumerEntered = true;
                    gateChanged.notify_all();
                    gateChanged.wait(gateLock, [&] { return releaseConsumer; });
                }
            }
            consumedIds.push_back(id);
        });

        FrameMeta meta;
        QImage image(1, 1, QImage::Format_Grayscale8);
        image.fill(5);
        meta.delivered = 1;
        expect(full.offer(image, meta, 1.0, makeCollectionMembership(true)).accepted,
               "blocked dispatcher accepts the active item");
        {
            std::unique_lock<std::mutex> gateLock(gateMutex);
            expect(gateChanged.wait_for(gateLock, std::chrono::seconds(2), [&] { return consumerEntered; }),
                   "dispatcher consumer enters the deterministic blocking gate");
        }

        for (std::size_t i = 0; i < LiveFrameDispatcher::capacity(); ++i) {
            meta.delivered++;
            expect(full.offer(image, meta, 1.0, makeCollectionMembership(true)).accepted,
                   "dispatcher fills each fixed FIFO slot");
        }
        meta.delivered++;
        const auto offerStart = std::chrono::steady_clock::now();
        const auto rejectedFirst = full.offer(image, meta, 1.0, makeCollectionMembership(true));
        meta.delivered++;
        const auto rejectedSecond = full.offer(image, meta, 1.0, makeCollectionMembership(true));
        const auto offerElapsed = std::chrono::steady_clock::now() - offerStart;
        expect(!rejectedFirst.accepted && !rejectedSecond.accepted &&
                   offerElapsed < std::chrono::milliseconds(250),
               "full-queue offers reject promptly without waiting for the blocked consumer");

        const std::uint64_t checkpoint = full.closeCollectionBoundary();
        expect(checkpoint == 17, "collection checkpoint names the last accepted ID, not trailing rejected IDs");
        {
            std::lock_guard<std::mutex> gateLock(gateMutex);
            releaseConsumer = true;
        }
        gateChanged.notify_all();
        full.waitThrough(checkpoint);
        full.stopAndDrain();

        std::vector<std::uint64_t> expectedIds;
        for (std::uint64_t id = 1; id <= 17; ++id)
            expectedIds.push_back(id);
        expect(consumedIds == expectedIds, "accepted frames remain in exact FIFO order through drain");
        const auto integrity = full.integrity();
        expect(integrity.handoffAccepted == 17 && integrity.queueRejectedCount == 2 &&
                   integrity.queueRejected.size() == 1 && integrity.queueRejected.front().first == 18 &&
                   integrity.queueRejected.front().last == 19,
               "drop-newest overflow records the exact rejected handoff range");
        expect(integrity.sourceGapCount == 0 && integrity.sourceGaps.empty(),
               "queue rejections are not double-counted as camera source gaps");
    }

    {
        std::mutex gateMutex;
        std::condition_variable gateChanged;
        bool consumerEntered = false;
        bool releaseConsumer = false;
        LiveFrameDispatcher datasetOnly(
            [&](const QImage&, const FrameMeta&, double, std::uint64_t,
                LiveFrameDispatcher::Membership) {
                std::unique_lock<std::mutex> gateLock(gateMutex);
                consumerEntered = true;
                gateChanged.notify_all();
                gateChanged.wait(gateLock, [&] { return releaseConsumer; });
                throw std::runtime_error("injected dataset consumer failure");
            });
        datasetOnly.openDatasetBoundary();
        LiveFrameDispatcher::Membership membership;
        membership.datasetCapture = true;
        QImage image(1, 1, QImage::Format_Grayscale8);
        FrameMeta meta;
        meta.delivered = 100;
        expect(datasetOnly.offer(image, meta, 1.0, membership).accepted,
               "dataset-only integrity test accepts the active item");
        {
            std::unique_lock<std::mutex> gateLock(gateMutex);
            expect(gateChanged.wait_for(gateLock, std::chrono::seconds(2), [&] { return consumerEntered; }),
                   "dataset-only integrity test blocks its consumer");
        }
        for (std::size_t i = 0; i < LiveFrameDispatcher::capacity(); ++i) {
            meta.delivered = (i == 0) ? 103 : meta.delivered + 1;
            expect(datasetOnly.offer(image, meta, 1.0, membership).accepted,
                   "dataset-only integrity test fills each queue slot");
        }
        meta.delivered++;
        expect(!datasetOnly.offer(image, meta, 1.0, membership).accepted,
               "dataset-only overflow rejects the incoming item");
        const std::uint64_t checkpoint = datasetOnly.closeDatasetBoundary();
        {
            std::lock_guard<std::mutex> gateLock(gateMutex);
            releaseConsumer = true;
        }
        gateChanged.notify_all();
        datasetOnly.waitThrough(checkpoint);
        datasetOnly.stopAndDrain();
        const auto integrity = datasetOnly.datasetIntegrity();
        expect(datasetOnly.integrity().handoffAccepted == 0 && integrity.handoffAccepted == 17,
               "dataset-only handoff integrity is independent from collection integrity");
        expect(integrity.sourceGapCount == 2 && integrity.sourceGaps.size() == 1 &&
                   integrity.sourceGaps.front().first == 101 && integrity.sourceGaps.front().last == 102,
               "dataset-only source gaps retain exact delivery ranges");
        expect(integrity.queueRejectedCount == 1 && integrity.queueRejected.size() == 1 &&
                   integrity.queueRejected.front().first == 18 && integrity.queueRejected.front().last == 18,
               "dataset-only overflow retains the exact rejected handoff range");
        expect(integrity.consumerFailureCount == 17 && integrity.consumerFailures.size() == 1 &&
                   integrity.consumerFailures.front().first == 1 && integrity.consumerFailures.front().last == 17,
               "dataset-only consumer failure retains the active and queued handoff range");

        QTemporaryDir output;
        DatasetCaptureSession session;
        DatasetCaptureConfig config;
        config.sessionDir = std::filesystem::path(output.path().toStdWString()) / "dataset";
        config.sessionId = "dataset_integrity";
        config.sourceType = "live_stream";
        config.batchTarget = 10;
        std::string error;
        expect(output.isValid() && session.start(config, error), "dataset integrity metadata session starts");
        DatasetCaptureIntegrity persisted;
        persisted.handoffAccepted = integrity.handoffAccepted;
        persisted.sourceGapCount = integrity.sourceGapCount;
        persisted.queueRejectedCount = integrity.queueRejectedCount;
        persisted.consumerFailureCount = integrity.consumerFailureCount;
        for (const auto& range : integrity.sourceGaps)
            persisted.sourceGaps.push_back({range.first, range.last});
        for (const auto& range : integrity.queueRejected)
            persisted.queueRejected.push_back({range.first, range.last});
        for (const auto& range : integrity.consumerFailures)
            persisted.consumerFailures.push_back({range.first, range.last});
        session.setIntegrity(std::move(persisted));
        session.setStopReason("error");
        expect(session.finalize(error), "dataset integrity metadata finalizes");
        const QString manifestPath =
            QDir(QString::fromStdWString(config.sessionDir.wstring())).filePath("metadata/dataset_manifest.json");
        QFile manifestFile(manifestPath);
        expect(manifestFile.open(QIODevice::ReadOnly), "dataset integrity manifest opens");
        const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        const QJsonObject integrityObject = manifest.value("integrity").toObject();
        expect(integrityObject.value("handoff_accepted_count").toInteger() == 17 &&
                   integrityObject.value("source_gap_count").toInteger() == 2 &&
                   integrityObject.value("queue_rejection_count").toInteger() == 1 &&
                   integrityObject.value("consumer_failure_count").toInteger() == 17,
               "dataset manifest persists exact additive integrity counts");
        expect(integrityObject.value("source_gaps").toArray().first().toArray().first().toInteger() == 101 &&
                   integrityObject.value("queue_rejections").toArray().first().toArray().first().toInteger() == 18 &&
                   integrityObject.value("consumer_failures").toArray().first().toArray().first().toInteger() == 1,
               "dataset manifest persists exact additive integrity ranges");
    }

    {
        LiveFrameDispatcher continued(
            [](const QImage&, const FrameMeta&, double, std::uint64_t, LiveFrameDispatcher::Membership) {});
        QImage image(1, 1, QImage::Format_Grayscale8);
        LiveFrameDispatcher::Membership membership;
        membership.datasetCapture = true;
        FrameMeta meta;
        continued.openDatasetBoundary();
        meta.delivered = 10;
        continued.offer(image, meta, 1.0, membership);
        meta.delivered = 12;
        continued.offer(image, meta, 1.0, membership);
        continued.waitThrough(continued.closeDatasetBoundary());
        continued.resumeDatasetBoundary();
        meta.delivered = 15;
        continued.offer(image, meta, 1.0, membership);
        continued.waitThrough(continued.closeDatasetBoundary());
        continued.stopAndDrain();
        const auto combined = continued.datasetIntegrity();
        expect(combined.handoffAccepted == 3 && combined.sourceGapCount == 3 &&
                   combined.sourceGaps.size() == 2 && combined.sourceGaps[0].first == 11 &&
                   combined.sourceGaps[0].last == 11 && combined.sourceGaps[1].first == 13 &&
                   combined.sourceGaps[1].last == 14,
               "dataset continuation preserves combined counts and ranges across batch segments");

        QTemporaryDir output;
        DatasetCaptureSession session;
        DatasetCaptureConfig config;
        config.sessionDir = std::filesystem::path(output.path().toStdWString()) / "continued_dataset";
        config.sessionId = "continued_dataset_integrity";
        config.sourceType = "live_stream";
        config.batchTarget = 10;
        std::string error;
        expect(output.isValid() && session.start(config, error), "continued dataset metadata session starts");
        DatasetCaptureIntegrity persisted;
        persisted.handoffAccepted = combined.handoffAccepted;
        persisted.sourceGapCount = combined.sourceGapCount;
        for (const auto& range : combined.sourceGaps)
            persisted.sourceGaps.push_back({range.first, range.last});
        session.setIntegrity(std::move(persisted));
        session.setStopReason("cancelled");
        expect(session.finalize(error), "continued dataset metadata finalizes");
        const QDir metadataDir(
            QDir(QString::fromStdWString(config.sessionDir.wstring())).filePath("metadata"));
        auto readIntegrity = [&](const QString& fileName) {
            QFile file(metadataDir.filePath(fileName));
            expect(file.open(QIODevice::ReadOnly), (fileName + " opens").toStdString());
            return QJsonDocument::fromJson(file.readAll()).object().value("integrity").toObject();
        };
        const QJsonObject manifestIntegrity = readIntegrity("dataset_manifest.json");
        const QJsonObject sessionIntegrity = readIntegrity("collection_session.json");
        for (const auto& integrity : {manifestIntegrity, sessionIntegrity}) {
            const QJsonArray ranges = integrity.value("source_gaps").toArray();
            expect(integrity.value("handoff_accepted_count").toInteger() == 3 &&
                       integrity.value("source_gap_count").toInteger() == 3 && ranges.size() == 2 &&
                       ranges[0].toArray()[0].toInteger() == 11 && ranges[1].toArray()[0].toInteger() == 13 &&
                       ranges[1].toArray()[1].toInteger() == 14,
                   "continued dataset JSON preserves combined integrity counts and ranges");
        }
    }

    {
        std::mutex gateMutex;
        std::condition_variable gateChanged;
        bool consumerEntered = false;
        bool releaseConsumer = false;
        std::vector<LiveFrameDispatcher::Membership> observed;
        LiveFrameDispatcher snapshots(
            [&](const QImage&, const FrameMeta&, double, std::uint64_t,
                LiveFrameDispatcher::Membership membership) {
                std::unique_lock<std::mutex> gateLock(gateMutex);
                if (!consumerEntered) {
                    consumerEntered = true;
                    gateChanged.notify_all();
                    gateChanged.wait(gateLock, [&] { return releaseConsumer; });
                }
                observed.push_back(membership);
            });
        QImage image(1, 1, QImage::Format_Grayscale8);
        FrameMeta meta;
        meta.delivered = 1;
        expect(snapshots.offer(image, meta, 1.0, {}).accepted, "membership test accepts the blocking item");
        {
            std::unique_lock<std::mutex> gateLock(gateMutex);
            expect(gateChanged.wait_for(gateLock, std::chrono::seconds(2), [&] { return consumerEntered; }),
                   "membership test blocks before the delayed item is offered");
        }
        LiveFrameDispatcher::Membership expected{true, false, true, true, true, false};
        meta.delivered = 2;
        expect(snapshots.offer(image, meta, 1.0, expected).accepted,
               "membership test accepts the delayed item");
        LiveFrameDispatcher::Membership changed{false, true, false, false, false, true};
        expected = changed;
        {
            std::lock_guard<std::mutex> gateLock(gateMutex);
            releaseConsumer = true;
        }
        gateChanged.notify_all();
        snapshots.stopAndDrain();
        expect(observed.size() == 2 && observed[1].recording && !observed[1].sequenceRunning &&
                   observed[1].pipelineEnabled && observed[1].collection && observed[1].datasetCapture &&
                   !observed[1].liveLogging,
               "all six operation memberships remain fixed while a frame waits in the queue");
    }

    {
        std::vector<bool> collectionMembership;
        std::vector<bool> datasetMembership;
        LiveFrameDispatcher boundaries(
            [&](const QImage&, const FrameMeta&, double, std::uint64_t, LiveFrameDispatcher::Membership membership) {
                collectionMembership.push_back(membership.collection);
                datasetMembership.push_back(membership.datasetCapture);
            });
        QImage image(1, 1, QImage::Format_Grayscale8);
        FrameMeta meta;
        boundaries.openCollectionBoundary();
        boundaries.openDatasetBoundary();
        auto bothMemberships = makeCollectionMembership(true);
        bothMemberships.datasetCapture = true;
        meta.delivered = 100;
        boundaries.offer(image, meta, 1.0, bothMemberships);
        meta.delivered = 103;
        boundaries.offer(image, meta, 1.0, bothMemberships);
        const std::uint64_t checkpoint = boundaries.closeCollectionBoundary();
        const std::uint64_t datasetCheckpoint = boundaries.closeDatasetBoundary();
        meta.delivered = 104;
        boundaries.offer(image, meta, 1.0, bothMemberships);
        boundaries.waitThrough(checkpoint);
        boundaries.stopAndDrain();

        const auto integrity = boundaries.integrity();
        expect(integrity.handoffAccepted == 2 && integrity.sourceGapCount == 2 &&
                   integrity.sourceGaps.size() == 1 && integrity.sourceGaps.front().first == 101 &&
                   integrity.sourceGaps.front().last == 102,
               "source delivery gaps remain distinct and exact within one collection session");
        expect(collectionMembership == std::vector<bool>({true, true, false}),
               "frames accepted after the collection boundary cannot enter the finalized collection");
        expect(datasetCheckpoint == checkpoint && datasetMembership == std::vector<bool>({true, true, false}),
               "frames accepted after the dataset boundary cannot enter the finalized dataset");
    }

    {
        std::atomic<bool> queuedGuiWorkRan(false);
        LiveFrameDispatcher nonblockingPrompt(
            [&](const QImage&, const FrameMeta&, double, std::uint64_t, LiveFrameDispatcher::Membership) {
                QMetaObject::invokeMethod(QCoreApplication::instance(),
                                          [&] { queuedGuiWorkRan.store(true); }, Qt::QueuedConnection);
            });
        QImage image(1, 1, QImage::Format_Grayscale8);
        FrameMeta meta;
        meta.delivered = 1;
        LiveFrameDispatcher::Membership membership;
        membership.datasetCapture = true;
        expect(nonblockingPrompt.offer(image, meta, 1.0, membership).accepted,
               "nonblocking prompt test accepts one dataset frame");
        const auto drainStart = std::chrono::steady_clock::now();
        nonblockingPrompt.stopAndDrain();
        expect(std::chrono::steady_clock::now() - drainStart < std::chrono::milliseconds(250) &&
                   !queuedGuiWorkRan.load(),
               "queued GUI continuation cannot block dispatcher drain during application exit");
        QCoreApplication::removePostedEvents(QCoreApplication::instance());
    }

    {
        std::mutex gateMutex;
        std::condition_variable gateChanged;
        bool consumerEntered = false;
        bool releaseConsumer = false;
        QTemporaryDir output;
        LiveDataCollectionWriter writer;
        std::string writerError;
        expect(output.isValid() && writer.start(output.path(), writerError),
               "writer-fault test starts a real collection writer");
        expect(QDir(QDir(writer.sessionDir()).filePath("stream")).removeRecursively(),
               "writer-fault test removes only its temporary stream directory");
        LiveFrameDispatcher faulting([&](const QImage& image, const FrameMeta& meta, double, std::uint64_t,
                                         LiveFrameDispatcher::Membership) {
            std::unique_lock<std::mutex> gateLock(gateMutex);
            consumerEntered = true;
            gateChanged.notify_all();
            gateChanged.wait(gateLock, [&] { return releaseConsumer; });
            gateLock.unlock();
            PipelineEvent event;
            std::string error;
            if (!writer.writeFrame(image, event, false, meta.frameIndex, error))
                throw std::runtime_error(error);
        });
        QImage image(1, 1, QImage::Format_Grayscale8);
        FrameMeta meta;
        meta.delivered = 1;
        expect(faulting.offer(image, meta, 1.0, makeCollectionMembership(true)).accepted,
               "fault test accepts the active item");
        {
            std::unique_lock<std::mutex> gateLock(gateMutex);
            expect(gateChanged.wait_for(gateLock, std::chrono::seconds(2), [&] { return consumerEntered; }),
                   "fault test consumer enters before queued work is added");
        }
        meta.delivered = 2;
        expect(faulting.offer(image, meta, 1.0, makeCollectionMembership(true)).accepted,
               "fault test accepts one queued item");
        const std::uint64_t checkpoint = faulting.closeCollectionBoundary();
        {
            std::lock_guard<std::mutex> gateLock(gateMutex);
            releaseConsumer = true;
        }
        gateChanged.notify_all();
        faulting.waitThrough(checkpoint);
        faulting.stopAndDrain();
        const auto integrity = faulting.integrity();
        expect(faulting.faulted() && integrity.consumerFailureCount == 2 &&
                   integrity.consumerFailures.size() == 1 && integrity.consumerFailures.front().first == 1 &&
                   integrity.consumerFailures.front().last == 2,
               "real writer failure records the active and queued collection range and unblocks drain");
    }
}

} // namespace

extern "C" int32 DAQmxGetExtendedErrorInfo(char buffer[], uInt32 bufferSize) {
    if (bufferSize)
        buffer[0] = '\0';
    return 0;
}
extern "C" int32 DAQmxGetSysDevNames(char buffer[], uInt32 bufferSize) { return bufferSize ? (buffer[0] = '\0', 0) : 0; }
extern "C" int32 DAQmxGetDevProductType(const char[], char buffer[], uInt32 bufferSize) { return bufferSize ? (buffer[0] = '\0', 0) : 0; }
extern "C" int32 DAQmxGetDevAOPhysicalChans(const char[], char buffer[], uInt32 bufferSize) { return bufferSize ? (buffer[0] = '\0', 0) : 0; }
extern "C" int32 DAQmxGetDevAOMaxRate(const char[], float64* value) {
    fakeDaq.calls.push_back("max_rate");
    *value = fakeDaq.deviceMaxRate;
    return 0;
}
extern "C" int32 DAQmxCreateTask(const char[], TaskHandle* task) { fakeDaq.calls.push_back("create"); *task = reinterpret_cast<TaskHandle>(1); return 0; }
extern "C" int32 DAQmxCreateAOVoltageChan(TaskHandle, const char[], const char[], float64, float64, int32, const char[]) { fakeDaq.calls.push_back("channel"); return 0; }
extern "C" int32 DAQmxCfgSampClkTiming(TaskHandle, const char[], float64 rate, int32, int32, uInt32) {
    fakeDaq.calls.push_back("timing"); fakeDaq.timingRates.push_back(rate); return fakeDaq.timingFailures-- > 0 ? -1 : 0;
}
extern "C" int32 DAQmxCfgOutputBuffer(TaskHandle, uInt32) { fakeDaq.calls.push_back("buffer"); return 0; }
extern "C" int32 DAQmxStopTask(TaskHandle) { fakeDaq.calls.push_back("stop"); return 0; }
extern "C" int32 DAQmxClearTask(TaskHandle) { fakeDaq.calls.push_back("clear"); return 0; }
extern "C" int32 DAQmxWriteAnalogF64(TaskHandle, int32 samples, bool32 autoStart, float64 timeout, bool32 dataLayout,
                                      const float64 values[], int32* written, bool32*) {
    fakeDaq.calls.push_back("write");
    fakeDaq.writeSampleCounts.push_back(samples);
    fakeDaq.writeAutoStarts.push_back(autoStart);
    fakeDaq.writeTimeouts.push_back(timeout);
    fakeDaq.writeDataLayouts.push_back(dataLayout);
    fakeDaq.waveform.assign(values, values + samples);
    *written = samples;
    return 0;
}
extern "C" int32 DAQmxWaitUntilTaskDone(TaskHandle, float64 timeout) { fakeDaq.calls.push_back("wait"); fakeDaq.waitTimeouts.push_back(timeout); return fakeDaq.waitStatus; }

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::addLibraryPath(QLibraryInfo::path(QLibraryInfo::PluginsPath));
    characterizeDaqTrigger();
    characterizePersistence();
    characterizeBackgroundTasks();
    characterizeLiveFrameDispatcher();
    if (failures != 0) {
        std::cerr << failures << " P0-2A characterization assertion(s) failed\n";
        return 1;
    }
    std::cout << "P0-2A characterization passed.\n";
    return 0;
}
