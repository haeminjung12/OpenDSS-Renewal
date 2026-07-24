#include "v2/camera/camera_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/sequence/image_sequence_capture_service.h"
#include "v2/sequence/sequence_manifest_v2.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>
#include <memory>

using namespace desktop_app::v2;
using namespace desktop_app::v2::sequence;

namespace {

QStringList warnings;

void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg)
        warnings.push_back(message);
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
    bool latestFrame(CameraFrame&, QString*) override { return false; }
};

struct Fixture {
    ApplicationStateStore store;
    std::unique_ptr<CameraService> camera;
    OperationCoordinator operations;
    qint64 now = 0;

    Fixture() {
        camera = std::make_unique<CameraService>(std::make_unique<CameraDevice>(), store);
        QString error;
        camera->open(&error);
        camera->start(&error);
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
        return check(false, "Write failure must be asynchronous and nonblocking.");
    if (!waitForLifecycle(service, OperationLifecycle::Failed))
        return check(false, "Consumer write failure must transition to Failed.");
    const auto state = service.snapshot();
    return check(QFileInfo::exists(QDir(folder).filePath("sequence.partial.json")),
                 "Failed sequence must retain its recovery marker.") &&
           check(!QFileInfo::exists(QDir(folder).filePath("sequence.json")),
                 "Failed sequence must not publish a canonical manifest.") &&
           check(state.savedFrameCount == 0 &&
                     state.integrity.consumerFailures.count == 1,
                 "Failed write must not count a frame and must record consumer failure.") &&
           check(fixture.operations.snapshot().lifecycle == OperationLifecycle::Idle,
                 "Write failure must release its operation lock.");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto previousHandler = qInstallMessageHandler(messageHandler);
    const bool ok = manualCompletion() && timedPauseCompletion() &&
                    sourceGapCompletion() && startConflicts() && writeFailure();
    qInstallMessageHandler(previousHandler);
    return ok ? 0 : 1;
}
