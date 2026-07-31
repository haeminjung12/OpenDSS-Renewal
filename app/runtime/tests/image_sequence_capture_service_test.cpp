#include "v2/camera/camera_service.h"
#include "v2/camera/frame_conversion.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/sequence/image_sequence_capture_service.h"
#include "v2/sequence/sequence_manifest_v2.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using namespace desktop_app::v2;
using namespace desktop_app::v2::sequence;

namespace {

QStringList warnings;
std::mutex warningGateMutex;
std::condition_variable warningGate;
bool blockInitiatingWarning = false;
bool initiatingWarningEntered = false;
bool releaseInitiatingWarning = false;

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg) {
        warnings.push_back(message);
        if (message.contains("consumer failure initiating handoff")) {
            std::unique_lock lock(warningGateMutex);
            if (blockInitiatingWarning) {
                initiatingWarningEntered = true;
                warningGate.notify_all();
                warningGate.wait(lock, [] { return releaseInitiatingWarning; });
            }
        }
    }
}

bool check(bool condition, const char* message) {
    if (!condition)
        std::fprintf(stderr, "%s\n", message);
    return condition;
}

class CameraDevice final : public ICameraDevice {
  public:
    QString deviceId() const override { return QStringLiteral("sequence-test-camera"); }
    bool open(QString*) override { return true; }
    bool start(QString*) override { return true; }
    bool stop(QString*) override { return true; }
    bool close(QString*) override { return true; }
    CameraFrameResult latestFrame(CameraFrame&, QString*) override {
        return CameraFrameResult::NoFrame;
    }
};

struct Fixture {
    ApplicationStateStore store;
    std::unique_ptr<CameraService> camera;
    OperationCoordinator operations;
    qint64 now = 0;

    Fixture() {
        camera = std::make_unique<CameraService>(std::make_unique<CameraDevice>(), store);
        camera->open();
        camera->start();
    }
};

CameraFrame frame(quint64 delivery, uchar value = 42) {
    CameraFrame result;
    result.pixelFormat = CameraPixelFormat::Mono8;
    result.width = 3;
    result.height = 2;
    result.rowBytes = 3;
    result.bitDepth = 8;
    result.deliveryId = delivery;
    result.monotonicTimestampNs = static_cast<qint64>(delivery);
    result.bytes = QByteArray(6, static_cast<char>(value));
    return result;
}

CameraFrame frame16(quint64 delivery, quint16 value = 0x7f00) {
    CameraFrame result;
    result.pixelFormat = CameraPixelFormat::Mono16;
    result.width = 3;
    result.height = 2;
    result.rowBytes = 6;
    result.bitDepth = 16;
    result.deliveryId = delivery;
    result.monotonicTimestampNs = static_cast<qint64>(delivery);
    result.bytes.resize(12);
    for (int offset = 0; offset != result.bytes.size(); offset += 2) {
        result.bytes[offset] = static_cast<char>(value & 0xff);
        result.bytes[offset + 1] = static_cast<char>(value >> 8);
    }
    return result;
}

ImageSequenceCaptureRequest request(const QString& root,
                                    std::optional<double> duration = std::nullopt) {
    return {root, "Sequence: test", "Experiment", "Notes", duration, "2.0.0",
            QJsonObject{{"exposure_us", 200}}};
}

bool waitForLifecycle(ImageSequenceCaptureService& service, OperationLifecycle expected) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (service.snapshot().lifecycle == expected)
            return true;
        QThread::msleep(5);
    }
    return false;
}

QJsonObject readJsonObject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool manualCompletion() {
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    if (!check(service.start(request(root.path()), &error), "Manual sequence must start.") ||
        !check(service.offerFrame(frame(1), 25.0, &error), "Manual frame must be offered.") ||
        !check(service.stop(&error), "Manual sequence must stop cleanly.")) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return false;
    }
    const auto state = service.snapshot();
    const QString framePath = QDir(state.folder).filePath("frames/frame_00000001.tif");
    QString loadError;
    const auto manifest =
        SequenceManifestV2::load(QDir(state.folder).filePath("sequence.json"), &loadError);
    return check(state.lifecycle == OperationLifecycle::Completed && state.savedFrameCount == 1,
                 "Manual sequence snapshot must be completed with one frame.") &&
           check(!QImageReader(framePath).read().isNull(), "Saved TIFF must be readable.") &&
           check(manifest && manifest->data().frameCount == 1 &&
                     manifest->data().stopReason == "user" &&
                     !manifest->data().requestedDurationSeconds,
                 "Manual sequence manifest must be factual.") &&
           check(!QFileInfo::exists(QDir(state.folder).filePath("sequence.partial.json")),
                 "Clean completion must remove the recovery marker.") &&
           check(fixture.operations.snapshot().lifecycle == OperationLifecycle::Idle,
                 "Manual completion must release its operation lock.");
}

