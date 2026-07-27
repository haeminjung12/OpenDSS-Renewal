#include "../detection/droplet_detector.h"
#include "../v2/model/model_load_service.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/run/run_manifest_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence_test/sequence_test_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <cmath>
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
QString sequenceTestRequestedDevice;

ModelLoadService::ModelLoadService(QString registryFilePath)
    : registryFilePath_(std::move(registryFilePath)) {}

std::unique_ptr<OnnxInferenceAdapter>
ModelLoadService::preparePersistedActive(const QString& requestedDevice,
                                         QString*, QString* error) const {
    sequenceTestRequestedDevice = requestedDevice;
    if (error)
        *error = QStringLiteral("No production model in this deterministic test.");
    return {};
}
} // namespace desktop_app::v2

namespace {

QString* suppressedDaqWarning = nullptr;

void captureSuppressedDaqWarning(QtMsgType type, const QMessageLogContext&,
                                 const QString& message) {
    if (type == QtWarningMsg && suppressedDaqWarning &&
        message.contains(QStringLiteral("DAQ Hit output suppressed"))) {
        *suppressedDaqWarning = message;
    }
}

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

QByteArray fileBytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Could not read fixture.");
    return file.readAll();
}

QString makeSequence(const QString& root, qint64 frameCount,
                     const QVector<qint64>& validFrames) {
    require(QDir().mkpath(QDir(root).filePath("frames")), "Could not create frames.");
    for (const qint64 index : validFrames) {
        QImage image(8, 8, QImage::Format_Grayscale8);
        image.fill(static_cast<int>(index * 20));
        require(image.save(framePath(root, index), "TIFF"), "Could not write TIFF fixture.");
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

std::shared_ptr<const sequence_test::LoadedSequence>
loadSequenceBuffer(const QString& sequenceJson) {
    QString error;
    auto manifest = sequence::SequenceManifestV2::load(sequenceJson, &error);
    require(manifest.has_value(), qPrintable(error));
    auto loaded = std::make_shared<sequence_test::LoadedSequence>();
    loaded->sourceSequenceJson = QFileInfo(sequenceJson).canonicalFilePath();
    loaded->sequenceId = manifest->data().sequenceId;
    loaded->frames.reserve(static_cast<qsizetype>(manifest->data().frameCount));
    const QString root = QFileInfo(sequenceJson).absolutePath();
    for (qint64 index = 1; index <= manifest->data().frameCount; ++index) {
        QImage image(framePath(root, index));
        require(!image.isNull(), "Could not decode loaded Sequence fixture.");
        loaded->frames.push_back({index, std::move(image)});
    }
    return loaded;
}

sequence_test::SequenceTestRequest request(const QString& sequence,
                                           const QString& output) {
    sequence_test::SequenceTestRequest value;
    value.sequenceJson = sequence;
    if (QFileInfo(sequence).isFile()) {
        value.frozenManifestBytes = fileBytes(sequence);
        value.loadedSequence = loadSequenceBuffer(sequence);
    }
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

void productionModelPreparationRequestsCpu() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create CPU-policy output.");
    const QString manifest = makeSequence(sequenceRoot, 1, {1});
    FakeDetector detector;
    OperationCoordinator operations;
    ModelLoadService loader{QString()};
    sequence_test::SequenceTestService service(operations, detector, &loader);
    auto value = request(manifest, output);
    value.triggerMode = run::TriggerMode::ClassBased;
    value.useActiveModel = true;
    value.hitClassId = QStringLiteral("c0");
    sequenceTestRequestedDevice.clear();
    QString error;

    require(!service.run(value, &error) &&
                sequenceTestRequestedDevice == QStringLiteral("cpu"),
            "Production Sequence Test did not explicitly request CPU inference.");
}

void daqOffRequiresNoReadinessOrOwnership() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create DAQ-off output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    auto heldDaq = operations.acquireMomentary(ResourceLock::Daq);
    require(heldDaq.acquired(), "Could not hold DAQ for the OFF test.");
    int readinessCalls = 0;
    int pulseCalls = 0;
    sequence_test::SequenceTestService service(
        operations, detector, nullptr, {},
        [&](bool, QString*) {
            ++pulseCalls;
            return run::DaqPulseStatus::Failed;
        },
        [&](QString* error) {
            ++readinessCalls;
            if (error)
                *error = QStringLiteral("Injected unavailable DAQ.");
            return false;
        });
    QString error;
    require(service.run(request(manifest, output), &error), qPrintable(error));
    const auto data = loadRun(output);
    require(readinessCalls == 0 && pulseCalls == 0 &&
                !data.routing.physicalDaqOutputEnabled &&
                data.events.size() == 1 &&
                data.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued,
            "DAQ OFF checked readiness, owned DAQ, issued output, or persisted false status.");
    heldDaq.lease.release();
}

void daqOnPreflightAndLockConflict() {
    QTemporaryDir unavailableTemporary;
    const QString unavailableSequence =
        QDir(unavailableTemporary.path()).filePath("sequence");
    const QString unavailableOutput =
        QDir(unavailableTemporary.path()).filePath("runs");
    require(QDir().mkpath(unavailableOutput), "Could not create unavailable output.");
    const QString unavailableManifest =
        makeSequence(unavailableSequence, 1, {1});
    FakeDetector unavailableDetector;
    OperationCoordinator unavailableOperations;
    int readinessCalls = 0;
    int pulseCalls = 0;
    sequence_test::SequenceTestService unavailableService(
        unavailableOperations, unavailableDetector, nullptr, {},
        [&](bool, QString*) {
            ++pulseCalls;
            return run::DaqPulseStatus::Issued;
        },
        [&](QString* error) {
            ++readinessCalls;
            if (error)
                *error = QStringLiteral("Injected unavailable DAQ.");
            return false;
        });
    auto unavailableRequest = request(unavailableManifest, unavailableOutput);
    unavailableRequest.physicalDaqOutputEnabled = true;
    QString error;
    require(!unavailableService.run(unavailableRequest, &error) &&
                error.contains(QStringLiteral("Injected unavailable DAQ")) &&
                readinessCalls == 1 && pulseCalls == 0 &&
                !unavailableOperations.snapshot().kind &&
                !QFileInfo(QDir(unavailableOutput).filePath("Run")).exists(),
            "DAQ ON did not reject unavailable hardware before processing.");

    QTemporaryDir conflictTemporary;
    const QString conflictSequence =
        QDir(conflictTemporary.path()).filePath("sequence");
    const QString conflictOutput = QDir(conflictTemporary.path()).filePath("runs");
    require(QDir().mkpath(conflictOutput), "Could not create conflict output.");
    const QString conflictManifest = makeSequence(conflictSequence, 1, {1});
    FakeDetector conflictDetector;
    OperationCoordinator conflictOperations;
    auto heldDaq = conflictOperations.acquireMomentary(ResourceLock::Daq);
    require(heldDaq.acquired(), "Could not hold DAQ for the conflict test.");
    readinessCalls = 0;
    sequence_test::SequenceTestService conflictService(
        conflictOperations, conflictDetector, nullptr, {},
        [](bool, QString*) { return run::DaqPulseStatus::Issued; },
        [&](QString*) {
            ++readinessCalls;
            return true;
        });
    auto conflictRequest = request(conflictManifest, conflictOutput);
    conflictRequest.physicalDaqOutputEnabled = true;
    require(!conflictService.run(conflictRequest, &error) &&
                error.contains(QStringLiteral("resource"), Qt::CaseInsensitive) &&
                readinessCalls == 0 &&
                !QFileInfo(QDir(conflictOutput).filePath("Run")).exists(),
            "DAQ lock conflict did not block before readiness and processing.");
    heldDaq.lease.release();
}

void daqHitMissAndSuppressionStatuses() {
    QTemporaryDir issuedTemporary;
    const QString issuedSequence = QDir(issuedTemporary.path()).filePath("sequence");
    const QString issuedOutput = QDir(issuedTemporary.path()).filePath("runs");
    require(QDir().mkpath(issuedOutput), "Could not create issued output.");
    const QString issuedManifest = makeSequence(issuedSequence, 2, {1, 2});
    FakeDetector issuedDetector;
    issuedDetector.results = {detection(true, true, 2.0f),
                              detection(false, false, 0.0f)};
    OperationCoordinator issuedOperations;
    int issuedCalls = 0;
    sequence_test::SequenceTestService issuedService(
        issuedOperations, issuedDetector, nullptr, {},
        [&](bool enabled, QString*) {
            ++issuedCalls;
            return enabled ? run::DaqPulseStatus::Issued
                           : run::DaqPulseStatus::Failed;
        },
        [](QString*) { return true; });
    auto issuedRequest = request(issuedManifest, issuedOutput);
    issuedRequest.physicalDaqOutputEnabled = true;
    QString error;
    require(issuedService.run(issuedRequest, &error), qPrintable(error));
    const auto issuedData = loadRun(issuedOutput);
    require(issuedCalls == 1 && issuedData.routing.physicalDaqOutputEnabled &&
                issuedData.events.size() == 1 &&
                issuedData.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::Issued,
            "A Hit did not issue exactly once or persist Issued.");

    QTemporaryDir missTemporary;
    const QString missSequence = QDir(missTemporary.path()).filePath("sequence");
    const QString missOutput = QDir(missTemporary.path()).filePath("runs");
    require(QDir().mkpath(missOutput), "Could not create miss output.");
    const QString missManifest = makeSequence(missSequence, 2, {1, 2});
    FakeDetector missDetector;
    missDetector.results = {detection(true, true, 2.0f),
                            detection(false, false, 0.0f)};
    sequence_test::ModelProvider missModel = [](QString*) {
        sequence_test::PreparedModel model;
        model.snapshot = {
            QStringLiteral("model-id"), QStringLiteral("Model"),
            QString(64, QLatin1Char('b')),
            {{QStringLiteral("c0"), QStringLiteral("Class 0")},
             {QStringLiteral("c1"), QStringLiteral("Class 1")}}};
        model.classify = [](const cv::Mat&, QString*) {
            return std::optional<sequence_test::ModelInferenceResult>{
                sequence_test::ModelInferenceResult{{0.9, 0.1}}};
        };
        return std::optional(model);
    };
    OperationCoordinator missOperations;
    int missPulseCalls = 0;
    sequence_test::SequenceTestService missService(
        missOperations, missDetector, nullptr, missModel,
        [&](bool, QString*) {
            ++missPulseCalls;
            return run::DaqPulseStatus::Issued;
        },
        [](QString*) { return true; });
    auto missRequest = request(missManifest, missOutput);
    missRequest.triggerMode = run::TriggerMode::ClassBased;
    missRequest.useActiveModel = true;
    missRequest.hitClassId = QStringLiteral("c1");
    missRequest.physicalDaqOutputEnabled = true;
    require(missService.run(missRequest, &error), qPrintable(error));
    const auto missData = loadRun(missOutput);
    require(missPulseCalls == 0 && missData.events.size() == 1 &&
                missData.events.at(0).decision == run::Route::Waste &&
                missData.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::NotRequested,
            "A Waste decision issued DAQ output or persisted the wrong status.");

    QTemporaryDir suppressedTemporary;
    const QString suppressedSequence =
        QDir(suppressedTemporary.path()).filePath("sequence");
    const QString suppressedOutput =
        QDir(suppressedTemporary.path()).filePath("runs");
    require(QDir().mkpath(suppressedOutput), "Could not create suppressed output.");
    const QString suppressedManifest =
        makeSequence(suppressedSequence, 2, {1, 2});
    FakeDetector suppressedDetector;
    suppressedDetector.results = {detection(true, true, 2.0f),
                                  detection(false, false, 0.0f)};
    OperationCoordinator suppressedOperations;
    int suppressedCalls = 0;
    sequence_test::SequenceTestService suppressedService(
        suppressedOperations, suppressedDetector, nullptr, {},
        [&](bool, QString* diagnostic) {
            ++suppressedCalls;
            if (diagnostic)
                *diagnostic = QStringLiteral("Injected output interlock.");
            return run::DaqPulseStatus::SuppressedNotIssued;
        },
        [](QString*) { return true; });
    auto suppressedRequest = request(suppressedManifest, suppressedOutput);
    suppressedRequest.physicalDaqOutputEnabled = true;
    QString capturedWarning;
    suppressedDaqWarning = &capturedWarning;
    const QtMessageHandler previousHandler =
        qInstallMessageHandler(captureSuppressedDaqWarning);
    const bool suppressedResult =
        suppressedService.run(suppressedRequest, &error);
    qInstallMessageHandler(previousHandler);
    suppressedDaqWarning = nullptr;
    const auto suppressedData = loadRun(suppressedOutput);
    require(suppressedResult && suppressedCalls == 1 &&
                capturedWarning.contains(QStringLiteral("Injected output interlock")) &&
                suppressedData.events.size() == 1 &&
                suppressedData.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued,
            "Callback suppression did not persist status and surface its reason.");
}

void daqFailureStopsTruthfully() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create DAQ-failure output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    int pulseCalls = 0;
    sequence_test::SequenceTestService service(
        operations, detector, nullptr, {},
        [&](bool, QString* diagnostic) {
            ++pulseCalls;
            if (diagnostic)
                *diagnostic = QStringLiteral("Injected DAQ fire failure.");
            return run::DaqPulseStatus::Failed;
        },
        [](QString*) { return true; });
    auto value = request(manifest, output);
    value.physicalDaqOutputEnabled = true;
    QString error;
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("Injected DAQ fire failure")) &&
                pulseCalls == 1 && !operations.snapshot().kind,
            "DAQ failure did not stop safely with the callback diagnostic.");
    const auto data = loadRun(output);
    require(data.status == run::RunStatus::Failed &&
                data.stopReason == QStringLiteral("daq_pulse_failed") &&
                data.events.size() == 1 &&
                data.events.at(0).daqPulseStatus == run::DaqPulseStatus::Failed,
            "DAQ failure did not persist a failed event and factual Run summary.");
    auto releasedDaq = operations.acquireMomentary(ResourceLock::Daq);
    require(releasedDaq.acquired(), "DAQ failure retained the DAQ lease.");
    releasedDaq.lease.release();
}

