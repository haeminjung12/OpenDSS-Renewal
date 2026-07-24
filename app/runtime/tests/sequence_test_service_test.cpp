#include "../detection/droplet_detector.h"
#include "../v2/model/model_load_service.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/run/run_manifest_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence_test/sequence_test_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <utility>

using namespace desktop_app::v2;

namespace desktop_app::v2 {
std::unique_ptr<OnnxInferenceAdapter>
ModelLoadService::preparePersistedActive(const QString&, QString*, QString* error) const {
    if (error)
        *error = QStringLiteral("No production model in this deterministic test.");
    return {};
}
} // namespace desktop_app::v2

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

class FakeDetector final : public IDropletDetector {
public:
    QVector<DropletDetectionFrame> results;
    std::function<void(int)> onProcess;

    void reset() override { index_ = 0; }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        if (onProcess)
            onProcess(index_);
        return index_ < results.size() ? results.at(index_++)
                                       : DropletDetectionFrame{};
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

QString framePath(const QString& root, qint64 index) {
    return QDir(root).filePath(
        QStringLiteral("frames/frame_%1.tif").arg(index, 8, 10, QLatin1Char('0')));
}

QByteArray hashFile(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Could not hash fixture.");
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

QString makeSequence(const QString& root, qint64 frameCount,
                     const QVector<qint64>& validFrames,
                     const QVector<qint64>& corruptFrames = {}) {
    require(QDir().mkpath(QDir(root).filePath("frames")), "Could not create frames.");
    for (const qint64 index : validFrames) {
        QImage image(8, 8, QImage::Format_Grayscale8);
        image.fill(static_cast<int>(index * 20));
        require(image.save(framePath(root, index), "TIFF"), "Could not write TIFF fixture.");
    }
    for (const qint64 index : corruptFrames) {
        QFile file(framePath(root, index));
        require(file.open(QIODevice::WriteOnly) && file.write("not a tiff") == 10,
                "Could not write corrupt fixture.");
    }
    sequence::SequenceManifestData data{
        QStringLiteral("sequence-id"),
        QStringLiteral("Sequence"),
        QString(),
        QString(),
        QStringLiteral("completed"),
        QStringLiteral("2026-07-24T10:00:00Z"),
        QStringLiteral("2026-07-24T10:00:00Z"),
        QStringLiteral("2026-07-24T10:00:01Z"),
        std::nullopt,
        QStringLiteral("end_of_sequence"),
        QStringLiteral("2"),
        frameCount,
        QJsonObject{{QStringLiteral("fixture"), true}},
        8,
        8,
        8,
        20.0,
    };
    const QString path = QDir(root).filePath("sequence.json");
    QString error;
    require(sequence::SequenceManifestV2::save(path, data, &error),
            qPrintable(error));
    return path;
}

sequence_test::SequenceTestRequest request(const QString& sequence,
                                           const QString& output) {
    sequence_test::SequenceTestRequest value;
    value.sequenceJson = sequence;
    value.outputRoot = output;
    value.runName = QStringLiteral("Run");
    value.triggerMode = run::TriggerMode::EveryDroplet;
    value.hitBoundary = {4.0, run::HitSide::PositiveY, 8, 8};
    value.requestedProcessingFps = 20.0;
    value.opendssVersion = QStringLiteral("2");
    value.detectorSettings = {{QStringLiteral("fixed"), true}};
    value.cropSettings = {{QStringLiteral("size"), 64}};
    value.timingSettings = {{QStringLiteral("fixed"), true}};
    value.cameraSettings = {{QStringLiteral("source"), QStringLiteral("sequence")}};
    value.daqSettings = {{QStringLiteral("physical"), false}};
    return value;
}

run::RunManifestData loadRun(const QString& output, const QString& folder = "Run") {
    QString error;
    auto loaded = run::RunManifestV2::load(
        QDir(output).filePath(folder + QStringLiteral("/run_summary.json")), &error);
    require(loaded.has_value(), qPrintable(error));
    return loaded->data();
}

void noModelEveryDroplet() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create output.");
    const QString manifest = makeSequence(sequenceRoot, 5, {1, 2, 3, 4, 5});
    const QByteArray beforeManifest = hashFile(manifest);
    const QByteArray beforeFrame = hashFile(framePath(sequenceRoot, 1));

    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f), detection(true, false, 6.0f),
                        detection(false, false, 0.0f), detection(true, true, 4.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    QString error;
    require(service.run(request(manifest, output), &error), qPrintable(error));
    const auto data = loadRun(output);
    require(data.status == run::RunStatus::Completed && data.events.size() == 2,
            "Every Droplet Run did not complete with two events.");
    require(!data.model && !data.events.at(0).predictedClassId &&
                data.events.at(0).scores.isEmpty() &&
                !data.events.at(0).inferenceTimeMs,
            "No-model Run contains inference facts.");
    require(data.events.at(0).decision == run::Route::Hit &&
                data.events.at(0).observedRoute == run::Route::Hit &&
                data.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued,
            "Every Droplet decision or observed route is incorrect.");
    require(data.events.at(1).observedRoute == run::Route::Unresolved,
            "Exact-boundary route was not preserved as Unresolved.");
    require(QFileInfo(QDir(output).filePath("Run/crops/droplet_000001.png")).isFile() &&
                hashFile(manifest) == beforeManifest &&
                hashFile(framePath(sequenceRoot, 1)) == beforeFrame,
            "Run crops are missing or source Sequence changed.");
}

void classBasedModel() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create output.");
    const QString manifest = makeSequence(sequenceRoot, 4, {1, 2, 3, 4});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f), detection(false, false, 0.0f),
                        detection(true, true, 6.0f), detection(false, false, 0.0f)};
    auto calls = std::make_shared<int>(0);
    sequence_test::ModelProvider provider = [calls](QString*) {
        sequence_test::PreparedModel model;
        model.snapshot = {
            QStringLiteral("model-id"), QStringLiteral("Model"),
            QString(64, QLatin1Char('a')),
            {{QStringLiteral("c0"), QStringLiteral("Class 0")},
             {QStringLiteral("c1"), QStringLiteral("Class 1")},
             {QStringLiteral("c2"), QStringLiteral("Class 2")}}};
        model.classify = [calls](const cv::Mat&, QString*) {
            sequence_test::ModelInferenceResult result;
            result.scores = (*calls)++ == 0 ? QVector<double>{0.1, 0.8, 0.1}
                                             : QVector<double>{0.7, 0.2, 0.1};
            return std::optional(result);
        };
        return std::optional(model);
    };
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr, provider);
    auto value = request(manifest, output);
    value.triggerMode = run::TriggerMode::ClassBased;
    value.useActiveModel = true;
    value.hitClassId = QStringLiteral("c1");
    QString error;
    require(service.run(value, &error), qPrintable(error));
    const auto data = loadRun(output);
    require(data.model && data.model->classes.size() == 3 &&
                data.events.size() == 2 &&
                data.events.at(0).predictedClassId == QStringLiteral("c1") &&
                data.events.at(0).decision == run::Route::Hit &&
                data.events.at(0).observedRoute == run::Route::Waste &&
                data.events.at(1).predictedClassId == QStringLiteral("c0") &&
                data.events.at(1).decision == run::Route::Waste &&
                data.events.at(1).observedRoute == run::Route::Hit &&
                data.events.at(0).inferenceTimeMs &&
                *data.events.at(0).inferenceTimeMs >= 0.0,
            "Class-Based prediction, decision, route, or timing is incorrect.");
}