bool timedPauseCompletion() {
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    if (!service.start(request(root.path(), 2.0), &error) ||
        !service.offerFrame(frame(10), 20.0, &error))
        return check(false, "Timed sequence setup must succeed.");
    const QString folder = service.snapshot().folder;
    fixture.now = 1'000'000'000;
    if (!service.pause(&error))
        return check(false, "Timed sequence must pause.");
    fixture.now = 101'000'000'000;
    if (!service.pollDuration(&error) ||
        service.snapshot().lifecycle != OperationLifecycle::Paused)
        return check(false, "Paused wall time must not expire active Duration.");
    if (!service.resume(&error) || service.snapshot().folder != folder ||
        !service.offerFrame(frame(100), 20.0, &error))
        return check(false, "Resume must continue the same folder.");
    fixture.now = 102'100'000'000;
    if (!service.pollDuration(&error))
        return check(false, "Timed poll must finalize successfully.");
    const auto state = service.snapshot();
    QString loadError;
    const auto manifest =
        SequenceManifestV2::load(QDir(folder).filePath("sequence.json"), &loadError);
    return check(state.lifecycle == OperationLifecycle::Completed &&
                     state.savedFrameCount == 2,
                 "Timed sequence must complete with two cumulatively numbered frames.") &&
           check(QFileInfo::exists(QDir(folder).filePath("frames/frame_00000001.tif")) &&
                     QFileInfo::exists(QDir(folder).filePath("frames/frame_00000002.tif")),
                 "Pause/resume must preserve one numbering sequence.") &&
           check(manifest && manifest->data().stopReason == "duration" &&
                     manifest->data().integrity.sourceFrameGaps.count == 0,
                 "Timed manifest must exclude pause delivery gaps.");
}

bool sourceGapCompletion() {
    warnings.clear();
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    if (!service.start(request(root.path()), &error) ||
        !service.offerFrame(frame(1), 30.0, &error) ||
        !service.offerFrame(frame(4), 30.0, &error) || !service.stop(&error))
        return check(false, "Gap sequence must complete silently.");
    const auto state = service.snapshot();
    QString loadError;
    const auto manifest =
        SequenceManifestV2::load(QDir(state.folder).filePath("sequence.json"), &loadError);
    bool logged = false;
    for (const QString& warning : warnings)
        logged |= warning.contains("2 - 3") || warning.contains("2-3");
    return check(manifest && manifest->data().integrity.sourceFrameGaps.count == 2,
                 "Gap manifest must record two missing source frames.") &&
           check(manifest->data().integrity.sourceFrameGaps.ranges.size() == 1 &&
                     manifest->data().integrity.sourceFrameGaps.ranges.front().first == 2 &&
                     manifest->data().integrity.sourceFrameGaps.ranges.front().last == 3,
                 "Gap manifest must record exact inclusive range 2-3.") &&
           check(logged, "Gap range must be written to diagnostics.");
}

bool input16PersistsAs8Bit() {
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    if (!service.start(request(root.path()), &error) ||
        !service.offerFrame(frame16(1), 31.6, &error) || !service.stop(&error)) {
        return check(false, "16-bit input sequence must finalize successfully.");
    }
    const auto state = service.snapshot();
    const QString framePath = QDir(state.folder).filePath("frames/frame_00000001.tif");
    const QImage saved = QImageReader(framePath).read();
    QString loadError;
    const auto manifest =
        SequenceManifestV2::load(QDir(state.folder).filePath("sequence.json"), &loadError);
    return check(!saved.isNull() && saved.depth() == 8,
                 "Image Sequence TIFF output must remain 8-bit for 16-bit camera input.") &&
           check(manifest && manifest->data().bitDepth == 8,
                 "Image Sequence manifest must report the persisted 8-bit format.");
}

