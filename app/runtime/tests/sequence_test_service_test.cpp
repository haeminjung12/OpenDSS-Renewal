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
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
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

void artifactDamageFails() {
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
    require(!service.run(request(manifest, output), &error) &&
                error.contains(QStringLiteral("frame 2")) &&
                loadRun(output).status == run::RunStatus::Failed &&
                loadRun(output).stopReason == QStringLiteral("missing_frame_2") &&
                QFileInfo(QDir(output).filePath("Run/events.partial.csv")).isFile() &&
                QFileInfo(QDir(output)
                              .filePath("Run/run_summary.partial.json"))
                    .isFile() &&
                hashFile(framePath(sequenceRoot, 1)) == frameOne &&
                !operations.snapshot().kind,
            "Missing frame did not preserve a factual failed Run.");

    QTemporaryDir corruptTemporary;
    const QString corruptRoot =
        QDir(corruptTemporary.path()).filePath("sequence");
    const QString corruptOutput = QDir(corruptTemporary.path()).filePath("runs");
    require(QDir().mkpath(corruptOutput), "Could not create corrupt output.");
    const QString corruptManifest = makeSequence(corruptRoot, 1, {}, {1});
    const QByteArray corruptHash = hashFile(framePath(corruptRoot, 1));
    OperationCoordinator corruptOperations;
    sequence_test::SequenceTestService corruptService(corruptOperations, detector, nullptr);
    require(!corruptService.run(request(corruptManifest, corruptOutput), &error) &&
                error.contains(QStringLiteral("frame 1")) &&
                loadRun(corruptOutput).status == run::RunStatus::Failed &&
                loadRun(corruptOutput).stopReason == QStringLiteral("corrupt_frame_1") &&
                hashFile(framePath(corruptRoot, 1)) == corruptHash &&
                !corruptOperations.snapshot().kind,
            "Corrupt frame did not preserve a factual failed Run.");
}

void stopAndSingleRunGuard() {
    QTemporaryDir stoppedTemp;
    const QString stoppedSequence = QDir(stoppedTemp.path()).filePath("sequence");
    const QString stoppedOutput = QDir(stoppedTemp.path()).filePath("runs");
    require(QDir().mkpath(stoppedOutput), "Could not create stop output.");
    const QString stoppedManifest = makeSequence(stoppedSequence, 3, {1, 2, 3});
    FakeDetector stoppingDetector;
    OperationCoordinator stoppingOperations;
    sequence_test::SequenceTestService stoppingService(stoppingOperations,
                                                       stoppingDetector, nullptr);
    std::mutex mutex;
    std::condition_variable enteredCondition;
    std::condition_variable releaseCondition;
    bool entered = false;
    bool release = false;
    stoppingDetector.onProcess = [&](int) {
        std::unique_lock lock(mutex);
        entered = true;
        enteredCondition.notify_one();
        releaseCondition.wait(lock, [&] { return release; });
    };
    bool firstResult = false;
    QString firstError;
    std::thread first([&] {
        firstResult =
            stoppingService.run(request(stoppedManifest, stoppedOutput), &firstError);
    });
    {
        std::unique_lock lock(mutex);
        enteredCondition.wait(lock, [&] { return entered; });
    }
    stoppingService.requestStop();
    QString secondError;
    require(!stoppingService.run(request(stoppedManifest, stoppedOutput), &secondError) &&
                secondError.contains(QStringLiteral("already running")),
            "Concurrent run was not rejected.");
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    releaseCondition.notify_one();
    first.join();
    require(firstResult, qPrintable(firstError));
    require(loadRun(stoppedOutput).status == run::RunStatus::Interrupted &&
                !stoppingOperations.snapshot().kind,
            "Concurrent rejection cleared a pending Stop or retained the lock.");
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

    auto storage = operations.acquireMomentary(ResourceLock::Storage);
    auto invalidSource = request(QDir(temporary.path()).filePath("missing.json"), output);
    require(storage.acquired() && !service.run(invalidSource, &error) &&
                error.contains(QStringLiteral("resource"), Qt::CaseInsensitive),
            "Storage was not acquired before reading the Sequence.");
    storage.lease.release();

    bool providerCalled = false;
    sequence_test::ModelProvider provider = [&](QString*) {
        providerCalled = true;
        return std::optional<sequence_test::PreparedModel>{};
    };
    sequence_test::SequenceTestService modeledService(operations, detector, nullptr,
                                                      provider);
    auto modelLock = operations.acquireMomentary(ResourceLock::Model);
    auto modeled = request(manifest, output);
    modeled.triggerMode = run::TriggerMode::ClassBased;
    modeled.useActiveModel = true;
    modeled.hitClassId = QStringLiteral("c0");
    require(modelLock.acquired() && !modeledService.run(modeled, &error) &&
                !providerCalled,
            "Model was prepared before acquiring its resource lock.");
    modelLock.lease.release();
}

void exceptionRecovery() {
    QTemporaryDir providerTemporary;
    const QString providerSequence =
        QDir(providerTemporary.path()).filePath("sequence");
    const QString providerOutput = QDir(providerTemporary.path()).filePath("runs");
    require(QDir().mkpath(providerOutput), "Could not create provider output.");
    const QString providerManifest = makeSequence(providerSequence, 1, {1});
    FakeDetector providerDetector;
    OperationCoordinator providerOperations;
    sequence_test::ModelProvider throwingProvider = [](QString*)
        -> std::optional<sequence_test::PreparedModel> {
        throw std::runtime_error("provider boom");
    };
    sequence_test::SequenceTestService providerService(
        providerOperations, providerDetector, nullptr, throwingProvider);
    auto modeled = request(providerManifest, providerOutput);
    modeled.triggerMode = run::TriggerMode::ClassBased;
    modeled.useActiveModel = true;
    modeled.hitClassId = QStringLiteral("c0");
    QString error;
    require(!providerService.run(modeled, &error) &&
                error.contains(QStringLiteral("provider boom")) &&
                !providerOperations.snapshot().kind &&
                !QFileInfo(QDir(providerOutput).filePath("Run")).exists(),
            "Provider exception escaped or retained resources.");

    QTemporaryDir detectorTemporary;
    const QString detectorSequence =
        QDir(detectorTemporary.path()).filePath("sequence");
    const QString detectorOutput = QDir(detectorTemporary.path()).filePath("runs");
    require(QDir().mkpath(detectorOutput), "Could not create detector output.");
    const QString detectorManifest = makeSequence(detectorSequence, 1, {1});
    FakeDetector throwingDetector;
    throwingDetector.onProcess = [](int) {
        throw std::runtime_error("detector boom");
    };
    OperationCoordinator detectorOperations;
    sequence_test::SequenceTestService detectorService(
        detectorOperations, throwingDetector, nullptr);
    require(!detectorService.run(request(detectorManifest, detectorOutput), &error) &&
                error.contains(QStringLiteral("detector boom")) &&
                loadRun(detectorOutput).status == run::RunStatus::Failed &&
                loadRun(detectorOutput).stopReason ==
                    QStringLiteral("processing_exception") &&
                QFileInfo(QDir(detectorOutput)
                              .filePath("Run/events.partial.csv"))
                    .isFile() &&
                QFileInfo(QDir(detectorOutput)
                              .filePath("Run/run_summary.partial.json"))
                    .isFile() &&
                !detectorOperations.snapshot().kind,
            "Detector exception did not preserve failed Run recovery.");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    noModelEveryDroplet();
    classBasedModel();
    artifactDamageFails();
    stopAndSingleRunGuard();
    rejectsUnsafeRequests();
    exceptionRecovery();
    return 0;
}
