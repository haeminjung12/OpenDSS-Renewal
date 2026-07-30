#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/model/model_load_service.h"
#include "../v2/model_test/model_test_service.h"
#include "../v2/model_test/model_test_summary_v2.h"
#include "../v2/operation/operation_coordinator.h"
#include "../desktop_app/pipeline_runner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;
using namespace desktop_app::v2::model_test;

bool PipelineRunner::isReady() const { return false; }
void PipelineRunner::installInference(
    std::unique_ptr<OnnxInferenceAdapter>) noexcept {}

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read fixture");
    return file.readAll();
}

QString sha(const QString& path) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256).toHex());
}

void writeGarbage(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "open corrupt crop");
    require(file.write("not-an-image") == 12, "write corrupt crop");
}

struct DatasetFixture {
    QString datasetJson;
    QString root;
    QString firstCrop;
    QString secondCrop;
};

DatasetFixture makeDataset(const QString& parent, bool labels = true,
                           bool corruptFirst = false, int classCount = 2) {
    DatasetFixture fixture;
    fixture.root = QDir(parent).filePath("dataset");
    fixture.datasetJson = QDir(fixture.root).filePath("dataset.json");
    fixture.firstCrop = QDir(fixture.root).filePath("crops/first.png");
    fixture.secondCrop = QDir(fixture.root).filePath("crops/second.png");
    QDir().mkpath(QFileInfo(fixture.firstCrop).absolutePath());
    if (corruptFirst) {
        writeGarbage(fixture.firstCrop);
    } else {
        QImage image(64, 64, QImage::Format_Grayscale8);
        image.fill(20);
        require(image.save(fixture.firstCrop, "PNG"), "save first crop");
    }
    QImage second(64, 64, QImage::Format_Grayscale8);
    second.fill(220);
    require(second.save(fixture.secondCrop, "PNG"), "save second crop");
    const QString excludedCrop =
        QDir(fixture.root).filePath("crops/excluded.png");
    const QString unlabeledCrop =
        QDir(fixture.root).filePath("crops/unlabeled.png");
    require(second.save(excludedCrop, "PNG") &&
                second.save(unlabeledCrop, "PNG"),
            "save ineligible crops");

    DatasetManifestData data;
    data.datasetId = "dataset-id";
    auto& p = data.provenance;
    p.name = "Dataset";
    p.experimentType = "";
    p.notes = "";
    p.opendssVersion = "2.0";
    p.createdAt = "2026-07-24T12:00:00Z";
    p.updatedAt = "2026-07-24T12:01:00Z";
    p.captureStartedAt = "2026-07-24T12:00:00Z";
    p.captureEndedAt = "2026-07-24T12:01:00Z";
    p.stopReason = "duration_elapsed";
    p.status = "completed";
    p.sequence.frameCount = 4;
    p.sequence.imageWidth = 64;
    p.sequence.imageHeight = 64;
    p.sequence.bitDepth = 8;
    p.sequence.nominalFps = 100.0;
    data.classes = {{"0", "Dataset Alpha"}, {"1", "Dataset Beta"}};
    if (classCount == 3)
        data.classes.push_back({"2", "Dataset Gamma"});
    const auto record = [&](const QString& id, const QString& path,
                            qint64 frame) {
        return DatasetRecord{id,
                             QDir::fromNativeSeparators(
                                 QDir(fixture.root).relativeFilePath(path)),
                             sha(path),
                             QString("frame-%1").arg(frame),
                             QString("event-%1").arg(frame),
                             "2026-07-24T12:00:01Z",
                             QRect(0, 0, 64, 64),
                             frame};
    };
    data.records = {record("first", fixture.firstCrop, 1),
                    record("second", fixture.secondCrop, 2),
                    record("excluded", excludedCrop, 3),
                    record("unlabeled", unlabeledCrop, 4)};
    if (labels) {
        data.labels = {{"label-1", "first", "0", false},
                       {"label-2", "second", "1", false},
                       {"label-3", "excluded", {}, true}};
    }
    QString error;
    require(DatasetManifestV2::save(fixture.datasetJson, data, &error),
            qPrintable(error));
    return fixture;
}

