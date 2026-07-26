#include "../detection/droplet_detector.h"
#include "../v2/model/model_load_service.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/run/run_manifest_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/sequence_test/sequence_test_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

using namespace desktop_app::v2;
using namespace desktop_app::v2::sequence_test;

namespace desktop_app::v2 {
std::unique_ptr<OnnxInferenceAdapter>
ModelLoadService::preparePersistedActive(const QString&,
                                         QString*,
                                         QString* error) const {
    if (error)
        *error = QStringLiteral("No production model in this deterministic test.");
    return {};
}
} // namespace desktop_app::v2

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return predicate();
}

QByteArray fileBytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read fixture bytes");
    return file.readAll();
}

QString framePath(const QString& root, qint64 index) {
    return QDir(root).filePath(
        QStringLiteral("frames/frame_%1.tif")
            .arg(index, 8, 10, QLatin1Char('0')));
}

QString makeSequence(const QString& root,
                     qint64 frameCount,
                     bool corruptSecond = false) {
    require(QDir().mkpath(QDir(root).filePath("frames")),
            "create Sequence frames folder");
    for (qint64 index = 1; index <= frameCount; ++index) {
        const QString path = framePath(root, index);
        if (corruptSecond && index == 2) {
            QFile corrupt(path);
            require(corrupt.open(QIODevice::WriteOnly), "open corrupt frame");
            require(corrupt.write("not-a-tiff") > 0, "write corrupt frame");
            continue;
        }
        QImage image(8, 8, QImage::Format_Grayscale8);
        image.fill(static_cast<int>(index * 20));
        require(image.save(path, "TIFF"), "save Sequence frame");
    }

    sequence::SequenceManifestData data;
    data.sequenceId = QStringLiteral("sequence-id");
    data.name = QStringLiteral("Sequence");
    data.experimentType = QStringLiteral("controller-test");
    data.notes = QStringLiteral("facts");
    data.status = QStringLiteral("completed");
    data.createdAt = QStringLiteral("2026-07-25T12:00:00Z");
    data.startedAt = QStringLiteral("2026-07-25T12:00:00Z");
    data.endedAt = QStringLiteral("2026-07-25T12:00:01Z");
    data.stopReason = QStringLiteral("end_of_sequence");
    data.opendssVersion = QStringLiteral("2");
    data.frameCount = frameCount;
    data.cameraSettings = {{QStringLiteral("fixture"), true}};
    data.imageWidth = 8;
    data.imageHeight = 8;
    data.bitDepth = 8;
    data.nominalFps = 20.0;
    const QString path = QDir(root).filePath(QStringLiteral("sequence.json"));
    QString error;
    require(sequence::SequenceManifestV2::save(path, data, &error),
            qPrintable(error));
    return path;
}

run::ModelSnapshot activeModelSnapshot() {
    return {QStringLiteral("active-model"),
            QStringLiteral("Active Model"),
            QString(64, QLatin1Char('a')),
            {{QStringLiteral("0"), QStringLiteral("Empty")},
             {QStringLiteral("1"), QStringLiteral("Single")},
             {QStringLiteral("2"), QStringLiteral("More Than One")}}};
}

class FakeDetector final : public IDropletDetector {
  public:
    std::function<void(int)> onProcess;

    void reset() override { index_ = 0; }
    int backgroundFramesRemaining() const override { return 0; }
    DropletDetectionFrame processFrame(const cv::Mat&) override {
        if (onProcess)
            onProcess(index_);
        ++index_;
        return {};
    }

  private:
    int index_ = 0;
};

PreparedModel preparedModel() {
    PreparedModel model;
    model.snapshot = activeModelSnapshot();
    model.classify = [](const cv::Mat&, QString*) {
        return std::optional<ModelInferenceResult>(
            ModelInferenceResult{{0.1, 0.8, 0.1}});
    };
    return model;
}

