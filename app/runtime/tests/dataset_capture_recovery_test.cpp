#include "../v2/dataset/dataset_capture_service.h"
#include "../detection/droplet_detector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <iostream>

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
        return value;
    }
    int calls = 0;
};

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
    QTemporaryDir temporary;
    QString error;
    qint64 now = 0;

    OperationCoordinator operations;
    FakeDetector detector;
    DatasetCaptureService manual(operations, detector, [&] { return now; });
    if (!check(manual.start(request(temporary.path(), "manual"), &error), error) ||
        !check(manual.offerFrame(frame(), meta(1), 1000.0, &error), error) ||
        !check(manual.offerFrame(frame(), meta(3), 1000.0, &error), error))
        return 1;
    now = 50'000'000;
    const bool paused = manual.pause(&error);
    if (!check(paused, error.isEmpty() ? "Manual pause failed: " + manual.snapshot().error : error))
        return 2;
    const QString manualFolder = manual.snapshot().folder;
    now = 5'000'000'000;
    if (!check(manual.resume(&error), error) ||
        !check(manual.offerFrame(frame(), meta(10), 1000.0, &error), error) ||
        !check(manual.stop(&error), error))
        return 3;
    const auto manualManifest =
        DatasetManifestV2::load(QDir(manualFolder).filePath("dataset.json"), &error);
    if (!check(manualManifest && manualManifest->data().records.size() == 1 &&
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
    if (!check(QFileInfo(QDir(manualFolder).filePath("crops/crop_00000001.png")).isFile(),
               "Exactly one event-entered crop was not written") ||
        !check(operations.snapshot().lifecycle == OperationLifecycle::Idle,
               "Completed capture retained its operation lease"))
        return 6;

    OperationCoordinator timedOperations;
    FakeDetector timedDetector;
    now = 0;
    DatasetCaptureService timed(timedOperations, timedDetector, [&] { return now; });
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
    DatasetCaptureService interrupted(interruptedOperations, interruptedDetector,
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
    interrupted.pause(&error);
    const auto interruptedState = interrupted.snapshot();
    const auto interruptedManifest =
        DatasetManifestV2::load(QDir(interruptedFolder).filePath("dataset.json"), &error);
    if (!check(interruptedState.lifecycle == OperationLifecycle::Interrupted &&
                   interruptedManifest &&
                   interruptedManifest->data().provenance.status == "interrupted" &&
                   interruptedManifest->data().provenance.sequence.integrity.consumerFailures.count >= 1 &&
                   interruptedOperations.snapshot().lifecycle == OperationLifecycle::Idle,
               "Interrupted capture did not preserve recoverable canonical data"))
        return 12;

    OperationCoordinator emptyOperations;
    FakeDetector emptyDetector;
    DatasetCaptureService empty(emptyOperations, emptyDetector, [&] { return now; });
    if (!check(empty.start(request(temporary.path(), "empty"), &error), error))
        return 13;
    const QString emptyFolder = empty.snapshot().folder;
    if (!check(!empty.stop(&error), "Zero-frame capture incorrectly completed") ||
        !check(!QFileInfo(QDir(emptyFolder).filePath("dataset.json")).exists(),
               "Zero-frame failure claimed a canonical Dataset") ||
        !check(QFileInfo(QDir(emptyFolder).filePath("dataset.partial.json")).isFile(),
               "Zero-frame failure did not preserve factual recovery"))
        return 14;
    return 0;
}