void writeJson(const QString& path, const QJsonObject& object) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open JSON fixture");
    const QByteArray data = QJsonDocument(object).toJson();
    require(file.write(data) == data.size(), "write JSON fixture");
}

QString copyBundledModelPackage(const QString& parent) {
    const QDir source(
        QDir(QString::fromUtf8(OPENDSS_TEST_RUNTIME_DIR))
            .filePath("models/templates/pretrained/mobilenet_v3_small"));
    const QString packagePath = QDir(parent).filePath("model-package");
    require(QDir().mkpath(packagePath), "create model package");
    const QDir destination(packagePath);
    require(QFile::copy(source.filePath("metadata.json"),
                        destination.filePath("metadata.json")) &&
                QFile::copy(source.filePath("model.onnx"),
                            destination.filePath("model.onnx")) &&
                QFile::copy(source.filePath("model.onnx"),
                            destination.filePath("checkpoint.pth")),
            "copy bundled model package");
    return packagePath;
}

int runFakeModelTestProcess(const QStringList& arguments) {
    QTextStream input(stdin);
    QTextStream output(stdout);
    const int checkpointOption = arguments.indexOf("--checkpoint");
    if (checkpointOption < 0 || checkpointOption + 1 >= arguments.size())
        return 2;
    const QJsonObject request =
        QJsonDocument::fromJson(input.readLine().toUtf8()).object();
    const QJsonArray items = request.value("items").toArray();
    output << QJsonDocument(
                  QJsonObject{{"schema_version", 1},
                              {"event", "ready"},
                              {"checkpoint_sha256",
                               sha(arguments.at(checkpointOption + 1))},
                              {"device", "cpu"},
                              {"total", items.size()}})
                  .toJson(QJsonDocument::Compact)
           << Qt::endl;
    if (QJsonDocument::fromJson(input.readLine().toUtf8())
            .object()
            .value("command")
            .toString() != "start") {
        return 3;
    }
    if (qEnvironmentVariableIntValue("OPENDSS_MODEL_TEST_FAKE_HANG") == 1) {
        while (true)
            QThread::msleep(100);
    }

    qint64 processed = 0;
    bool stopped = false;
    for (qsizetype index = 0; index < items.size() && !stopped; ++index) {
        const QJsonObject item = items.at(index).toObject();
        output << QJsonDocument(
                      QJsonObject{
                          {"schema_version", 1},
                          {"event", "batch_ready"},
                          {"batch_index", index},
                          {"facts",
                           QJsonArray{QJsonObject{
                               {"sequence", item.value("sequence")},
                               {"record_id", item.value("record_id")},
                               {"image_path", item.value("image_path")},
                               {"true_class_id", item.value("true_class_id")},
                               {"predicted_class_id", QString::number(index)},
                               {"class_scores",
                                index == 0
                                    ? QJsonArray{0.9, 0.05, 0.05}
                                    : QJsonArray{0.05, 0.9, 0.05}}}}}})
                      .toJson(QJsonDocument::Compact)
               << Qt::endl;
        const QJsonObject acknowledgement =
            QJsonDocument::fromJson(input.readLine().toUtf8()).object();
        if (acknowledgement.value("command").toString() != "committed" ||
            acknowledgement.value("batch_index").toInteger(-1) != index) {
            return 4;
        }
        ++processed;
        stopped = acknowledgement.value("stop").toBool();
    }
    output << QJsonDocument(
                  QJsonObject{{"schema_version", 1},
                              {"event", "run_finished"},
                              {"status", stopped ? "stopped" : "completed"},
                              {"processed", processed},
                              {"total", items.size()}})
                  .toJson(QJsonDocument::Compact)
           << Qt::endl;
    return 0;
}