SequenceTestController makeController(
    SequenceTestService& service,
    const QString& output,
    qulonglong& availableMemory,
    int& resultsRefreshes,
    ActiveModelSnapshotProvider modelProvider = {},
    DaqReadinessGate daqReadinessProvider = {}) {
    return SequenceTestController(
        service,
        std::move(modelProvider),
        [&resultsRefreshes] { ++resultsRefreshes; },
        [output] { return output; },
        [&availableMemory] { return availableMemory; },
        std::move(daqReadinessProvider),
        {4.0, run::HitSide::PositiveY, 8, 8},
        {{QStringLiteral("fixed_detector"), true}},
        {{QStringLiteral("crop_size"), 64}},
        {{QStringLiteral("fixed_timing"), true}},
        QStringLiteral("2"));
}

QString onlyRunFolder(const QString& output) {
    const QStringList children =
        QDir(output).entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                               QDir::Name);
    require(children.size() == 1, "one direct Run child");
    return QDir(output).filePath(children.front());
}

void selectionLoadingAndImmutableBuffer() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create selection temporary directory");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "create selection output");

    FakeDetector detector;
    OperationCoordinator operations;
    SequenceTestService service(operations, detector, nullptr);
    qulonglong availableMemory = 128;
    int resultsRefreshes = 0;
    auto controller =
        makeController(service, output, availableMemory, resultsRefreshes);

    require(!controller.selectSequence(QUrl(QStringLiteral("https://example.test/sequence.json"))) &&
                controller.presentation() == QStringLiteral("error") &&
                !controller.memoryReady(),
            "reject non-local Sequence URL");

    const QString malformedRoot =
        QDir(temporary.path()).filePath("malformed");
    require(QDir().mkpath(malformedRoot), "create malformed folder");
    QFile malformed(QDir(malformedRoot).filePath("sequence.json"));
    require(malformed.open(QIODevice::WriteOnly), "open malformed manifest");
    require(malformed.write("{broken") > 0, "write malformed manifest");
    malformed.close();
    require(!controller.selectSequence(
                QUrl::fromLocalFile(malformed.fileName())),
            "reject malformed manifest");

    const QString missingRoot = QDir(temporary.path()).filePath("missing");
    const QString missingManifest = makeSequence(missingRoot, 2);
    require(QFile::remove(framePath(missingRoot, 2)),
            "remove referenced frame");
    require(controller.selectSequence(
                QUrl::fromLocalFile(missingManifest)),
            "selection does not inspect later frame references");
    require(controller.loadToMemory() &&
                waitUntil([&] {
                    return controller.loadStatus() ==
                           QStringLiteral("Error");
                }) &&
                !controller.memoryReady() &&
                controller.errorMessage().contains(
                    QStringLiteral("frame 2 could not be decoded")),
            "async load rejects missing later frame");

    const QString validRoot = QDir(temporary.path()).filePath("valid");
    const QString validManifest = makeSequence(validRoot, 3);
    require(controller.selectSequence(QUrl::fromLocalFile(validManifest)),
            "select valid Sequence");
    require(controller.sourceManifestUrl() ==
                    QUrl::fromLocalFile(QFileInfo(validManifest)
                                            .canonicalFilePath()) &&
                controller.sequenceName() == QStringLiteral("Sequence") &&
                controller.sequenceFolderUrl() ==
                    QUrl::fromLocalFile(QFileInfo(validManifest)
                                            .absolutePath()) &&
                controller.sequencePath() ==
                    QDir::cleanPath(QFileInfo(validManifest)
                                        .canonicalFilePath()) &&
                controller.frameCount() == 3 &&
                controller.recordedFps() == 20.0 &&
                controller.requestedProcessingFps() == 20.0 &&
                controller.previewUrl() ==
                    QUrl::fromLocalFile(QFileInfo(framePath(validRoot, 1))
                                            .canonicalFilePath()) &&
                controller.sequenceValidation() == QStringLiteral("Valid") &&
                controller.bufferBytes() == 192,
            "publish Sequence preview and facts");
    require(!controller.canLoadToMemory() &&
                !controller.loadToMemory() &&
                controller.errorMessage().contains(
                    QStringLiteral("requires 192 bytes")),
            "reject load above available-memory preflight");

    availableMemory = 1024 * 1024;
    controller.refreshPreflight();
    require(controller.canLoadToMemory() && controller.loadToMemory(),
            "start asynchronous memory load");
    require(waitUntil([&] {
                return controller.loadStatus() == QStringLiteral("Ready");
            }) &&
                controller.memoryReady() && controller.bufferBytes() == 192,
            "publish complete immutable memory load");

    controller.setTriggerEveryDroplet(true);
    const QByteArray acceptedManifestBytes = fileBytes(validManifest);
    QString provenanceError;
    const auto acceptedManifest =
        sequence::SequenceManifestV2::load(validManifest,
                                           &provenanceError);
    require(acceptedManifest.has_value(), qPrintable(provenanceError));
    auto compatibleMutation = acceptedManifest->data();
    compatibleMutation.name = QStringLiteral("Mutated Live Name");
    compatibleMutation.experimentType =
        QStringLiteral("mutated-experiment");
    compatibleMutation.notes = QStringLiteral("mutated-notes");
    compatibleMutation.cameraSettings = {
        {QStringLiteral("mutated"), true}};
    compatibleMutation.nominalFps = 1.0;
    require(sequence::SequenceManifestV2::save(
                validManifest, compatibleMutation, &provenanceError),
            qPrintable(provenanceError));
    require(fileBytes(validManifest) != acceptedManifestBytes,
            "compatible mutation did not change live manifest bytes");
    controller.refreshPreflight();
    require(controller.canStart(),
            "compatible live metadata mutation does not invalidate snapshot");

    require(QFile::remove(framePath(validRoot, 2)),
            "remove source frame after immutable load");
    require(controller.canStart() && controller.start(),
            "start from frozen snapshot after compatible live mutation");
    require(waitUntil([&] {
                return controller.presentation() ==
                       QStringLiteral("completed");
            }) &&
                controller.processedFrames() == 3 &&
                controller.totalFrames() == 3 &&
                controller.achievedProcessingFps() > 0.0 &&
                controller.outputStatus() == QStringLiteral("Completed") &&
                resultsRefreshes == 1,
            "complete immutable-buffer Run with progress and Results refresh");
    const QString runFolder = onlyRunFolder(output);
    const QString runSummaryPath =
        QDir(runFolder).filePath(QStringLiteral("run_summary.json"));
    const QString archivedManifestPath =
        QDir(runFolder)
            .filePath(QStringLiteral("source/sequence.json"));
    const auto run =
        run::RunManifestV2::load(runSummaryPath, &provenanceError);
    const auto archivedManifest =
        sequence::SequenceManifestV2::load(
            archivedManifestPath, &provenanceError);
    require(run.has_value() && archivedManifest.has_value() &&
                run->data().sourceSequence.name ==
                    QStringLiteral("Sequence") &&
                run->data().experimentType ==
                    QStringLiteral("controller-test") &&
                run->data().notes == QStringLiteral("facts") &&
                run->data().cameraSettings.value(
                    QStringLiteral("fixture"))
                    .toBool() &&
                run->data().requestedProcessingFps == 20.0 &&
                archivedManifest->data().name ==
                    QStringLiteral("Sequence") &&
                archivedManifest->data().nominalFps == 20.0 &&
                archivedManifest->data().cameraSettings.value(
                    QStringLiteral("fixture"))
                    .toBool() &&
                fileBytes(archivedManifestPath) ==
                    acceptedManifestBytes,
            "Run facts and archived source did not use accepted snapshot");

    const QString corruptRoot = QDir(temporary.path()).filePath("corrupt");
    const QString corruptManifest = makeSequence(corruptRoot, 2, true);
    require(controller.selectSequence(
                QUrl::fromLocalFile(corruptManifest)),
            "selection reads only first frame");
    require(controller.loadToMemory(), "start corrupt decode load");
    require(waitUntil([&] {
                return controller.loadStatus() == QStringLiteral("Error");
            }) &&
                !controller.memoryReady() && !controller.canStart() &&
                controller.errorMessage().contains(
                    QStringLiteral("frame 2 could not be decoded")),
            "decode failure publishes no partial-ready buffer");
}

