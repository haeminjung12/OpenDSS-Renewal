#include "../desktop_app/pipeline_runner.h"
#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/model/model_load_service.h"
#include "../v2/model_test/model_test_controller.h"
#include "../v2/operation/operation_coordinator.h"
#include "../desktop_app/model_registry_service.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;
using namespace desktop_app::v2::model_test;

bool PipelineRunner::isReady() const { return false; }
void PipelineRunner::installInference(
    std::unique_ptr<OnnxInferenceAdapter>) noexcept {}

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read fixture");
    return file.readAll();
}

QString sha256(const QString& path) {
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256)
            .toHex());
}

void writeJson(const QString& path, const QJsonObject& object) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()),
            "create JSON fixture directory");
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
                            destination.filePath("model.onnx")),
            "copy bundled model package");
    return packagePath;
}

QString makeDataset(const QString& parent, bool corruptSecond = false) {
    const QString root = QDir(parent).filePath("dataset");
    const QString first = QDir(root).filePath("crops/first.png");
    const QString second = QDir(root).filePath("crops/second.png");
    require(QDir().mkpath(QFileInfo(first).absolutePath()),
            "create crop directory");
    QImage image(64, 64, QImage::Format_Grayscale8);
    image.fill(32);
    require(image.save(first, "PNG"), "save first crop");
    image.fill(224);
    if (corruptSecond) {
        QFile corrupt(second);
        require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                    corrupt.write("not-an-image") == 12,
                "save corrupt second crop");
    } else {
        require(image.save(second, "PNG"), "save second crop");
    }

    DatasetManifestData data;
    data.datasetId = QStringLiteral("controller-dataset");
    data.provenance.name = QStringLiteral("Controller Dataset");
    data.provenance.opendssVersion = QStringLiteral("2.0");
    data.provenance.createdAt = QStringLiteral("2026-07-25T12:00:00Z");
    data.provenance.updatedAt = QStringLiteral("2026-07-25T12:01:00Z");
    data.provenance.captureStartedAt =
        QStringLiteral("2026-07-25T12:00:00Z");
    data.provenance.captureEndedAt =
        QStringLiteral("2026-07-25T12:01:00Z");
    data.provenance.stopReason = QStringLiteral("duration_elapsed");
    data.provenance.status = QStringLiteral("completed");
    data.provenance.sequence.frameCount = 2;
    data.provenance.sequence.imageWidth = 64;
    data.provenance.sequence.imageHeight = 64;
    data.provenance.sequence.bitDepth = 8;
    data.provenance.sequence.nominalFps = 100.0;
    data.classes = {{"0", "Empty"},
                    {"1", "Single"},
                    {"2", "More Than One"}};
    const auto record = [&](const QString& id, const QString& path,
                            qint64 frame) {
        return DatasetRecord{
            id,
            QDir::fromNativeSeparators(QDir(root).relativeFilePath(path)),
            sha256(path),
            QStringLiteral("frame-%1").arg(frame),
            QStringLiteral("event-%1").arg(frame),
            QStringLiteral("2026-07-25T12:00:01Z"),
            QRect(0, 0, 64, 64),
            frame};
    };
    data.records = {record(QStringLiteral("first"), first, 1),
                    record(QStringLiteral("second"), second, 2)};
    data.labels = {{QStringLiteral("label-1"), QStringLiteral("first"),
                    QStringLiteral("0"), false},
                   {QStringLiteral("label-2"), QStringLiteral("second"),
                    QStringLiteral("1"), false}};
    const QString manifest = QDir(root).filePath("dataset.json");
    QString error;
    require(DatasetManifestV2::save(manifest, data, &error),
            qPrintable(error));
    return manifest;
}

bool waitUntil(const std::function<bool()>& predicate) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 30000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents();
    return predicate();
}