PreparedModelTestModel prepared(
    std::function<std::optional<ModelTestInferenceResult>(const cv::Mat&, QString*)>
        classify = {}) {
    PreparedModelTestModel model;
    model.snapshot.id = "active-entry";
    model.snapshot.name = "Active Model";
    model.snapshot.checkpointSha256 = QString(64, 'a');
    model.snapshot.metadataSha256 = QString(64, 'b');
    model.snapshot.classes = {{"red-internal", "Model Red"},
                              {"blue-internal", "Model Blue"}};
    model.effectiveDevice = EffectiveDevice::Cpu;
    model.fallbackWarning = "CUDA unavailable; Auto mode used CPU.";
    model.classify =
        classify ? std::move(classify)
                 : [](const cv::Mat& crop,
                      QString*) -> std::optional<ModelTestInferenceResult> {
                       return crop.at<uchar>(0, 0) < 100
                                  ? ModelTestInferenceResult{{0.9, 0.1}}
                                  : ModelTestInferenceResult{{0.2, 0.8}};
                   };
    return model;
}

void testCompletedAndClassCountOnly() {
    stage = "completed";
    QTemporaryDir temporary;
    const auto fixture = makeDataset(temporary.path());
    OperationCoordinator operations;
    QVector<QPair<qint64, qint64>> progress;
    ModelTestService service(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            return prepared();
        },
        [&](qint64 done, qint64 total) { progress.push_back({done, total}); });
    const QByteArray datasetBefore = bytes(fixture.datasetJson);
    const QString output = QDir(temporary.path()).filePath("result");
    QString error;
    require(service.run({fixture.datasetJson, output, "2.0"}, &error),
            qPrintable(error));
    auto summary = ModelTestSummaryV2::load(
        QDir(output).filePath("model_test_summary.json"), &error);
    require(summary && summary->data().status == ModelTestStatus::Completed &&
                summary->data().stopReason == "end_of_dataset" &&
                summary->data().eligibleImages == 2 &&
                summary->predictions().size() == 2 &&
                summary->derivedResults().correctPredictions == 2,
            qPrintable(error));
    require(summary->data().dataset.classes.at(0).id == "0" &&
                summary->data().activeModel.classes.at(0).id == "red-internal",
            "different IDs and names preserved");
    require(progress.size() == 3 && progress.constLast() == qMakePair(2LL, 2LL),
            "progress reports initial and each completed crop");
    require(bytes(fixture.datasetJson) == datasetBefore,
            "source Dataset unchanged");
}

void testCheckpointProvenanceMigration() {
    stage = "checkpoint-provenance";
    QTemporaryDir temporary;
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString metadataPath = QDir(packagePath).filePath("metadata.json");
    const QString checkpointPath = QDir(packagePath).filePath("checkpoint.pth");
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    const QString registryName = "Registry Display Name";
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "real-active"},
                                    {"display_name", registryName},
                                    {"package_path",
                                     QDir::cleanPath(packagePath)},
                                     {"active", true}}}}});

    ModelLoadService loader(registryPath);
    const QJsonObject before =
        QJsonDocument::fromJson(bytes(metadataPath)).object();
    require(!before.value("artifact")
                 .toObject()
                 .contains("checkpoint_sha256"),
            "legacy metadata starts without checkpoint SHA");
    const auto migrated = loader.inspectAndMigratePersistedActiveCheckpoint();
    require(migrated.loadable && migrated.id == "real-active" &&
                migrated.displayName == registryName &&
                migrated.checkpointSha256 == sha(checkpointPath) &&
                migrated.metadataSha256 == sha(metadataPath),
            qPrintable(migrated.error));
    QJsonObject expected = before;
    QJsonObject expectedArtifact = expected.value("artifact").toObject();
    expectedArtifact.insert("checkpoint_sha256", sha(checkpointPath));
    expected.insert("artifact", expectedArtifact);
    const QJsonObject after =
        QJsonDocument::fromJson(bytes(metadataPath)).object();
    require(after == expected,
            "legacy migration changes exactly checkpoint_sha256");

    const QByteArray migratedBytes = bytes(metadataPath);
    const auto repeated = loader.inspectAndMigratePersistedActiveCheckpoint();
    require(repeated.loadable && bytes(metadataPath) == migratedBytes,
            "trusted provenance is stable after migration");

    QJsonObject mismatch = after;
    QJsonObject mismatchArtifact = mismatch.value("artifact").toObject();
    mismatchArtifact.insert("checkpoint_sha256", QString(64, 'c'));
    mismatch.insert("artifact", mismatchArtifact);
    writeJson(metadataPath, mismatch);
    const QByteArray mismatchBytes = bytes(metadataPath);
    const auto rejectedMismatch =
        loader.inspectAndMigratePersistedActiveCheckpoint();
    require(!rejectedMismatch.loadable &&
                rejectedMismatch.error.contains("does not match") &&
                bytes(metadataPath) == mismatchBytes,
            "checkpoint hash mismatch blocks without rewriting metadata");

    require(QFile::remove(checkpointPath), "remove checkpoint fixture");
    const auto rejectedMissing =
        loader.inspectAndMigratePersistedActiveCheckpoint();
    require(!rejectedMissing.loadable &&
                rejectedMissing.error.contains("missing or unreadable") &&
                bytes(metadataPath) == mismatchBytes,
            "missing checkpoint blocks without rewriting metadata");
}

