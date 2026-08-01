#include "../v2/dataset/dataset_capture_service.h"
#include "../detection/droplet_detector.h"
#include "../detection/droplet_frame_processor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;

namespace {
class FakeDetector final : public IDropletDetector {
  public:
    void reset() override { calls = 0; }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        ++calls;
        DropletDetectionFrame value;
        value.detected = calls <= 2;
        value.eventEntered = calls == 1;
        value.bbox = {2, 3, 12, 20};
        if (calls == 1) {
            value.enteredTrackCount = 2;
            value.enteredTracks[0] = {101, 0, 0.0, {2, 3, 12, 20}, {}};
            value.enteredTracks[1] = {102, 0, 0.0, {14, 2, 10, 18}, {}};
        }
        return value;
    }
    int calls = 0;
};

class GateDetector final : public IDropletDetector {
  public:
    explicit GateDetector(bool eventEntered) : eventEntered_(eventEntered) {}
    void reset() override {}
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        std::unique_lock lock(mutex_);
        entered_ = true;
        enteredChanged_.notify_all();
        released_.wait(lock, [&] { return releasedFlag_; });
        DropletDetectionFrame value;
        value.detected = eventEntered_;
        value.eventEntered = eventEntered_;
        value.bbox = {2, 3, 12, 20};
        if (eventEntered_) {
            value.enteredTrackCount = 1;
            value.enteredTracks[0] = {1, 0, 0.0, value.bbox, {}};
        }
        return value;
    }
    void waitUntilEntered() {
        std::unique_lock lock(mutex_);
        enteredChanged_.wait(lock, [&] { return entered_; });
    }
    void release() {
        std::lock_guard lock(mutex_);
        releasedFlag_ = true;
        released_.notify_all();
    }

  private:
    bool eventEntered_ = false;
    std::mutex mutex_;
    std::condition_variable enteredChanged_;
    std::condition_variable released_;
    bool entered_ = false;
    bool releasedFlag_ = false;
};

std::mutex messageMutex;
std::vector<QString> warningMessages;

void captureMessages(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (type == QtWarningMsg) {
        std::lock_guard lock(messageMutex);
        warningMessages.push_back(message);
    }
}

bool hasFinalIntegrity(const sequence::SequenceIntegrity& integrity) {
    const auto rangeText = [](const sequence::SequenceLossCategory& value) {
        QStringList ranges;
        for (const auto& range : value.ranges)
            ranges.push_back(QString("%1-%2").arg(range.first).arg(range.last));
        return ranges.isEmpty() ? QStringLiteral("none") : ranges.join(',');
    };
    const QString expected =
        QString("Dataset Capture final integrity: source_frame_gaps count %1 ranges %2 "
                "queue_rejections count %3 ranges %4 consumer_failures count %5 ranges %6")
            .arg(integrity.sourceFrameGaps.count)
            .arg(rangeText(integrity.sourceFrameGaps))
            .arg(integrity.queueRejections.count)
            .arg(rangeText(integrity.queueRejections))
            .arg(integrity.consumerFailures.count)
            .arg(rangeText(integrity.consumerFailures));
    std::lock_guard lock(messageMutex);
    return std::find(warningMessages.cbegin(), warningMessages.cend(), expected) !=
           warningMessages.cend();
}

bool check(bool value, const QString& message) {
    if (!value)
        std::cerr << message.toStdString() << '\n';
    return value;
}

DatasetCaptureRequest request(const QString& root, const QString& name,
                              std::optional<double> duration = {}) {
    DatasetCaptureRequest value;
    value.saveRoot = root;
    value.name = name;
    value.experimentType = "test";
    value.notes = "neutral";
    value.durationSeconds = duration;
    value.opendssVersion = "v2";
    value.cameraSettings = {{"camera", "fake"}};
    value.detectionSettings = {{"detector", "fake"}};
    value.programSettings = {{"program", "test"}};
    return value;
}

QImage frame() {
    QImage value(32, 24, QImage::Format_Grayscale8);
    for (int y = 0; y < value.height(); ++y)
        for (int x = 0; x < value.width(); ++x)
            value.scanLine(y)[x] = static_cast<uchar>((x + y * 3) % 256);
    return value;
}