bool deferredFinalizationDoesNotThrottleCapture() {
    QTemporaryDir root;
    Fixture fixture;
    std::mutex writerMutex;
    std::condition_variable writerGate;
    bool writerEntered = false;
    bool releaseWriter = false;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; }, {},
        [&](const QImage& image, const QString& target, QString* error) {
            {
                std::unique_lock lock(writerMutex);
                if (!writerEntered) {
                    writerEntered = true;
                    writerGate.notify_all();
                    writerGate.wait(lock, [&] { return releaseWriter; });
                }
            }
            QImageWriter writer(target, "tiff");
            if (writer.write(image))
                return true;
            if (error)
                *error = writer.errorString();
            return false;
        });
    const auto releaseBlockedWriter = [&] {
        {
            std::lock_guard lock(writerMutex);
            releaseWriter = true;
        }
        writerGate.notify_all();
    };

    QString error;
    if (!service.start(request(root.path()), &error))
        return check(false, "Deferred-finalization fixture must start.");
    constexpr quint64 frameCount = 32;
    for (quint64 delivery = 1; delivery <= frameCount; ++delivery) {
        if (!service.offerFrame(frame(delivery), 25.0, &error)) {
            releaseBlockedWriter();
            return check(false, "Spool persistence must accept the bounded test stream.");
        }
        QThread::msleep(1);
    }
    if (!service.pause(&error)) {
        releaseBlockedWriter();
        return check(false, "Pausing must drain all offered frames into the spool.");
    }
    const auto captured = service.snapshot();
    if (!check(captured.capturedFrameCount == frameCount && captured.savedFrameCount == 0,
               "TIFF writing must remain deferred while frames are captured.")) {
        releaseBlockedWriter();
        return false;
    }

    bool stopped = false;
    QString stopError;
    std::thread finalizer([&] { stopped = service.stop(&stopError); });
    {
        std::unique_lock lock(writerMutex);
        if (!writerGate.wait_for(lock, std::chrono::seconds(2),
                                 [&] { return writerEntered; })) {
            releaseWriter = true;
            lock.unlock();
            writerGate.notify_all();
            finalizer.join();
            return check(false, "Stop must enter the deferred TIFF writer.");
        }
    }
    const auto finalizing = service.snapshot();
    const bool finalizingFacts =
        check(finalizing.lifecycle == OperationLifecycle::Stopping &&
                  finalizing.capturedFrameCount == frameCount &&
                  finalizing.savedFrameCount == 0 &&
                  finalizing.integrity.queueRejections.count == 0,
              "Finalizing must expose captured, saved, and rejection counts independently.");
    releaseBlockedWriter();
    finalizer.join();
    const auto completed = service.snapshot();
    return finalizingFacts &&
           check(stopped && stopError.isEmpty() &&
                     completed.lifecycle == OperationLifecycle::Completed &&
                     completed.capturedFrameCount == frameCount &&
                     completed.savedFrameCount == frameCount,
                 "Deferred finalization must publish every captured frame.");
}

bool pauseOfferRace() {
    QTemporaryDir root;
    Fixture fixture;
    std::mutex gateMutex;
    std::condition_variable gate;
    bool entered = false;
    bool release = false;
    bool blockFirst = true;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; },
        [&](const CameraFrame& input, QString* error) {
            std::unique_lock lock(gateMutex);
            if (blockFirst) {
                blockFirst = false;
                entered = true;
                gate.notify_all();
                gate.wait(lock, [&] { return release; });
            }
            lock.unlock();
            return convertCameraFrame(input, error);
        });
    QString error;
    if (!service.start(request(root.path()), &error))
        return check(false, "Race fixture must start.");

    bool offerResult = true;
    QString offerError;
    std::thread producer([&] {
        offerResult = service.offerFrame(frame(1), 25.0, &offerError);
    });
    {
        std::unique_lock lock(gateMutex);
        gate.wait(lock, [&] { return entered; });
    }
    const bool paused = service.pause(&error);
    {
        std::lock_guard lock(gateMutex);
        release = true;
    }
    gate.notify_all();
    producer.join();
    if (!check(paused && !offerResult &&
                   offerError.contains("not accepting", Qt::CaseInsensitive),
               "A frame converting across Pause must not be submitted.")) {
        return false;
    }
    if (!service.resume(&error) || !service.offerFrame(frame(2), 25.0, &error) ||
        !service.stop(&error)) {
        return check(false, "Race fixture must resume and complete.");
    }
    return check(service.snapshot().savedFrameCount == 1,
                 "Post-pause race must save only the frame offered after Resume.");
}