void daqStopPreventsOutputAndReleases() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create DAQ-stop output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    std::mutex mutex;
    std::condition_variable enteredCondition;
    std::condition_variable releaseCondition;
    bool entered = false;
    bool release = false;
    detector.onProcess = [&](int) {
        std::unique_lock lock(mutex);
        entered = true;
        enteredCondition.notify_one();
        releaseCondition.wait(lock, [&] { return release; });
    };
    int pulseCalls = 0;
    sequence_test::SequenceTestService service(
        operations, detector, nullptr, {},
        [&](bool, QString*) {
            ++pulseCalls;
            return run::DaqPulseStatus::Issued;
        },
        [](QString*) { return true; });
    auto value = request(manifest, output);
    value.physicalDaqOutputEnabled = true;
    QVector<sequence_test::SequenceTestProgress> progress;
    value.progressCallback =
        [&](const sequence_test::SequenceTestProgress& update) {
            progress.push_back(update);
        };
    bool result = false;
    QString error;
    std::thread worker([&] { result = service.run(value, &error); });
    {
        std::unique_lock lock(mutex);
        enteredCondition.wait(lock, [&] { return entered; });
    }
    const auto active = operations.snapshot();
    require(active.kind == OperationKind::SequenceTest &&
                active.locks.testFlag(ResourceLock::Daq),
            "Physical Sequence Test did not hold the DAQ lease while active.");
    service.requestStop();
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    releaseCondition.notify_one();
    worker.join();
    const auto stoppedRun = loadRun(output);
    require(result && pulseCalls == 0 &&
                progress.isEmpty() &&
                stoppedRun.status == run::RunStatus::Interrupted &&
                stoppedRun.achievedProcessingFps == 0.0 &&
                !operations.snapshot().kind,
            "Stop after detection counted a partial frame, allowed output, or retained the operation.");
    auto releasedDaq = operations.acquireMomentary(ResourceLock::Daq);
    require(releasedDaq.acquired(), "Stop retained the DAQ lease.");
    releasedDaq.lease.release();
}