void testValidationAndCompletedResult() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create temporary directory");
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "active-model"},
                                    {"display_name", "Active Test Model"},
                                    {"package_path",
                                     QDir::cleanPath(packagePath)},
                                    {"active", true}}}}});
    const QByteArray registryBefore = bytes(registryPath);

    require(qputenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE", "1"),
            "set deterministic preflight CUDA override");
    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestController controller(operations, loader, QStringLiteral("2.0"));
    require(!controller.openSummary() &&
                controller.actionError().contains("unavailable") &&
                controller.presentation() == "empty",
            "missing summary artifact rejected");
    require(controller.activeModelReady() &&
                controller.activeModelId() == "active-model" &&
                controller.activeModelName() == "Active Test Model" &&
                controller.plannedDeviceText() == "CPU (automatic)" &&
                !controller.canStart(),
            "persisted Active Model facts projected before execution");
    controller.setDatasetManifestUrl(QUrl(QStringLiteral("https://example.test/dataset.json")));
    controller.setOutputFolderUrl(
        QUrl::fromLocalFile(QDir(temporary.path()).filePath("invalid-output")));
    require(!controller.start() && controller.presentation() == "error" &&
                controller.errorMessage().contains("local"),
            "non-local Dataset URL rejected synchronously");

    const QString manifest = makeDataset(temporary.path());
    controller.setDatasetManifestUrl(QUrl::fromLocalFile(manifest));
    require(!controller.canStart() &&
                controller.errorMessage().contains("writable directory"),
            "nonexistent output parent rejected by preflight");
    const QString outputParent =
        QDir(temporary.path()).filePath("model-test-results");
    require(QDir().mkpath(QDir(outputParent).filePath("model-test")),
            "create colliding Model Test run directory");
    const QString output = QDir(outputParent).filePath("model-test-2");
    auto busy =
        operations.acquire(OperationKind::Training, ResourceLock::Model);
    require(busy.acquired(), "create operation preflight conflict");
    controller.setOutputFolderUrl(QUrl::fromLocalFile(outputParent));
    require(!controller.canStart() &&
                controller.errorMessage().contains("operation"),
            "operation availability blocks preflight");
    busy.lease.release();
    controller.setOutputFolderUrl({});
    controller.setOutputFolderUrl(QUrl::fromLocalFile(outputParent));
    require(controller.presentation() == "ready" && controller.canStart(),
            "valid preflight inputs become ready");
    require(controller.start(), "start accepted");
    require(waitUntil([&] {
                return controller.presentation() == "completed" ||
                       controller.presentation() == "failed";
            }),
            "completed result published");
    qunsetenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE");

    require(controller.presentation() == "completed" &&
                controller.errorMessage().isEmpty(),
            qPrintable(controller.errorMessage()));
    const QVariantMap result = controller.resultSummary();
    require(result.value("status").toString() == "completed" &&
                result.value("activeModelId").toString() == "active-model" &&
                result.value("activeModelName").toString() ==
                    "Active Test Model" &&
                result.value("datasetId").toString() ==
                    "controller-dataset" &&
                result.value("effectiveDevice").toString() == "CPU" &&
                result.value("processedImages").toLongLong() == 2 &&
                result.value("eligibleImages").toLongLong() == 2 &&
                result.contains("overallAccuracy") &&
                result.value("perClass").toList().size() == 3 &&
                result.value("confusionMatrix").toList().size() == 3,
            "factual aggregate result published");
    require(controller.processedImages() == 2 &&
                controller.eligibleImages() == 2 &&
                controller.progress() == 1.0,
            "completed progress published");
    require(controller.summaryUrl() ==
                    QUrl::fromLocalFile(
                        QDir(output).filePath("model_test_summary.json")) &&
                controller.predictionsCsvUrl() ==
                    QUrl::fromLocalFile(
                        QDir(output).filePath("predictions.csv")) &&
                controller.artifactOutputFolderUrl() ==
                    QUrl::fromLocalFile(output),
            "artifact URLs published");
    require(controller.partialSummaryUrl().isEmpty() &&
                controller.partialPredictionsCsvUrl().isEmpty(),
            "completed output has no partial artifacts");
    require(QFileInfo(controller.artifactOutputFolderUrl().toLocalFile())
                    .absolutePath() == QFileInfo(outputParent).absoluteFilePath(),
            "unique Model Test run directory remains contained by output parent");
    require(QFile::remove(controller.summaryUrl().toLocalFile()),
            "remove summary for artifact action failure");
    require(!controller.openSummary() &&
                controller.actionError().contains("unavailable") &&
                controller.presentation() == "completed" &&
                !controller.resultSummary().isEmpty(),
            "artifact action failure preserves completed results");
    require(bytes(registryPath) == registryBefore,
            "Model Test did not mutate Active Model registry");
}

void testImmediateStop() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create stop temporary directory");
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "active-model"},
                                    {"display_name", "Active Test Model"},
                                    {"package_path",
                                     QDir::cleanPath(packagePath)},
                                    {"active", true}}}}});
    require(qputenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE", "1"),
            "set deterministic stop preflight CUDA override");
    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestController controller(operations, loader, QStringLiteral("2.0"));
    controller.setDatasetManifestUrl(
        QUrl::fromLocalFile(makeDataset(temporary.path())));
    controller.setOutputFolderUrl(QUrl::fromLocalFile(temporary.path()));
    require(controller.plannedDeviceText() == "CPU (automatic)",
            "stop preflight publishes planned CPU");
    require(controller.start() && controller.stop(),
            "immediate Stop accepted");
    require(waitUntil([&] {
                return controller.presentation() == "interrupted" ||
                       controller.presentation() == "failed";
            }),
            "stopped result published");
    qunsetenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE");
    require(controller.presentation() == "interrupted" &&
                controller.resultSummary().value("status").toString() ==
                    "stopped" &&
                controller.errorMessage().isEmpty() &&
                controller.partialSummaryUrl().isEmpty() &&
                controller.partialPredictionsCsvUrl().isEmpty() &&
                !controller.summaryUrl().isEmpty() &&
                !controller.predictionsCsvUrl().isEmpty(),
            "immediate Stop remains effective after service startup");

    require(controller.start() && controller.stop(),
            "recovery Start Another accepted");
    require(waitUntil([&] {
                return controller.presentation() == "interrupted" ||
                       controller.presentation() == "failed";
            }) &&
                QFileInfo::exists(
                    QDir(temporary.path()).filePath("model-test-2")),
            "recovery starts a unique output");
}