bool zeroFrameFailure() {
    QTemporaryDir manualRoot;
    Fixture manualFixture;
    ImageSequenceCaptureService manual(
        *manualFixture.camera, manualFixture.operations, [&] { return manualFixture.now; });
    QString error;
    if (!manual.start(request(manualRoot.path()), &error))
        return check(false, "Zero-frame manual fixture must start.");
    const QString manualFolder = manual.snapshot().folder;
    if (!check(!manual.stop(&error) && error == "No frames were captured.",
               "Manual stop with zero frames must fail directly.")) {
        return false;
    }
    const QJsonObject manualRecovery =
        readJsonObject(QDir(manualFolder).filePath("sequence.partial.json"));
    bool ok = check(manual.snapshot().lifecycle == OperationLifecycle::Failed &&
                        manualFixture.operations.snapshot().lifecycle == OperationLifecycle::Idle,
                    "Zero-frame manual failure must release its lease.") &&
              check(manualRecovery.value("status") == "failed" &&
                        manualRecovery.value("stop_reason") == "user" &&
                        manualRecovery.value("error") == "No frames were captured.",
                    "Zero-frame manual recovery marker must be factual.") &&
              check(!QFileInfo::exists(QDir(manualFolder).filePath("sequence.json")),
                    "Zero-frame manual failure must not publish sequence.json.");

    QTemporaryDir timedRoot;
    Fixture timedFixture;
    ImageSequenceCaptureService timed(
        *timedFixture.camera, timedFixture.operations, [&] { return timedFixture.now; });
    if (!timed.start(request(timedRoot.path(), 1.0), &error))
        return check(false, "Zero-frame timed fixture must start.");
    const QString timedFolder = timed.snapshot().folder;
    timedFixture.now = 1'100'000'000;
    ok &= check(!timed.pollDuration(&error) && error == "No frames were captured.",
                "Timed expiry with zero frames must fail directly.");
    const QJsonObject timedRecovery =
        readJsonObject(QDir(timedFolder).filePath("sequence.partial.json"));
    ok &= check(timedRecovery.value("status") == "failed" &&
                    timedRecovery.value("stop_reason") == "duration",
                "Zero-frame timed recovery marker must record duration.");
    ok &= check(timedFixture.operations.snapshot().lifecycle == OperationLifecycle::Idle,
                "Zero-frame timed failure must release its lease.");
    return ok;
}

bool startConflicts() {
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    bool ok = check(service.start(request(root.path()), &error), "First start must succeed.") &&
              check(!service.start(request(root.path()), &error), "Double start must fail.") &&
              check(service.offerFrame(frame(1), 25.0, &error) && service.stop(&error),
                    "Double-start fixture must still complete.");

    Fixture conflictedFixture;
    auto held = conflictedFixture.operations.acquire(OperationKind::Training,
                                                     ResourceLock::Storage);
    ImageSequenceCaptureService conflicted(
        *conflictedFixture.camera, conflictedFixture.operations,
        [&] { return conflictedFixture.now; });
    ok &= check(held.acquired() && !conflicted.start(request(root.path()), &error),
                "Conflicting operation must block Image Sequence start.");
    return ok;
}