void stopDuringStartupIsNotCleared() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create startup-stop output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(false, false, 0.0f)};
    OperationCoordinator operations;
    std::mutex mutex;
    std::condition_variable gateEnteredCondition;
    std::condition_variable releaseGateCondition;
    bool gateEntered = false;
    bool releaseGate = false;
    int pulseCalls = 0;
    sequence_test::SequenceTestService service(
        operations, detector, nullptr, {},
        [&](bool, QString*) {
            ++pulseCalls;
            return run::DaqPulseStatus::Issued;
        },
        [&](QString*) {
            std::unique_lock lock(mutex);
            gateEntered = true;
            gateEnteredCondition.notify_one();
            releaseGateCondition.wait(lock, [&] { return releaseGate; });
            return true;
        });
    auto value = request(manifest, output);
    value.physicalDaqOutputEnabled = true;
    bool result = false;
    QString error;
    std::thread worker([&] { result = service.run(value, &error); });
    {
        std::unique_lock lock(mutex);
        gateEnteredCondition.wait(lock, [&] { return gateEntered; });
    }
    service.requestStop();
    {
        std::lock_guard lock(mutex);
        releaseGate = true;
    }
    releaseGateCondition.notify_one();
    worker.join();
    require(result && pulseCalls == 0 &&
                loadRun(output).status == run::RunStatus::Interrupted &&
                !operations.snapshot().kind,
            "A stop accepted during startup was cleared or allowed DAQ output.");
}