FrameMeta meta(qint64 delivered) {
    FrameMeta value;
    value.width = 32;
    value.height = 24;
    value.bits = 8;
    value.frameIndex = delivered;
    value.delivered = delivered;
    return value;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto previousHandler = qInstallMessageHandler(captureMessages);
    QTemporaryDir temporary;
    QString error;
    qint64 now = 0;

    OperationCoordinator operations;
    FakeDetector detector;
    DropletFrameProcessor manualProcessor(detector);
    DatasetCaptureService manual(operations, manualProcessor, [&] { return now; });
    if (!check(manual.start(request(temporary.path(), "manual"), &error), error) ||
        !check(manual.offerFrame(frame(), meta(1), 1000.0, &error), error) ||
        !check(manual.offerFrame(frame(), meta(3), 1000.0, &error), error))
        return 1;
    now = 50'000'000;
    const bool paused = manual.pause(&error);
    if (!check(paused, error.isEmpty() ? "Manual pause failed: " + manual.snapshot().error : error))
        return 2;
    const QString manualFolder = manual.snapshot().folder;
    if (!check(!QFileInfo(QDir(manualFolder).filePath("sequence/frame_00000001.tif")).exists() &&
                   QFileInfo(QDir(manualFolder).filePath("sequence.frames.partial")).isFile(),
               "Dataset full frames were published before stop finalization"))
        return 30;
    now = 5'000'000'000;
    if (!check(manual.resume(&error), error) ||
        !check(manual.offerFrame(frame(), meta(10), 1000.0, &error), error) ||
        !check(manual.stop(&error), error))
        return 3;
    const auto manualManifest =
        DatasetManifestV2::load(QDir(manualFolder).filePath("dataset.json"), &error);
    if (!check(manualManifest && manualManifest->data().records.size() == 2 &&
                   manualManifest->data().records.front().sourceFrameIndex == 1 &&
                   manualManifest->data().classes.isEmpty() &&
                   manualManifest->data().labels.isEmpty() &&
                   manualManifest->data().provenance.sequence.frameCount == 3 &&
                   manualManifest->data().provenance.sequence.integrity.sourceFrameGaps.count == 1,
               "Manual capture neutral record/integrity contract failed: " + error))
        return 4;
    for (int index = 1; index <= 3; ++index) {
        const QString path = QDir(manualFolder)
                                 .filePath(QString("sequence/frame_%1.tif")
                                               .arg(index, 8, 10, QLatin1Char('0')));
        QImageReader reader(path);
        if (!check(reader.canRead() && !reader.read().isNull(),
                   "Numbered TIFF is unreadable: " + path))
            return 5;
    }
    if (!check(detector.calls == 3 &&
                   QFileInfo(QDir(manualFolder).filePath("crops/droplet_000001.png")).isFile() &&
                   QFileInfo(QDir(manualFolder).filePath("crops/droplet_000002.png")).isFile(),
               "Two same-frame entered crops were not written from one detector call") ||
        !check(operations.snapshot().lifecycle == OperationLifecycle::Idle,
               "Completed capture retained its operation lease"))
        return 6;

    OperationCoordinator lockOperations;
    FakeDetector lockDetector;
    DropletFrameProcessor lockProcessor(lockDetector);
    DatasetCaptureService lockCapture(lockOperations, lockProcessor, [&] { return now; });
    if (!check(lockCapture.start(request(temporary.path(), "identity-lock"), &error), error))
        return 26;
    const QString lockedDataset =
        QDir(lockCapture.snapshot().folder).filePath("dataset.json");
    QFile visibleTarget(lockedDataset);
    if (!visibleTarget.open(QIODevice::WriteOnly) || visibleTarget.write("{}") != 2)
        return 27;
    visibleTarget.close();
    auto sameDatasetRead =
        lockOperations.acquireDataset(lockedDataset, DatasetAccess::Read);
    auto otherDatasetRead = lockOperations.acquireDataset(
        QDir(manualFolder).filePath("dataset.json"), DatasetAccess::Read);
    if (!check(!sameDatasetRead.acquired() && otherDatasetRead.acquired(),
               "Dataset Capture did not block only its exact target Dataset"))
        return 28;
    otherDatasetRead.lease.release();
    QFile::remove(lockedDataset);
    lockCapture.stop(&error);

    OperationCoordinator blockedOperations;
    auto blocker =
        blockedOperations.acquire(OperationKind::ModelTest, ResourceLock::Model);
    FakeDetector blockedDetector;
    DropletFrameProcessor blockedProcessor(blockedDetector);
    DatasetCaptureService blockedCapture(blockedOperations, blockedProcessor, [&] { return now; });
    if (!check(blocker.acquired()
                   && !blockedCapture.start(request(temporary.path(), "blocked-start"), &error)
                   && !QFileInfo::exists(QDir(temporary.path()).filePath("blocked-start")),
               "Failed Dataset Capture start did not clean its new folder"))
        return 29;
    blocker.lease.release();

    OperationCoordinator timedOperations;
    FakeDetector timedDetector;
    now = 0;
    DropletFrameProcessor timedProcessor(timedDetector);
    DatasetCaptureService timed(timedOperations, timedProcessor, [&] { return now; });
    if (!check(timed.start(request(temporary.path(), "timed", 0.1), &error), error) ||
        !check(timed.offerFrame(frame(), meta(1), 500.0, &error), error))
        return 7;
    now = 200'000'000;
    if (!check(timed.pollDuration(&error), error) ||
        !check(timed.snapshot().lifecycle == OperationLifecycle::Completed,
               "Timed capture did not complete"))
        return 8;

    OperationCoordinator interruptedOperations;
    FakeDetector interruptedDetector;
    now = 0;
    DropletFrameProcessor interruptedProcessor(interruptedDetector);
    DatasetCaptureService interrupted(interruptedOperations, interruptedProcessor,
                                      [&] { return now; });
    if (!check(interrupted.start(request(temporary.path(), "interrupted"), &error), error) ||
        !check(interrupted.offerFrame(frame(), meta(1), 500.0, &error), error) ||
        !check(interrupted.pause(&error), error) ||
        !check(interrupted.resume(&error), error))
        return 9;
    const QString interruptedFolder = interrupted.snapshot().folder;
    QFile collision(QDir(interruptedFolder).filePath("sequence/frame_00000002.tif"));
    if (!collision.open(QIODevice::WriteOnly) || collision.write("collision") < 0)
        return 10;
    collision.close();
    if (!check(interrupted.offerFrame(frame(), meta(2), 500.0, &error), error))
        return 11;
    if (!check(!interrupted.stop(&error), "Colliding final output incorrectly completed Dataset"))
        return 31;
    const auto interruptedState = interrupted.snapshot();
    const auto interruptedManifest =
        DatasetManifestV2::load(QDir(interruptedFolder).filePath("dataset.json"), &error);
    if (!check(interruptedState.lifecycle == OperationLifecycle::Interrupted &&
                   interruptedManifest &&
                   interruptedManifest->data().provenance.status == "interrupted" &&
                   QFileInfo(QDir(interruptedFolder).filePath("sequence.frames.partial")).isFile() &&
                   interruptedOperations.snapshot().lifecycle == OperationLifecycle::Idle,
               "Interrupted finalization did not preserve recoverable Dataset data"))
        return 12;

    OperationCoordinator emptyOperations;
    FakeDetector emptyDetector;
    DropletFrameProcessor emptyProcessor(emptyDetector);
    DatasetCaptureService empty(emptyOperations, emptyProcessor, [&] { return now; });
    if (!check(empty.start(request(temporary.path(), "empty"), &error), error))
        return 13;
    const QString emptyFolder = empty.snapshot().folder;
    if (!check(!empty.stop(&error), "Zero-frame capture incorrectly completed") ||
        !check(!QFileInfo(QDir(emptyFolder).filePath("dataset.json")).exists(),
               "Zero-frame failure claimed a canonical Dataset") ||
        !check(QFileInfo(QDir(emptyFolder).filePath("dataset.partial.json")).isFile(),
               "Zero-frame failure did not preserve factual recovery"))
        return 14;

    OperationCoordinator overflowOperations;
    GateDetector overflowDetector(false);
    DropletFrameProcessor overflowProcessor(overflowDetector);
    DatasetCaptureService overflow(overflowOperations, overflowProcessor, [&] { return now; });
    if (!check(overflow.start(request(temporary.path(), "overflow"), &error), error) ||
        !check(overflow.offerFrame(frame(), meta(1), 500.0, &error), error))
        return 15;
    overflowDetector.waitUntilEntered();
    for (qint64 delivered = 2; delivered <= 18; ++delivered) {
        if (!check(overflow.offerFrame(frame(), meta(delivered), 500.0, &error), error))
            return 16;
    }
    overflowDetector.release();
    if (!check(overflow.stop(&error), error))
        return 17;
    const auto overflowManifest = DatasetManifestV2::load(
        QDir(overflow.snapshot().folder).filePath("dataset.json"), &error);
    if (!check(overflowManifest &&
                   overflowManifest->data().provenance.sequence.integrity.queueRejections.count ==
                       1 &&
                   hasFinalIntegrity(
                       overflowManifest->data().provenance.sequence.integrity),
               "Overflow aggregate log does not exactly match Dataset metadata"))
        return 18;

    OperationCoordinator queuedFailureOperations;
    GateDetector queuedFailureDetector(true);
    DropletFrameProcessor queuedFailureProcessor(queuedFailureDetector);
    DatasetCaptureService queuedFailure(queuedFailureOperations, queuedFailureProcessor,
                                        [&] { return now; });
    if (!check(queuedFailure.start(request(temporary.path(), "queued-failure"), &error), error))
        return 19;
    const QString queuedFailureFolder = queuedFailure.snapshot().folder;
    QFile cropCollision(
        QDir(queuedFailureFolder).filePath("crops/droplet_000001.png"));
    if (!cropCollision.open(QIODevice::WriteOnly) || cropCollision.write("collision") < 0)
        return 20;
    cropCollision.close();
    if (!check(queuedFailure.offerFrame(frame(), meta(1), 500.0, &error), error))
        return 21;
    queuedFailureDetector.waitUntilEntered();
    for (qint64 delivered = 2; delivered <= 18; ++delivered) {
        if (!check(queuedFailure.offerFrame(frame(), meta(delivered), 500.0, &error), error))
            return 22;
    }
    queuedFailureDetector.release();
    queuedFailure.pause(&error);
    const auto queuedFailureManifest = DatasetManifestV2::load(
        QDir(queuedFailureFolder).filePath("dataset.json"), &error);
    if (!check(queuedFailureManifest &&
                   queuedFailureManifest->data()
                           .provenance.sequence.integrity.consumerFailures.count == 17 &&
                   queuedFailureManifest->data()
                           .provenance.sequence.integrity.queueRejections.count == 1 &&
                   hasFinalIntegrity(
                       queuedFailureManifest->data().provenance.sequence.integrity),
               "Queued consumer-failure aggregate log does not exactly match metadata"))
        return 23;

    OperationCoordinator scopeOperations;
    FakeDetector scopeDetector;
    QString scopeFolder;
    {
        DropletFrameProcessor scopeProcessor(scopeDetector);
        DatasetCaptureService scoped(scopeOperations, scopeProcessor, [&] { return now; });
        if (!check(scoped.start(request(temporary.path(), "scope-exit"), &error), error) ||
            !check(scoped.offerFrame(frame(), meta(1), 500.0, &error), error) ||
            !check(scoped.pause(&error), error))
            return 24;
        scopeFolder = scoped.snapshot().folder;
    }
    const auto scopeManifest =
        DatasetManifestV2::load(QDir(scopeFolder).filePath("dataset.json"), &error);
    if (!check(scopeManifest &&
                   scopeManifest->data().provenance.status == "interrupted" &&
                   scopeManifest->data().provenance.stopReason == "scope_exit" &&
                   hasFinalIntegrity(scopeManifest->data().provenance.sequence.integrity) &&
                   scopeOperations.snapshot().lifecycle == OperationLifecycle::Idle,
               "Scope destruction did not preserve factual interrupted recovery"))
        return 25;

    qInstallMessageHandler(previousHandler);
    return 0;
}