void modelRoutingAndDaqFacts() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create routing temporary directory");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "create routing output");
    const QString manifest =
        makeSequence(QDir(temporary.path()).filePath("sequence"), 2);

    FakeDetector detector;
    OperationCoordinator operations;
    bool daqReady = false;
    int daqReadinessCalls = 0;
    const auto daqReadiness = [&](QString* error) {
        ++daqReadinessCalls;
        if (!daqReady && error)
            *error = QStringLiteral("DAQ device is unavailable.");
        return daqReady;
    };
    SequenceTestService service(
        operations,
        detector,
        nullptr,
        [](QString*) {
            return std::optional<PreparedModel>(preparedModel());
        },
        [](bool, QString*) { return run::DaqPulseStatus::Issued; },
        daqReadiness);
    qulonglong availableMemory = 1024 * 1024;
    int resultsRefreshes = 0;
    auto controller = makeController(
        service,
        output,
        availableMemory,
        resultsRefreshes,
        [](QString*) {
            return std::optional<run::ModelSnapshot>(
                activeModelSnapshot());
        },
        daqReadiness);

    require(controller.selectSequence(QUrl::fromLocalFile(manifest)) &&
                controller.loadToMemory() &&
                waitUntil([&] { return controller.memoryReady(); }),
            "load model-routing Sequence");
    require(controller.activeModelReady() &&
                controller.activeModelName() ==
                    QStringLiteral("Active Model") &&
                controller.hitClassModel().size() == 3 &&
                !controller.canStart(),
            "publish Active Model and require Hit Class");
    controller.setSelectedHitClassId(QStringLiteral("missing"));
    require(!controller.canStart() &&
                controller.errorMessage().contains(
                    QStringLiteral("not present")),
            "reject incompatible Hit Class");
    controller.setSelectedHitClassId(QStringLiteral("1"));
    require(controller.canStart() && daqReadinessCalls == 0,
            "DAQ OFF does not require or inspect DAQ readiness");
    controller.setPhysicalDaqOutputEnabled(true);
    require(!controller.canStart() && daqReadinessCalls == 1 &&
                controller.errorMessage() ==
                    QStringLiteral("DAQ device is unavailable."),
            "DAQ ON requires readiness with the direct provider reason");
    daqReady = true;
    controller.refreshPreflight();
    controller.setRequestedProcessingFps(1000.0);
    require(controller.canStart() && controller.start(),
            "start class-based physical-DAQ request");
    require(waitUntil([&] {
                return controller.presentation() ==
                       QStringLiteral("completed");
            }) &&
                resultsRefreshes == 1,
            "complete class-based request");

    QString error;
    const auto run = run::RunManifestV2::load(
        QDir(onlyRunFolder(output))
            .filePath(QStringLiteral("run_summary.json")),
        &error);
    require(run.has_value(), qPrintable(error));
    const auto& facts = run->data();
    require(facts.routing.triggerMode == run::TriggerMode::ClassBased &&
                facts.routing.hitClassId ==
                    std::optional<QString>(QStringLiteral("1")) &&
                facts.routing.physicalDaqOutputEnabled &&
                facts.model && facts.model->id == QStringLiteral("active-model") &&
                facts.requestedProcessingFps == 1000.0 &&
                facts.achievedProcessingFps > 0.0 &&
                facts.detectorSettings.value(
                    QStringLiteral("fixed_detector"))
                    .toBool() &&
                facts.cropSettings.value(QStringLiteral("crop_size"))
                        .toInt() == 64 &&
                facts.timingSettings.value(
                    QStringLiteral("fixed_timing"))
                    .toBool() &&
                facts.cameraSettings.value(QStringLiteral("fixture"))
                    .toBool() &&
                facts.daqSettings.value(
                    QStringLiteral("physical_output_enabled"))
                    .toBool(),
            "persist model, routing, DAQ, FPS, and immutable provenance facts");
}