void stopBeforePulseDispatchPreventsCallback() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create pre-dispatch-stop output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    FakeDetector detector;
    detector.results = {detection(true, true, 2.0f),
                        detection(false, false, 0.0f)};
    std::mutex mutex;
    std::condition_variable classifyEnteredCondition;
    std::condition_variable releaseClassifyCondition;
    bool classifyEntered = false;
    bool releaseClassify = false;
    sequence_test::ModelProvider modelProvider = [&](QString*) {
        sequence_test::PreparedModel model;
        model.snapshot = {
            QStringLiteral("model-id"), QStringLiteral("Model"),
            QString(64, QLatin1Char('c')),
            {{QStringLiteral("c0"), QStringLiteral("Class 0")},
             {QStringLiteral("c1"), QStringLiteral("Class 1")}}};
        model.classify = [&](const cv::Mat&, QString*) {
            std::unique_lock lock(mutex);
            classifyEntered = true;
            classifyEnteredCondition.notify_one();
            releaseClassifyCondition.wait(lock, [&] { return releaseClassify; });
            return std::optional<sequence_test::ModelInferenceResult>{
                sequence_test::ModelInferenceResult{{0.1, 0.9}}};
        };
        return std::optional(model);
    };
    OperationCoordinator operations;
    int pulseCalls = 0;
    sequence_test::SequenceTestService service(
        operations, detector, nullptr, modelProvider,
        [&](bool, QString*) {
            ++pulseCalls;
            return run::DaqPulseStatus::Issued;
        },
        [](QString*) { return true; });
    auto value = request(manifest, output);
    value.triggerMode = run::TriggerMode::ClassBased;
    value.useActiveModel = true;
    value.hitClassId = QStringLiteral("c1");
    value.physicalDaqOutputEnabled = true;
    bool result = false;
    QString error;
    std::thread worker([&] { result = service.run(value, &error); });
    {
        std::unique_lock lock(mutex);
        classifyEnteredCondition.wait(lock, [&] { return classifyEntered; });
    }
    service.requestStop();
    {
        std::lock_guard lock(mutex);
        releaseClassify = true;
    }
    releaseClassifyCondition.notify_one();
    worker.join();
    const auto data = loadRun(output);
    require(result && pulseCalls == 0 &&
                data.status == run::RunStatus::Interrupted &&
                data.events.size() == 1 &&
                data.events.at(0).decision == run::Route::Hit &&
                data.events.at(0).daqPulseStatus ==
                    run::DaqPulseStatus::SuppressedNotIssued &&
                !operations.snapshot().kind,
            "A stop accepted before pulse dispatch allowed a later callback.");
}