void testProductionProcessCommitsBatchBeforeStop() {
    stage = "production-process-stop";
    QTemporaryDir temporary;
    const auto fixture = makeDataset(temporary.path(), true, false, 3);
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "real-active"},
                                    {"display_name", "Registry Display Name"},
                                    {"package_path", QDir::cleanPath(packagePath)},
                                    {"active", true}}}}});

    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestService* servicePointer = nullptr;
    bool runningModelWriteBlocked = false;
    ModelTestService service(
        operations, &loader, {},
        [&](qint64 processed, qint64) {
            if (processed == 0) {
                const auto runningWrite =
                    operations.acquireModel(packagePath, ModelAccess::Write);
                runningModelWriteBlocked =
                    !runningWrite.acquired() && runningWrite.fault.has_value();
            } else if (processed == 1) {
                servicePointer->requestStop();
            }
        },
        QCoreApplication::applicationFilePath(), temporary.path());
    servicePointer = &service;
    const QString output = QDir(temporary.path()).filePath("process-result");
    QString error;
    auto idleWrite = operations.acquireModel(packagePath, ModelAccess::Write);
    require(idleWrite.acquired(),
            "idle Model Test reference does not lock the Model Package");
    idleWrite.lease.release();
    require(service.run({fixture.datasetJson, output, "2.0"}, &error),
            qPrintable(error));
    auto writeAfterRun =
        operations.acquireModel(packagePath, ModelAccess::Write);
    require(runningModelWriteBlocked && writeAfterRun.acquired(),
            "running Model Test holds and then releases the Model Package");
    writeAfterRun.lease.release();

    const auto summary = ModelTestSummaryV2::load(
        QDir(output).filePath("model_test_summary.json"), &error);
    require(summary && summary->data().status == ModelTestStatus::Stopped &&
                summary->data().stopReason == "user" &&
                summary->predictions().size() == 1 &&
                summary->predictions().constFirst().imagePath ==
                    "crops/first.png" &&
                summary->data().activeModel.checkpointSha256 ==
                    sha(QDir(packagePath).filePath("checkpoint.pth")) &&
                summary->data().activeModel.metadataSha256 ==
                    sha(QDir(packagePath).filePath("metadata.json")),
            qPrintable(error));
}

void testStopTerminatesUnresponsiveProcess() {
    stage = "production-process-forced-stop";
    QTemporaryDir temporary;
    const auto fixture = makeDataset(temporary.path(), true, false, 3);
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "real-active"},
                                    {"display_name", "Registry Display Name"},
                                    {"package_path", QDir::cleanPath(packagePath)},
                                    {"active", true}}}}});
    require(qputenv("OPENDSS_MODEL_TEST_FAKE_HANG", "1"),
            "set fake process hang");
    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestService* servicePointer = nullptr;
    ModelTestService service(
        operations, &loader, {},
        [&](qint64 processed, qint64) {
            if (processed == 0)
                servicePointer->requestStop();
        },
        QCoreApplication::applicationFilePath(), temporary.path());
    servicePointer = &service;
    const QString output = QDir(temporary.path()).filePath("forced-stop-result");
    QString error;
    const bool ran =
        service.run({fixture.datasetJson, output, "2.0"}, &error);
    qunsetenv("OPENDSS_MODEL_TEST_FAKE_HANG");
    require(ran, qPrintable(error));
    const auto summary = ModelTestSummaryV2::load(
        QDir(output).filePath("model_test_summary.json"), &error);
    require(summary && summary->data().status == ModelTestStatus::Stopped &&
                summary->predictions().isEmpty(),
            qPrintable(error));
}