void skipsAndStops() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create output.");
    const QString manifest = makeSequence(sequenceRoot, 4, {1, 4}, {3});
    const QByteArray frameOne = hashFile(framePath(sequenceRoot, 1));
    FakeDetector detector;
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    QString error;
    require(service.run(request(manifest, output), &error), qPrintable(error));
    require(hashFile(framePath(sequenceRoot, 1)) == frameOne,
            "Skipped-frame processing changed its source.");

    QTemporaryDir stoppedTemp;
    const QString stoppedSequence = QDir(stoppedTemp.path()).filePath("sequence");
    const QString stoppedOutput = QDir(stoppedTemp.path()).filePath("runs");
    require(QDir().mkpath(stoppedOutput), "Could not create stop output.");
    const QString stoppedManifest = makeSequence(stoppedSequence, 3, {1, 2, 3});
    FakeDetector stoppingDetector;
    OperationCoordinator stoppingOperations;
    sequence_test::SequenceTestService stoppingService(stoppingOperations,
                                                       stoppingDetector, nullptr);
    stoppingDetector.onProcess = [&](int index) {
        if (index == 0)
            stoppingService.requestStop();
    };
    require(stoppingService.run(request(stoppedManifest, stoppedOutput), &error),
            qPrintable(error));
    require(loadRun(stoppedOutput).status == run::RunStatus::Interrupted &&
                !stoppingOperations.snapshot().kind,
            "Stop did not finalize interrupted or release the lock.");
}

void rejectsUnsafeRequests() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create output.");
    const QString manifest = makeSequence(sequenceRoot, 1, {1});
    FakeDetector detector;
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    QString error;
    auto physical = request(manifest, output);
    physical.physicalDaqOutputEnabled = true;
    require(!service.run(physical, &error) && !operations.snapshot().kind,
            "Physical DAQ request was not rejected before lock acquisition.");
    auto classBased = request(manifest, output);
    classBased.triggerMode = run::TriggerMode::ClassBased;
    classBased.hitClassId = QStringLiteral("c0");
    require(!service.run(classBased, &error) && !operations.snapshot().kind,
            "Class-Based request without Active Model was accepted.");

    QTemporaryDir failedTemp;
    const QString failedSequence = QDir(failedTemp.path()).filePath("sequence");
    const QString failedOutput = QDir(failedTemp.path()).filePath("runs");
    require(QDir().mkpath(failedOutput), "Could not create failure output.");
    const QString failedManifest = makeSequence(failedSequence, 1, {}, {1});
    require(!service.run(request(failedManifest, failedOutput), &error) &&
                !operations.snapshot().kind &&
                QFileInfo(QDir(failedOutput)
                              .filePath("Run/run_summary.json"))
                    .isFile(),
            "Processing failure did not preserve a failed Run and release its lock.");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    noModelEveryDroplet();
    classBasedModel();
    skipsAndStops();
    rejectsUnsafeRequests();
    return 0;
}