void loadedBufferValidationAndNoDiskReread() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});
    auto value = request(manifest, output);
    require(QFile::remove(framePath(sequenceRoot, 1)) &&
                QFile::remove(framePath(sequenceRoot, 2)),
            "Could not remove decoded source frames.");
    FakeDetector detector;
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    QString error;
    const auto validLoaded = value.loadedSequence;
    require(service.run(value, &error) &&
                loadRun(output).status == run::RunStatus::Completed,
            "A valid loaded buffer reread deleted frame files.");

    value.loadedSequence.reset();
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("loaded to memory")),
            "Sequence Test accepted a request without a loaded buffer.");

    auto countMismatch =
        std::make_shared<sequence_test::LoadedSequence>(*validLoaded);
    countMismatch->frames.removeLast();
    value.loadedSequence = countMismatch;
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("frame count")),
            "Loaded Sequence frame-count mismatch was accepted.");

    auto orderMismatch =
        std::make_shared<sequence_test::LoadedSequence>(*validLoaded);
    orderMismatch->frames[0].sourceFrameIndex = 2;
    value.loadedSequence = orderMismatch;
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("frame order")),
            "Loaded Sequence frame-order mismatch was accepted.");

    auto dimensionMismatch =
        std::make_shared<sequence_test::LoadedSequence>(*orderMismatch);
    dimensionMismatch->frames[0].sourceFrameIndex = 1;
    dimensionMismatch->frames[0].image =
        QImage(7, 8, QImage::Format_Grayscale8);
    value.loadedSequence = dimensionMismatch;
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("frame dimensions")),
            "Loaded Sequence frame-dimension mismatch was accepted.");

    auto sourceMismatch =
        std::make_shared<sequence_test::LoadedSequence>(*orderMismatch);
    sourceMismatch->frames[0].sourceFrameIndex = 1;
    sourceMismatch->sourceSequenceJson = output;
    value.loadedSequence = sourceMismatch;
    require(!service.run(value, &error) &&
                error.contains(QStringLiteral("source")),
            "Loaded Sequence source mismatch was accepted.");
}