void testFailedPartialRecovery() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create failed temporary directory");
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{QJsonObject{{"registry_entry_id", "active-model"},
                                    {"display_name", "Active Test Model"},
                                    {"package_path",
                                     QDir::cleanPath(packagePath)},
                                    {"active", true}}}}});
    require(qputenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE", "1"),
            "set deterministic failed preflight CUDA override");
    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestController controller(operations, loader, QStringLiteral("2.0"));
    controller.setDatasetManifestUrl(
        QUrl::fromLocalFile(makeDataset(temporary.path(), true)));
    const QString outputParent =
        QDir(temporary.path()).filePath("failed-results");
    require(QDir().mkpath(outputParent), "create failed output parent");
    controller.setOutputFolderUrl(QUrl::fromLocalFile(outputParent));
    require(controller.start(), "failed run start accepted");
    require(waitUntil(
                [&] { return controller.presentation() == "failed"; }),
            "runtime failure published distinctly");
    qunsetenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE");

    const QString output = QDir(outputParent).filePath("model-test");
    require(controller.errorMessage().contains("could not be decoded") &&
                controller.artifactOutputFolderUrl() ==
                    QUrl::fromLocalFile(output) &&
                controller.partialSummaryUrl() ==
                    QUrl::fromLocalFile(QDir(output).filePath(
                        "model_test_summary.partial.json")) &&
                controller.partialPredictionsCsvUrl() ==
                    QUrl::fromLocalFile(
                        QDir(output).filePath("predictions.partial.csv")) &&
                controller.resultSummary().value("status").toString() ==
                    "failed",
            "failed run publishes factual recoverable artifacts");
}

void testRegistryMutationRefresh() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "create refresh temporary directory");
    const QString packagePath = copyBundledModelPackage(temporary.path());
    const QString registryPath =
        QDir(temporary.path()).filePath("registry/model_registry.json");
    writeJson(
        registryPath,
        QJsonObject{
            {"schema_version", "model-registry-v3-simple"},
            {"entries",
             QJsonArray{
                 QJsonObject{{"registry_entry_id", "first-model"},
                             {"display_name", "First Model"},
                             {"package_path", QDir::cleanPath(packagePath)},
                             {"active", true}},
                 QJsonObject{{"registry_entry_id", "second-model"},
                             {"display_name", "Second Model"},
                             {"package_path", QDir::cleanPath(packagePath)},
                             {"active", false}}}}});
    require(qputenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE", "1"),
            "set deterministic refresh CUDA override");
    OperationCoordinator operations;
    ModelLoadService loader(registryPath);
    ModelTestController controller(operations, loader, QStringLiteral("2.0"));
    int changedCount = 0;
    QObject::connect(&controller, &ModelTestController::changed,
                     [&changedCount] { ++changedCount; });
    require(controller.activeModelId() == "first-model" &&
                controller.activeModelName() == "First Model" &&
                controller.plannedDeviceText() == "CPU (automatic)" &&
                controller.datasetManifestUrl().isEmpty() &&
                controller.outputFolderUrl().isEmpty() &&
                !controller.canStart(),
            "initial preflight facts");

    QString error;
    require(activateModelRegistryEntry(registryPath, "second-model", &error),
            qPrintable(error));
    controller.refreshPreflight();
    require(controller.activeModelId() == "second-model" &&
                controller.activeModelName() == "Second Model" &&
                controller.plannedDeviceText() == "CPU (automatic)" &&
                controller.datasetManifestUrl().isEmpty() &&
                controller.outputFolderUrl().isEmpty() &&
                !controller.canStart() && changedCount == 1,
            "Active switch refreshes preflight without Start inputs");

    require(renameRegistryEntryDisplayName(
                registryPath, "second-model", "Renamed Active Model", &error),
            qPrintable(error));
    controller.refreshPreflight();
    qunsetenv("OVDS_TEST_FORCE_CUDA_UNAVAILABLE");
    require(controller.activeModelId() == "second-model" &&
                controller.activeModelName() == "Renamed Active Model" &&
                controller.plannedDeviceText() == "CPU (automatic)" &&
                controller.datasetManifestUrl().isEmpty() &&
                controller.outputFolderUrl().isEmpty() &&
                !controller.canStart() && changedCount == 2,
            "Active rename refreshes preflight without Start inputs");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testValidationAndCompletedResult();
    testImmediateStop();
    testFailedPartialRecovery();
    testRegistryMutationRefresh();
    return 0;
}