void testConflictsAndSetupFailures() {
    stage = "setup";
    QTemporaryDir temporary;
    const auto fixture = makeDataset(temporary.path());
    OperationCoordinator operations;
    int providerCalls = 0;
    ModelTestService service(
        operations, nullptr,
        [&](QString*) -> std::optional<PreparedModelTestModel> {
            ++providerCalls;
            return prepared();
        });
    const QString blockedOutput = QDir(temporary.path()).filePath("blocked");
    auto write = operations.acquireDataset(fixture.datasetJson,
                                           DatasetAccess::Write);
    QString error;
    require(write.acquired() &&
                !service.run({fixture.datasetJson, blockedOutput, "2.0"},
                             &error) &&
                providerCalls == 0 && !QFileInfo::exists(blockedOutput),
            "Dataset conflict blocks before provider and artifacts");
    write.lease.release();

    for (const auto lock : {ResourceLock::Storage, ResourceLock::Model}) {
        auto held = operations.acquireMomentary(lock);
        const QString conflictOutput =
            QDir(temporary.path())
                .filePath(lock == ResourceLock::Storage ? "storage-blocked"
                                                       : "model-blocked");
        require(held.acquired() &&
                    !service.run(
                        {fixture.datasetJson, conflictOutput, "2.0"}, &error) &&
                    providerCalls == 0 && !QFileInfo::exists(conflictOutput),
                "global Model/Storage conflict blocks without side effects");
        held.lease.release();
    }

    const QString existing = QDir(temporary.path()).filePath("existing");
    QDir().mkpath(existing);
    require(!service.run({fixture.datasetJson, existing, "2.0"}, &error),
            "existing output folder refused");

    QTemporaryDir emptyTemporary;
    const auto empty = makeDataset(emptyTemporary.path(), false);
    const QString emptyOutput =
        QDir(emptyTemporary.path()).filePath("no-output");
    require(!service.run({empty.datasetJson, emptyOutput, "2.0"}, &error) &&
                error.contains("no eligible", Qt::CaseInsensitive) &&
                !QFileInfo::exists(emptyOutput),
            "zero eligible crops fail without artifacts");

    ModelTestService throwing(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            throw std::runtime_error("provider boom");
        });
    const QString throwOutput = QDir(temporary.path()).filePath("throw");
    require(!throwing.run({fixture.datasetJson, throwOutput, "2.0"}, &error) &&
                error.contains("provider boom") &&
                !QFileInfo::exists(throwOutput),
            "provider exception is factual and leaves no artifacts");
}