void frozenManifestSurvivesCompatiblePathMutation() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create frozen-manifest output.");
    const QString manifest = makeSequence(sequenceRoot, 2, {1, 2});

    QString error;
    const auto accepted =
        sequence::SequenceManifestV2::load(manifest, &error);
    require(accepted.has_value(), qPrintable(error));
    auto value = request(manifest, output);
    value.experimentType = accepted->data().experimentType;
    value.notes = accepted->data().notes;
    value.cameraSettings = accepted->data().cameraSettings;
    value.requestedProcessingFps = accepted->data().nominalFps;

    auto mutated = accepted->data();
    mutated.name = QStringLiteral("Mutated Live Name");
    mutated.experimentType = QStringLiteral("mutated-experiment");
    mutated.notes = QStringLiteral("mutated-notes");
    mutated.cameraSettings = {{QStringLiteral("mutated"), true}};
    mutated.nominalFps = 1.0;
    require(sequence::SequenceManifestV2::save(manifest, mutated, &error),
            qPrintable(error));
    require(fileBytes(manifest) != value.frozenManifestBytes,
            "Compatible mutation did not change live manifest bytes.");

    FakeDetector detector;
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    require(service.run(value, &error), qPrintable(error));

    const QString archivedPath =
        QDir(output).filePath(QStringLiteral("Run/source/sequence.json"));
    const auto data = loadRun(output);
    const auto archived =
        sequence::SequenceManifestV2::load(archivedPath, &error);
    require(archived.has_value(), qPrintable(error));
    require(data.status == run::RunStatus::Completed &&
                data.sourceSequence.name == QStringLiteral("Sequence") &&
                data.experimentType == accepted->data().experimentType &&
                data.notes == accepted->data().notes &&
                data.cameraSettings == accepted->data().cameraSettings &&
                data.requestedProcessingFps ==
                    accepted->data().nominalFps &&
                archived->data().name == QStringLiteral("Sequence") &&
                archived->data().nominalFps ==
                    accepted->data().nominalFps &&
                archived->data().cameraSettings ==
                    accepted->data().cameraSettings &&
                fileBytes(archivedPath) == value.frozenManifestBytes,
            "Live manifest mutation changed processing facts or archived provenance.");
}

