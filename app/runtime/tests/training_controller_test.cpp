#include "../desktop_app/pipeline_runner.h"
#include "../v2/dataset/dataset_manifest_v2.h"
#include "../v2/model/model_library_controller.h"
#include "../v2/model/model_load_service.h"
#include "../v2/operation/operation_coordinator.h"
#include "../v2/state/application_state_store.h"
#include "../v2/training/training_controller.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <iostream>

using namespace desktop_app::v2;
using namespace desktop_app::v2::dataset;
using namespace desktop_app::v2::training;

namespace {

bool check(bool value, const QString &message)
{
    if (!value)
        std::cerr << message.toStdString() << '\n';
    return value;
}

QString argumentValue(const QStringList &arguments, const QString &name)
{
    const qsizetype index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

void emitJsonLine(const QJsonObject &object)
{
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).constData()
              << '\n' << std::flush;
}

int runFakeTrainer(const QStringList &arguments)
{
    const QString outputDirectory = argumentValue(arguments, QStringLiteral("--output"));
    QDir().mkpath(outputDirectory);
    if (!writeBytes(
            QDir(outputDirectory).filePath(QStringLiteral("trainer_started")),
            QByteArrayLiteral("started"))) {
        return 2;
    }
    if (QFileInfo(outputDirectory).fileName() == QStringLiteral("cancel")) {
        QThread::sleep(10);
        return 0;
    }

    emitJsonLine({
        {QStringLiteral("event"), QStringLiteral("stage_started")},
        {QStringLiteral("stage"), QStringLiteral("head_and_late_blocks")},
        {QStringLiteral("epochs"), 20},
    });
    emitJsonLine({
        {QStringLiteral("event"), QStringLiteral("epoch_metrics")},
        {QStringLiteral("stage"), QStringLiteral("head_and_late_blocks")},
        {QStringLiteral("epoch"), 3},
        {QStringLiteral("global_epoch"), 3},
    });
    emitJsonLine({
        {QStringLiteral("event"), QStringLiteral("stage_started")},
        {QStringLiteral("stage"), QStringLiteral("controlled_fine_tune")},
        {QStringLiteral("epochs"), 15},
    });
    emitJsonLine({
        {QStringLiteral("event"), QStringLiteral("epoch_metrics")},
        {QStringLiteral("stage"), QStringLiteral("controlled_fine_tune")},
        {QStringLiteral("epoch"), 2},
        {QStringLiteral("global_epoch"), 22},
    });

    const QString runDirectory =
        QDir(outputDirectory).filePath(QStringLiteral("fake-run"));
    QDir().mkpath(runDirectory);
    const QString modelPath =
        QDir(runDirectory).filePath(QStringLiteral("model.onnx"));
    const QString metadataPath =
        QDir(runDirectory).filePath(QStringLiteral("metadata.json"));
    const QDir sourcePackage(
        QDir(QStringLiteral(OPENDSS_TEST_RUNTIME_DIR))
            .filePath(QStringLiteral("models/templates/pretrained/mobilenet_v3_small")));
    if (!QFile::copy(sourcePackage.filePath(QStringLiteral("model.onnx")), modelPath)
        || !QFile::copy(sourcePackage.filePath(QStringLiteral("metadata.json")), metadataPath)
        || !writeBytes(QDir(runDirectory).filePath(QStringLiteral("checkpoint.pth")),
                       QByteArrayLiteral("checkpoint"))) {
        return 3;
    }

    emitJsonLine({
        {QStringLiteral("event"), QStringLiteral("run_finished")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("run_dir"), runDirectory},
        {QStringLiteral("artifacts"),
         QJsonObject{
             {QStringLiteral("model_onnx"), modelPath},
             {QStringLiteral("metadata_json"), metadataPath},
         }},
    });
    return 0;
}

QString hash(const QByteArray &bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

DatasetManifestData datasetFixture(const QByteArray &firstBytes,
                                   const QByteArray &secondBytes)
{
    DatasetManifestData data;
    data.datasetId = QStringLiteral("training-controller-fixture");
    data.provenance.name = QStringLiteral("Training controller fixture");
    data.provenance.opendssVersion = QStringLiteral("v2-test");
    data.provenance.createdAt = QStringLiteral("2026-07-25T10:15:30Z");
    data.provenance.updatedAt = data.provenance.createdAt;
    data.provenance.captureStartedAt = data.provenance.createdAt;
    data.provenance.captureEndedAt = QStringLiteral("2026-07-25T10:15:31Z");
    data.provenance.stopReason = QStringLiteral("test");
    data.provenance.status = QStringLiteral("completed");
    data.provenance.sequence.frameCount = 2;
    data.provenance.sequence.imageWidth = 64;
    data.provenance.sequence.imageHeight = 64;
    data.provenance.sequence.bitDepth = 8;
    data.provenance.sequence.nominalFps = 100.0;
    data.classes = {{QStringLiteral("0"), QStringLiteral("Empty")},
                    {QStringLiteral("1"), QStringLiteral("Target")}};
    data.records = {
        {QStringLiteral("empty"), QStringLiteral("crops/empty.png"), hash(firstBytes),
         QStringLiteral("frame-empty"), QStringLiteral("event-empty"),
         data.provenance.createdAt, QRect(0, 0, 64, 64), 1},
        {QStringLiteral("target"), QStringLiteral("crops/target.png"), hash(secondBytes),
         QStringLiteral("frame-target"), QStringLiteral("event-target"),
         data.provenance.createdAt, QRect(0, 0, 64, 64), 2},
    };
    data.labels = {
        {QStringLiteral("label-empty"), QStringLiteral("empty"), QStringLiteral("0"), false},
        {QStringLiteral("label-target"), QStringLiteral("target"), QStringLiteral("1"), false},
    };
    return data;
}

bool waitForTerminalState(TrainingController &controller, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (controller.presentation() == QStringLiteral("running")
           && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return controller.presentation() != QStringLiteral("running");
}

bool waitForFile(const QString &path, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (!QFileInfo::exists(path) && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return QFileInfo::exists(path);
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments().mid(1);
    if (arguments.contains(QStringLiteral("-m")))
        return runFakeTrainer(arguments);

    QTemporaryDir temporary;
    if (!check(temporary.isValid(), QStringLiteral("Could not create temporary directory.")))
        return 1;

    const QString datasetRoot =
        QDir(temporary.path()).filePath(QStringLiteral("dataset"));
    const QString cropsRoot = QDir(datasetRoot).filePath(QStringLiteral("crops"));
    if (!check(QDir().mkpath(cropsRoot), QStringLiteral("Could not create crop directory.")))
        return 2;
    const QByteArray emptyBytes("empty crop");
    const QByteArray targetBytes("target crop");
    if (!check(writeBytes(QDir(cropsRoot).filePath(QStringLiteral("empty.png")), emptyBytes)
                   && writeBytes(QDir(cropsRoot).filePath(QStringLiteral("target.png")), targetBytes),
               QStringLiteral("Could not write crop fixtures."))) {
        return 3;
    }

    const QString datasetPath =
        QDir(datasetRoot).filePath(QStringLiteral("dataset.json"));
    QString saveError;
    if (!check(
            DatasetManifestV2::save(
                datasetPath, datasetFixture(emptyBytes, targetBytes), &saveError),
            saveError)) {
        return 4;
    }

    OperationCoordinator operations;
    ApplicationStateStore stateStore;
    const QString registryPath =
        QDir(temporary.path()).filePath(QStringLiteral("registry/model_registry.json"));
    ModelLoadService modelLoadService(registryPath);
    PipelineRunner pipeline;
    ModelLibraryController modelLibraryController(registryPath, operations);
    TrainingController controller(
        operations, stateStore, modelLoadService, pipeline, modelLibraryController,
        QCoreApplication::applicationFilePath(),
        QFileInfo(QStringLiteral(OPENDSS_TEST_REPOSITORY_ROOT)).absoluteFilePath());

    const QMetaObject *metaObject = controller.metaObject();
    const int changedSignal = metaObject->indexOfSignal("changed()");
    const char *propertyNames[] = {
        "datasetManifestUrl", "architecture", "modelName", "outputDirectoryUrl",
        "requestedDevice", "presentation", "errorMessage", "stage", "stageEpochs",
        "epoch", "globalEpoch", "resultDirectoryUrl", "modelOnnxUrl", "metadataUrl",
        "registeredPackageUrl", "retrySaveAvailable",
    };
    bool propertiesUseChangedSignal = changedSignal >= 0;
    for (const char *propertyName : propertyNames) {
        const int index = metaObject->indexOfProperty(propertyName);
        propertiesUseChangedSignal =
            propertiesUseChangedSignal && index >= 0
            && metaObject->property(index).notifySignalIndex() == changedSignal;
    }
    const int datasetProperty = metaObject->indexOfProperty("datasetManifestUrl");
    const int outputProperty = metaObject->indexOfProperty("outputDirectoryUrl");
    if (!check(propertiesUseChangedSignal,
               QStringLiteral("Training controller properties do not notify through changed.")) ||
        !check(datasetProperty >= 0
                   && metaObject->property(datasetProperty).metaType().id() == QMetaType::QUrl
                   && outputProperty >= 0
                   && metaObject->property(outputProperty).metaType().id() == QMetaType::QUrl,
               QStringLiteral("Training paths are not QUrl properties.")) ||
        !check(metaObject->indexOfMethod("start()") >= 0
                   && metaObject->indexOfMethod("stop()") >= 0
                   && metaObject->indexOfMethod("retrySave()") >= 0,
               QStringLiteral("Training action invokables are missing.")) ||
        !check(metaObject->indexOfProperty("effectiveDevice") < 0
                   && metaObject->indexOfProperty("elapsedMilliseconds") < 0
                   && metaObject->indexOfProperty("trainingLoss") < 0
                   && metaObject->indexOfMethod("activate()") < 0,
               QStringLiteral("Unsupported Training facts or actions were exposed.")) ||
        !check(controller.presentation() == QStringLiteral("empty")
                   && controller.errorMessage() == QStringLiteral("No dataset selected."),
               QStringLiteral("Initial Training presentation is incorrect."))) {
        return 5;
    }

    controller.setDatasetManifestUrl(QUrl(QStringLiteral("https://example.invalid/dataset.json")));
    if (!check(controller.presentation() == QStringLiteral("unavailable")
                   && controller.errorMessage().contains(QStringLiteral("local file URL")),
               QStringLiteral("Non-local Dataset URL was accepted."))) {
        return 6;
    }

    controller.setDatasetManifestUrl(QUrl::fromLocalFile(datasetPath));
    controller.setArchitecture(QStringLiteral("unsupported"));
    if (!check(controller.architecture() == QStringLiteral("mobilenet")
                   && !controller.errorMessage().isEmpty(),
               QStringLiteral("Unsupported Architecture replaced the valid selection."))) {
        return 7;
    }
    controller.setArchitecture(QStringLiteral("efficientnet"));
    controller.setRequestedDevice(QStringLiteral("other"));
    if (!check(controller.requestedDevice() == QStringLiteral("gpu")
                   && !controller.errorMessage().isEmpty(),
               QStringLiteral("Unsupported Compute Device replaced the valid selection."))) {
        return 8;
    }

    const QString successOutput =
        QDir(temporary.path()).filePath(QStringLiteral("success"));
    controller.setRequestedDevice(QStringLiteral("cpu"));
    controller.setModelName(QStringLiteral("Controller model"));
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(successOutput));
    if (!check(controller.presentation() == QStringLiteral("ready")
                   && controller.errorMessage().isEmpty(),
               QStringLiteral("Valid Training inputs are not Ready."))) {
        return 9;
    }

    bool sawRunning = false;
    bool sawEpochFacts = false;
    QObject::connect(&controller, &TrainingController::changed, &controller, [&] {
        sawRunning = sawRunning
            || controller.presentation() == QStringLiteral("running");
        sawEpochFacts = sawEpochFacts
            || (controller.stage() == QStringLiteral("controlled_fine_tune")
                && controller.stageEpochs() == 15 && controller.epoch() == 2
                && controller.globalEpoch() == 22);
    });

    if (!check(controller.start(), controller.errorMessage())
        || !check(waitForTerminalState(controller, 5000),
                  QStringLiteral("Training did not reach a terminal state."))
        || !check(controller.presentation() == QStringLiteral("completed")
                      && controller.errorMessage().isEmpty(),
                  QStringLiteral("Successful Training did not complete."))
        || !check(sawRunning && sawEpochFacts,
                  QStringLiteral("Running or epoch facts were not projected."))
        || !check(controller.resultDirectoryUrl().isLocalFile()
                      && controller.modelOnnxUrl().isLocalFile()
                      && controller.metadataUrl().isLocalFile()
                      && controller.registeredPackageUrl().isLocalFile()
                      && QFileInfo::exists(controller.modelOnnxUrl().toLocalFile())
                      && QFileInfo::exists(controller.metadataUrl().toLocalFile())
                      && QFileInfo(controller.registeredPackageUrl().toLocalFile()).isDir()
                      && !controller.retrySaveAvailable()
                      && modelLibraryController.selectedId()
                          == modelLibraryController.activeId(),
                  QStringLiteral("Completed registration facts are incorrect."))) {
        return 10;
    }

    const QJsonObject config =
        readJson(QDir(successOutput).filePath(QStringLiteral("training_config.json")));
    const auto trainingState = stateStore.snapshot().training;
    if (!check(config.value(QStringLiteral("architecture")).toString()
                   == QStringLiteral("efficientnet_b0")
                   && config.value(QStringLiteral("device_request")).toString()
                   == QStringLiteral("cpu"),
               QStringLiteral("Architecture or requested device was not forwarded.")) ||
        !check(trainingState.status == TrainingStatus::Completed
                   && trainingState.executionId.isEmpty()
                   && trainingState.fault.isEmpty(),
               QStringLiteral("Completed Training state was not published."))) {
        return 11;
    }

    const int modelCountAfterSuccess = modelLibraryController.modelRows().size();
    if (!check(!controller.retrySave()
                   && modelLibraryController.modelRows().size() == modelCountAfterSuccess,
               QStringLiteral("Repeated completion was allowed to register twice."))) {
        return 12;
    }

    const QString retryOutput =
        QDir(temporary.path()).filePath(QStringLiteral("retry"));
    const QString retryCollision =
        QDir(retryOutput).filePath(QStringLiteral("Retry model"));
    if (!check(QDir().mkpath(retryCollision),
               QStringLiteral("Could not create retry collision fixture."))) {
        return 13;
    }
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(retryOutput));
    controller.setModelName(QStringLiteral("Retry model"));
    if (!check(controller.start(), controller.errorMessage())
        || !check(waitForTerminalState(controller, 5000),
                  QStringLiteral("Retry fixture Training did not finish."))
        || !check(controller.presentation() == QStringLiteral("saveFailed")
                      && controller.retrySaveAvailable()
                      && !controller.errorMessage().isEmpty()
                      && controller.registeredPackageUrl().isEmpty()
                      && QFileInfo(controller.modelOnnxUrl().toLocalFile()).isFile()
                      && QFileInfo(controller.metadataUrl().toLocalFile()).isFile()
                      && QFileInfo(QDir(controller.resultDirectoryUrl().toLocalFile())
                                       .filePath(QStringLiteral("checkpoint.pth"))).isFile()
                      && modelLibraryController.modelRows().size() == modelCountAfterSuccess,
                  QStringLiteral("Save failure did not retain retryable Training artifacts."))) {
        return 14;
    }
    if (!check(!controller.retrySave()
                   && controller.presentation() == QStringLiteral("saveFailed")
                   && modelLibraryController.modelRows().size() == modelCountAfterSuccess,
               QStringLiteral("Repeated failed save corrupted registration state."))) {
        return 15;
    }
    if (!check(QDir(retryCollision).removeRecursively(),
               QStringLiteral("Could not clear retry collision fixture."))
        || !check(controller.retrySave(), controller.errorMessage())
        || !check(controller.presentation() == QStringLiteral("completed")
                      && !controller.retrySaveAvailable()
                      && QFileInfo(controller.registeredPackageUrl().toLocalFile()).isDir()
                      && modelLibraryController.selectedId()
                          == modelLibraryController.activeId(),
                  QStringLiteral("Retry Save did not register and activate the retained result."))) {
        return 16;
    }

    const QString cancelOutput =
        QDir(temporary.path()).filePath(QStringLiteral("cancel"));
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(cancelOutput));
    controller.setModelName(QStringLiteral("Cancelled model"));
    if (!check(controller.start(), controller.errorMessage())
        || !check(waitForFile(
                      QDir(cancelOutput).filePath(QStringLiteral("trainer_started")), 3000),
                  QStringLiteral("Cancellation trainer did not start."))) {
        return 17;
    }
    controller.setModelName(QStringLiteral("Ignored while running"));
    if (!check(controller.modelName() == QStringLiteral("Cancelled model"),
               QStringLiteral("Running Training selections were mutable."))) {
        return 18;
    }
    controller.stop();
    if (!check(waitForTerminalState(controller, 7000),
               QStringLiteral("Stopped Training did not terminate.")) ||
        !check(controller.presentation() == QStringLiteral("interrupted")
                   && controller.errorMessage()
                       == QStringLiteral("Training was interrupted."),
               QStringLiteral("Stopped Training did not project Interrupted.")) ||
        !check(stateStore.snapshot().training.status == TrainingStatus::Interrupted
                   && !stateStore.snapshot().training.fault.isEmpty(),
               QStringLiteral("Interrupted Training state was not published."))) {
        return 19;
    }

    return 0;
}