bool writeFailure() {
    warnings.clear();
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; });
    QString error;
    if (!service.start(request(root.path()), &error))
        return check(false, "Write-failure sequence must start.");
    const QString folder = service.snapshot().folder;
    QFile collision(QDir(folder).filePath("frames/frame_00000001.tif"));
    if (!collision.open(QIODevice::WriteOnly) || collision.write("collision") < 0)
        return check(false, "Write collision fixture must be created.");
    collision.close();
    if (!service.offerFrame(frame(1), 25.0, &error))
        return check(false, "Spooling must remain independent of a future TIFF collision.");
    if (service.stop(&error))
        return check(false, "The deferred TIFF collision must fail finalization.");
    const auto state = service.snapshot();
    const QJsonObject recovery =
        readJsonObject(QDir(folder).filePath("sequence.partial.json"));
    const QJsonObject consumer =
        recovery.value("integrity").toObject().value("consumer_failures").toObject();
    const QJsonArray failureRanges = consumer.value("ranges").toArray();
    return check(QFileInfo::exists(QDir(folder).filePath("sequence.partial.json")),
                 "Failed sequence must retain its recovery marker.") &&
           check(!QFileInfo::exists(QDir(folder).filePath("sequence.json")),
                 "Failed sequence must not publish a canonical manifest.") &&
           check(state.capturedFrameCount == 1 && state.savedFrameCount == 0 &&
                     state.integrity.consumerFailures.count == 1,
                 "Failed finalization must preserve captured and saved counts independently.") &&
           check(recovery.value("status") == "failed" &&
                     recovery.value("stop_reason") == "finalization_error" &&
                     !recovery.value("error").toString().isEmpty() &&
                     recovery.value("captured_frame_count").toInteger(-1) == 1 &&
                     recovery.value("saved_frame_count").toInteger(-1) == 0,
                 "Failed recovery marker must record factual failure state.") &&
           check(consumer.value("count").toInteger(-1) == 1 &&
                     failureRanges.size() == 1 &&
                     failureRanges.at(0).toObject().value("first").toInteger(-1) == 1 &&
                     failureRanges.at(0).toObject().value("last").toInteger(-1) == 1,
                 "Failed recovery metadata must contain exact failed handoff 1-1.") &&
           check(QFileInfo::exists(QDir(folder).filePath("sequence.frames.partial")),
                 "Failed finalization must retain the recoverable spool.") &&
           check(fixture.operations.snapshot().lifecycle == OperationLifecycle::Idle,
                 "Write failure must release its operation lock.");
}

bool queuedWriteFailureRange() {
    warnings.clear();
    QTemporaryDir root;
    Fixture fixture;
    ImageSequenceCaptureService service(
        *fixture.camera, fixture.operations, [&] { return fixture.now; }, {},
        [](const QImage& image, const QString& target, QString* error) {
            if (target.endsWith(QStringLiteral("frame_00000002.tif"))) {
                if (error)
                    *error = QStringLiteral("Deterministic deferred writer failure.");
                return false;
            }
            QImageWriter writer(target, "tiff");
            if (writer.write(image))
                return true;
            if (error)
                *error = writer.errorString();
            return false;
        });
    QString error;
    if (!service.start(request(root.path()), &error))
        return check(false, "Queued-failure sequence must start.");
    const QString folder = service.snapshot().folder;
    bool offersAccepted = true;
    for (quint64 delivery = 1; delivery <= 4; ++delivery) {
        offersAccepted &= service.offerFrame(frame(delivery), 25.0, &error);
        QThread::msleep(1);
    }
    if (!offersAccepted || service.stop(&error))
        return check(false, "All frames must spool before deterministic finalization failure.");

    const QJsonObject recovery =
        readJsonObject(QDir(folder).filePath("sequence.partial.json"));
    const QJsonObject consumer =
        recovery.value("integrity").toObject().value("consumer_failures").toObject();
    const QJsonArray ranges = consumer.value("ranges").toArray();
    const auto state = service.snapshot();
    return check(state.capturedFrameCount == 4 && state.savedFrameCount == 1,
                 "Finalization failure must retain exact captured and saved counts.") &&
           check(consumer.value("count").toInteger(-1) == 3 && ranges.size() == 1 &&
                     ranges.at(0).toObject().value("first").toInteger(-1) == 2 &&
                     ranges.at(0).toObject().value("last").toInteger(-1) == 4,
                 "Failed recovery metadata must contain the unfinalized range 2-4.");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto previousHandler = qInstallMessageHandler(messageHandler);
    const bool ok = manualCompletion() && input16PersistsAs8Bit() && timedPauseCompletion() &&
                    sourceGapCompletion() && deferredFinalizationDoesNotThrottleCapture() &&
                    pauseOfferRace() && zeroFrameFailure() && startConflicts() &&
                    writeFailure() && queuedWriteFailureRange();
    qInstallMessageHandler(previousHandler);
    return ok ? 0 : 1;
}