void progressReportsTruthAndSurvivesObserver() {
    QTemporaryDir temporary;
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "Could not create progress output.");
    const QString manifest = makeSequence(sequenceRoot, 4, {1, 2, 3, 4});
    FakeDetector detector;
    OperationCoordinator operations;
    sequence_test::SequenceTestService service(operations, detector, nullptr);
    QVector<sequence_test::SequenceTestProgress> progress;
    auto value = request(manifest, output);
    value.requestedProcessingFps = 20.0;
    value.progressCallback = [&](const sequence_test::SequenceTestProgress& update) {
        progress.push_back(update);
        if (update.processedFrames == 2)
            throw std::runtime_error("observer disappeared");
    };
    QString error;
    QElapsedTimer wallClock;
    wallClock.start();
    require(service.run(value, &error), qPrintable(error));
    const qint64 wallMilliseconds = wallClock.elapsed();
    require(progress.size() == 4, "Progress observer failure corrupted the Run.");
    for (qsizetype index = 0; index < progress.size(); ++index) {
        require(progress.at(index).processedFrames == index + 1 &&
                    progress.at(index).totalFrames == 4 &&
                    std::isfinite(progress.at(index).elapsedSeconds) &&
                    progress.at(index).elapsedSeconds >= 0.0 &&
                    std::isfinite(progress.at(index).achievedProcessingFps) &&
                    progress.at(index).achievedProcessingFps >= 0.0 &&
                    (index == 0 ||
                     progress.at(index).elapsedSeconds >=
                         progress.at(index - 1).elapsedSeconds),
                "Progress facts are not monotonic or factual.");
    }
    const auto completedRun = loadRun(output);
    require(progress.back().processedFrames == progress.back().totalFrames &&
                wallMilliseconds >= 160 && wallMilliseconds < 1000 &&
                completedRun.status == run::RunStatus::Completed &&
                completedRun.achievedProcessingFps ==
                    progress.back().achievedProcessingFps,
            "Requested FPS pacing or final completed-frame progress was not factual.");

    QTemporaryDir stopTemporary;
    const QString stopRoot = QDir(stopTemporary.path()).filePath("sequence");
    const QString stopOutput = QDir(stopTemporary.path()).filePath("runs");
    require(QDir().mkpath(stopOutput), "Could not create progress-stop output.");
    const QString stopManifest = makeSequence(stopRoot, 4, {1, 2, 3, 4});
    FakeDetector stopDetector;
    OperationCoordinator stopOperations;
    sequence_test::SequenceTestService stopService(
        stopOperations, stopDetector, nullptr);
    QVector<sequence_test::SequenceTestProgress> stoppedProgress;
    std::mutex stopMutex;
    std::condition_variable firstProgressCondition;
    bool firstProgressReported = false;
    auto stopValue = request(stopManifest, stopOutput);
    stopValue.requestedProcessingFps = 2.0;
    stopValue.progressCallback =
        [&](const sequence_test::SequenceTestProgress& update) {
            stoppedProgress.push_back(update);
            {
                std::lock_guard lock(stopMutex);
                firstProgressReported = true;
            }
            firstProgressCondition.notify_one();
        };
    bool stoppedResult = false;
    QString stoppedError;
    std::thread stoppedWorker(
        [&] { stoppedResult = stopService.run(stopValue, &stoppedError); });
    {
        std::unique_lock lock(stopMutex);
        firstProgressCondition.wait(lock, [&] { return firstProgressReported; });
    }
    QElapsedTimer stopClock;
    stopClock.start();
    stopService.requestStop();
    stoppedWorker.join();
    const qint64 stopMilliseconds = stopClock.elapsed();
    const auto interruptedRun = loadRun(stopOutput);
    require(stoppedResult && stoppedProgress.size() == 1 &&
                stoppedProgress.back().processedFrames == 1 &&
                stoppedProgress.back().totalFrames == 4 &&
                stopMilliseconds < 300 &&
                interruptedRun.status == run::RunStatus::Interrupted &&
                interruptedRun.achievedProcessingFps ==
                    stoppedProgress.back().achievedProcessingFps &&
                !stopOperations.snapshot().kind,
            "Stop did not promptly interrupt the scheduled wait at the reported frame.");
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
    const QString detectorManifest = makeSequence(detectorSequence, 2, {1, 2});
    FakeDetector throwingDetector;
    throwingDetector.results = {DropletDetectionFrame{}, DropletDetectionFrame{}};
    throwingDetector.onProcess = [](int index) {
        if (index == 1)
            throw std::runtime_error("detector boom");
    };
    OperationCoordinator detectorOperations;
    sequence_test::SequenceTestService detectorService(
        detectorOperations, throwingDetector, nullptr);
    QVector<sequence_test::SequenceTestProgress> failureProgress;
    auto detectorRequest = request(detectorManifest, detectorOutput);
    detectorRequest.progressCallback =
        [&](const sequence_test::SequenceTestProgress& update) {
            failureProgress.push_back(update);
        };
    require(!detectorService.run(detectorRequest, &error) &&
                error.contains(QStringLiteral("detector boom")) &&
                failureProgress.size() == 1 &&
                failureProgress.front().processedFrames == 1 &&
                failureProgress.front().totalFrames == 2 &&
                loadRun(detectorOutput).status == run::RunStatus::Failed &&
                loadRun(detectorOutput).stopReason ==
                    QStringLiteral("processing_exception") &&
                loadRun(detectorOutput).achievedProcessingFps ==
                    failureProgress.front().achievedProcessingFps &&
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
    productionModelPreparationRequestsCpu();
    daqOffRequiresNoReadinessOrOwnership();
    daqOnPreflightAndLockConflict();
    daqHitMissAndSuppressionStatuses();
    daqFailureStopsTruthfully();
    daqStopPreventsOutputAndReleases();
    stopDuringStartupIsNotCleared();
    stopBeforePulseDispatchPreventsCallback();
    loadedBufferValidationAndNoDiskReread();
    frozenManifestSurvivesCompatiblePathMutation();
    progressReportsTruthAndSurvivesObserver();
    stopAndSingleRunGuard();
    rejectsUnsafeRequests();
    exceptionRecovery();
    return 0;
}