void asynchronousStopAndTeardown() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create stop temporary directory");
    const QString output = QDir(temporary.path()).filePath("runs");
    require(QDir().mkpath(output), "create stop output");
    const QString manifest =
        makeSequence(QDir(temporary.path()).filePath("sequence"), 20);

    std::mutex detectorMutex;
    std::condition_variable detectorChanged;
    bool entered = false;
    bool release = false;
    FakeDetector detector;
    detector.onProcess = [&](int index) {
        if (index != 0)
            return;
        std::unique_lock lock(detectorMutex);
        entered = true;
        detectorChanged.notify_all();
        detectorChanged.wait(lock, [&] { return release; });
    };
    OperationCoordinator operations;
    SequenceTestService service(operations, detector, nullptr);
    qulonglong availableMemory = 1024 * 1024;
    int resultsRefreshes = 0;
    auto controller =
        makeController(service, output, availableMemory, resultsRefreshes);
    bool stopAccepted = false;
    QObject::connect(
        &controller,
        &SequenceTestController::stopRequestAccepted,
        [&] { stopAccepted = true; });
    require(controller.selectSequence(QUrl::fromLocalFile(manifest)) &&
                controller.loadToMemory() &&
                waitUntil([&] { return controller.memoryReady(); }),
            "load stoppable Sequence");
    controller.setTriggerEveryDroplet(true);
    controller.setRequestedProcessingFps(1000.0);
    require(controller.start(), "start stoppable Sequence");
    require(waitUntil([&] {
                std::lock_guard lock(detectorMutex);
                return entered;
            }),
            "enter background detector");

    require(controller.stop(),
            "Stop request accepted by the controller");
    require(waitUntil([&] { return stopAccepted; }),
            "service accepted Stop before detector release");
    {
        std::lock_guard lock(detectorMutex);
        release = true;
    }
    detectorChanged.notify_all();
    require(waitUntil([&] {
                return controller.presentation() ==
                       QStringLiteral("interrupted");
            }) &&
                controller.outputStatus() == QStringLiteral("Interrupted") &&
                controller.processedFrames() == 0 &&
                resultsRefreshes == 1,
            "queued progress, stop, interruption, and Results refresh");

    const QString teardownOutput =
        QDir(temporary.path()).filePath("teardown-runs");
    require(QDir().mkpath(teardownOutput), "create teardown output");
    FakeDetector teardownDetector;
    std::atomic_bool teardownEntered{false};
    std::atomic_bool teardownRelease{false};
    teardownDetector.onProcess = [&](int index) {
        if (index != 0)
            return;
        teardownEntered.store(true, std::memory_order_release);
        while (!teardownRelease.load(std::memory_order_acquire))
            std::this_thread::yield();
    };
    OperationCoordinator teardownOperations;
    SequenceTestService teardownService(
        teardownOperations, teardownDetector, nullptr);
    int teardownRefreshes = 0;
    auto teardownController = std::make_unique<SequenceTestController>(
        teardownService,
        ActiveModelSnapshotProvider{},
        [&teardownRefreshes] { ++teardownRefreshes; },
        [teardownOutput] { return teardownOutput; },
        [&availableMemory] { return availableMemory; },
        DaqReadinessGate{},
        run::HitBoundarySnapshot{4.0, run::HitSide::PositiveY, 8, 8},
        QJsonObject{{QStringLiteral("fixed_detector"), true}},
        QJsonObject{{QStringLiteral("crop_size"), 64}},
        QJsonObject{{QStringLiteral("fixed_timing"), true}},
        QStringLiteral("2"));
    require(teardownController->selectSequence(
                QUrl::fromLocalFile(manifest)) &&
                teardownController->loadToMemory() &&
                waitUntil([&] { return teardownController->memoryReady(); }),
            "load teardown Sequence");
    teardownController->setTriggerEveryDroplet(true);
    teardownController->setRequestedProcessingFps(1000.0);
    require(teardownController->start() &&
                waitUntil([&] {
                    return teardownEntered.load(std::memory_order_acquire);
                }),
            "start teardown Run");
    std::thread releaser([&] {
        QThread::msleep(20);
        teardownRelease.store(true, std::memory_order_release);
    });
    teardownController.reset();
    releaser.join();
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    selectionLoadingAndImmutableBuffer();
    modelRoutingAndDaqFacts();
    asynchronousStopAndTeardown();
    return 0;
}