void testRuntimeFailuresAndStop() {
    stage = "runtime";
    QTemporaryDir corruptTemporary;
    const auto corrupt = makeDataset(corruptTemporary.path(), true, true);
    OperationCoordinator operations;
    ModelTestService corruptService(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            return prepared();
        });
    const QString corruptOutput =
        QDir(corruptTemporary.path()).filePath("corrupt-result");
    QString error;
    require(!corruptService.run(
                {corrupt.datasetJson, corruptOutput, "2.0"}, &error),
            "corrupt crop fails");
    auto failed = ModelTestSummaryV2::load(
        QDir(corruptOutput).filePath("model_test_summary.json"), &error);
    require(failed && failed->data().status == ModelTestStatus::Failed &&
                failed->data().stopReason == "corrupt_image",
            qPrintable(error));

    QTemporaryDir scoreTemporary;
    const auto scoreFixture = makeDataset(scoreTemporary.path());
    ModelTestService scoreService(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            return prepared([](const cv::Mat&, QString*) {
                return std::optional<ModelTestInferenceResult>(
                    ModelTestInferenceResult{{1.0}});
            });
        });
    const QString scoreOutput =
        QDir(scoreTemporary.path()).filePath("score-result");
    require(!scoreService.run(
                {scoreFixture.datasetJson, scoreOutput, "2.0"}, &error),
            "invalid score count fails");
    failed = ModelTestSummaryV2::load(
        QDir(scoreOutput).filePath("model_test_summary.json"), &error);
    require(failed && failed->data().stopReason == "invalid_class_scores",
            qPrintable(error));

    ModelTestService exceptionService(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            return prepared([](const cv::Mat&, QString*)
                                -> std::optional<ModelTestInferenceResult> {
                throw std::runtime_error("classifier boom");
            });
        });
    const QString exceptionOutput =
        QDir(scoreTemporary.path()).filePath("exception-result");
    require(!exceptionService.run(
                {scoreFixture.datasetJson, exceptionOutput, "2.0"}, &error) &&
                error.contains("classifier boom"),
            "classifier exception is reported factually");
    failed = ModelTestSummaryV2::load(
        QDir(exceptionOutput).filePath("model_test_summary.json"), &error);
    require(failed && failed->data().stopReason == "processing_exception",
            qPrintable(error));

    QTemporaryDir stopTemporary;
    const auto stopFixture = makeDataset(stopTemporary.path());
    ModelTestService* servicePointer = nullptr;
    ModelTestService stopService(
        operations, nullptr,
        [](QString*) -> std::optional<PreparedModelTestModel> {
            return prepared();
        },
        [&](qint64 processed, qint64) {
            if (processed == 1)
                servicePointer->requestStop();
        });
    servicePointer = &stopService;
    const QString stopOutput =
        QDir(stopTemporary.path()).filePath("stop-result");
    require(stopService.run(
                {stopFixture.datasetJson, stopOutput, "2.0"}, &error),
            qPrintable(error));
    auto stopped = ModelTestSummaryV2::load(
        QDir(stopOutput).filePath("model_test_summary.json"), &error);
    require(stopped && stopped->data().status == ModelTestStatus::Stopped &&
                stopped->data().stopReason == "user" &&
                stopped->predictions().size() == 1,
            qPrintable(error));
    require(!operations.snapshot().kind,
            "leases release after completion, failure, and stop");
}

void testConcurrentRunRejected() {
    stage = "concurrent";
    QTemporaryDir temporary;
    const auto fixture = makeDataset(temporary.path());
    OperationCoordinator operations;
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool proceed = false;
    ModelTestService service(
        operations, nullptr,
        [&](QString*) -> std::optional<PreparedModelTestModel> {
            {
                std::lock_guard lock(mutex);
                entered = true;
            }
            condition.notify_one();
            std::unique_lock lock(mutex);
            condition.wait(lock, [&] { return proceed; });
            return prepared();
        });
    QString workerError;
    const QString output = QDir(temporary.path()).filePath("result");
    std::thread worker([&] {
        service.run({fixture.datasetJson, output, "2.0"}, &workerError);
    });
    {
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return entered; });
    }
    QString secondError;
    require(!service.run(
                {fixture.datasetJson,
                 QDir(temporary.path()).filePath("second"), "2.0"},
                &secondError) &&
                secondError.contains("already running"),
            "concurrent run rejected");
    {
        std::lock_guard lock(mutex);
        proceed = true;
    }
    condition.notify_one();
    worker.join();
    require(workerError.isEmpty(), qPrintable(workerError));
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    if (application.arguments().contains("model-test-process"))
        return runFakeModelTestProcess(application.arguments());
    testCompletedAndClassCountOnly();
    testCheckpointProvenanceMigration();
    testProductionProcessCommitsBatchBeforeStop();
    testStopTerminatesUnresponsiveProcess();
    testConflictsAndSetupFailures();
    testRuntimeFailuresAndStop();
    testConcurrentRunRejected();
    return 0;
}
