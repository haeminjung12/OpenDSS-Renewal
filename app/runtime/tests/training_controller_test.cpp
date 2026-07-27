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

bool writeJson(const QString &path, const QJsonObject &object)
{
    return writeBytes(
        path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

bool createCheckpointFixture(const QString &path, const QString &architecture,
                             const QString &name, int classCount)
{
    const QFileInfo info(path);
    QJsonArray classes;
    for (int index = 0; index < classCount; ++index)
        classes.append(QString::number(index));
    return QDir().mkpath(info.absolutePath())
        && writeBytes(path, QByteArrayLiteral("checkpoint"))
        && writeJson(
            QDir(info.absolutePath()).filePath(QStringLiteral("metadata.json")),
            QJsonObject{
                {QStringLiteral("schema_version"),
                 QStringLiteral("model-metadata-v2")},
                {QStringLiteral("model_id"),
                 name.toLower().replace(QLatin1Char(' '), QLatin1Char('-'))},
                {QStringLiteral("architecture"),
                 QJsonObject{
                     {QStringLiteral("id"), architecture},
                     {QStringLiteral("num_classes"), classCount},
                 }},
                {QStringLiteral("classes"), classes},
                {QStringLiteral("model_name"), name},
                {QStringLiteral("artifact"),
                 QJsonObject{
                     {QStringLiteral("checkpoint_file"), info.fileName()},
                 }},
            });
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
    const QString emptyDatasetRoot =
        QDir(temporary.path()).filePath(QStringLiteral("empty-dataset"));
    const QString emptyDatasetPath =
        QDir(emptyDatasetRoot).filePath(QStringLiteral("dataset.json"));
    DatasetManifestData emptyDataset = datasetFixture({}, {});
    emptyDataset.provenance.sequence.frameCount = 0;
    emptyDataset.classes.clear();
    emptyDataset.records.clear();
    emptyDataset.labels.clear();
    if (!check(QDir().mkpath(emptyDatasetRoot)
                   && DatasetManifestV2::save(
                       emptyDatasetPath, emptyDataset, &saveError),
               saveError)) {
        return 26;
    }

    OperationCoordinator operations;
    ApplicationStateStore stateStore;
    const QString registryPath =
        QDir(temporary.path()).filePath(QStringLiteral("registry/model_registry.json"));
    ModelLoadService modelLoadService(registryPath);
    PipelineRunner pipeline;
    ModelLibraryController modelLibraryController(registryPath, operations);
    if (!check(
            QDir().mkpath(QFileInfo(registryPath).absolutePath())
                && writeJson(
                    registryPath,
                    QJsonObject{
                        {QStringLiteral("schema_version"),
                         QStringLiteral("model-registry-v3-simple")},
                        {QStringLiteral("entries"), QJsonArray{}},
                    }),
            QStringLiteral("Could not create the Library registry fixture."))) {
        return 28;
    }
    qputenv(
        "OVDS_MODELS_ROOT_PATH",
        QDir(QStringLiteral(OPENDSS_TEST_RUNTIME_DIR))
            .filePath(QStringLiteral("models")).toUtf8());
    if (!check(
            modelLibraryController.refresh()
                && modelLibraryController.addModel(
                    QStringLiteral("Controller model"), 0, 0),
            modelLibraryController.errorMessage())) {
        return 29;
    }
    const QString modelsRoot =
        QDir(temporary.path()).filePath(QStringLiteral("models"));
    const QString imagenetRoot =
        QDir(modelsRoot).filePath(QStringLiteral("weights/imagenet"));
    const QString mobilenetImageNetPath =
        QDir(imagenetRoot)
            .filePath(QStringLiteral("mobilenet_v3_small-047dcff4.pth"));
    const QString efficientnetImageNetPath =
        QDir(imagenetRoot)
            .filePath(QStringLiteral(
                "efficientnet_b0_rwightman-7f5810bc.pth"));
    const QString efficientnetTrueFinalPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "weights/trained/efficientnet_b0/checkpoints/true_final.pth"));
    const QString efficientnetUserCheckpointPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/efficientnet_b0/user_model/my_resume_weights.pth"));
    const QString incompatibleEfficientnetPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/efficientnet_b0/incompatible_model/incompatible_weights.pth"));
    const QString missingMetadataPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/efficientnet_b0/no_metadata/path_spoof.pth"));
    const QString malformedMetadataPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/malformed/malformed_checkpoint.pth"));
    const QString unrelatedMetadataPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/unrelated/unrelated_checkpoint.pth"));
    const QString unknownClassCountPath =
        QDir(modelsRoot)
            .filePath(QStringLiteral(
                "user_models/unknown_classes/unknown_classes.pth"));
    const QString spoofedImageNetPath =
        QDir(imagenetRoot)
            .filePath(QStringLiteral(
                "efficientnet_b0_rwightman-7f5810bc-copy.pth"));
    const QFileInfo malformedInfo(malformedMetadataPath);
    const QFileInfo unrelatedInfo(unrelatedMetadataPath);
    const QFileInfo unknownClassesInfo(unknownClassCountPath);
    if (!check(
            QDir().mkpath(imagenetRoot)
                && writeBytes(mobilenetImageNetPath,
                              QByteArrayLiteral("mobilenet imagenet"))
                && writeBytes(efficientnetImageNetPath,
                              QByteArrayLiteral("efficientnet imagenet"))
                && writeBytes(spoofedImageNetPath,
                              QByteArrayLiteral("spoofed imagenet"))
                && createCheckpointFixture(
                    efficientnetTrueFinalPath,
                    QStringLiteral("efficientnet_b0"),
                    QStringLiteral("EfficientNet true final"), 2)
                && createCheckpointFixture(
                    efficientnetUserCheckpointPath,
                    QStringLiteral("efficientnet_b0"),
                    QStringLiteral("User checkpoint"), 2)
                && createCheckpointFixture(
                    incompatibleEfficientnetPath,
                    QStringLiteral("efficientnet_b0"),
                    QStringLiteral("Incompatible checkpoint"), 3)
                && QDir().mkpath(QFileInfo(missingMetadataPath).absolutePath())
                && writeBytes(missingMetadataPath,
                              QByteArrayLiteral("no metadata"))
                && QDir().mkpath(malformedInfo.absolutePath())
                && writeBytes(malformedMetadataPath,
                              QByteArrayLiteral("malformed checkpoint"))
                && writeBytes(
                    QDir(malformedInfo.absolutePath())
                        .filePath(QStringLiteral("metadata.json")),
                    QByteArrayLiteral("{not-json"))
                && QDir().mkpath(unrelatedInfo.absolutePath())
                && writeBytes(unrelatedMetadataPath,
                              QByteArrayLiteral("unrelated checkpoint"))
                && writeJson(
                    QDir(unrelatedInfo.absolutePath())
                        .filePath(QStringLiteral("metadata.json")),
                    QJsonObject{
                        {QStringLiteral("schema_version"),
                         QStringLiteral("model-metadata-v2")},
                        {QStringLiteral("model_id"),
                         QStringLiteral("unrelated-package")},
                        {QStringLiteral("model_name"),
                         QStringLiteral("Unrelated metadata")},
                        {QStringLiteral("architecture"),
                         QJsonObject{
                             {QStringLiteral("id"),
                              QStringLiteral("efficientnet_b0")},
                             {QStringLiteral("num_classes"), 2},
                         }},
                        {QStringLiteral("artifact"),
                         QJsonObject{
                             {QStringLiteral("checkpoint_file"),
                              QStringLiteral("different.pth")},
                         }},
                    })
                && QDir().mkpath(unknownClassesInfo.absolutePath())
                && writeBytes(unknownClassCountPath,
                              QByteArrayLiteral("unknown classes"))
                && writeJson(
                    QDir(unknownClassesInfo.absolutePath())
                        .filePath(QStringLiteral("metadata.json")),
                    QJsonObject{
                        {QStringLiteral("schema_version"),
                         QStringLiteral("model-metadata-v2")},
                        {QStringLiteral("model_id"),
                         QStringLiteral("unknown-classes-package")},
                        {QStringLiteral("model_name"),
                         QStringLiteral("Unknown classes checkpoint")},
                        {QStringLiteral("architecture"),
                         QJsonObject{
                             {QStringLiteral("id"),
                              QStringLiteral("efficientnet_b0")},
                         }},
                        {QStringLiteral("artifact"),
                         QJsonObject{
                             {QStringLiteral("checkpoint_file"),
                              unknownClassesInfo.fileName()},
                         }},
                    }),
            QStringLiteral("Could not create local weight fixtures."))) {
        return 24;
    }
    TrainingController controller(
        operations, stateStore, modelLoadService, pipeline, modelLibraryController,
        QCoreApplication::applicationFilePath(),
        QFileInfo(QStringLiteral(OPENDSS_TEST_REPOSITORY_ROOT)).absoluteFilePath());

    const QMetaObject *metaObject = controller.metaObject();
    const int changedSignal = metaObject->indexOfSignal("changed()");
    const char *propertyNames[] = {
        "datasetManifestUrl", "architecture", "modelName", "startingWeights",
        "libraryModelOptions", "selectedLibraryModelIndex",
        "selectedLibraryModelId", "outputDirectoryUrl",
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
                   && metaObject->indexOfMethod("retrySave()") >= 0
                   && metaObject->indexOfMethod("selectLibraryModel(int)") >= 0,
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
    if (!check(controller.libraryModelOptions()
                   == QStringList{QStringLiteral("Controller model")}
                   && controller.selectedLibraryModelIndex() == 0
                   && !controller.selectedLibraryModelId().isEmpty()
                   && controller.modelName() == QStringLiteral("Controller model")
                   && controller.architecture()
                       == QStringLiteral("MobileNetV3-Small")
                   && controller.startingWeights() == QStringLiteral("ImageNet"),
               QStringLiteral("Training does not consume the Library identity read-only."))) {
        return 23;
    }

    controller.setDatasetManifestUrl(QUrl(QStringLiteral("https://example.invalid/dataset.json")));
    if (!check(controller.presentation() == QStringLiteral("unavailable")
                   && controller.errorMessage().contains(QStringLiteral("local file URL")),
               QStringLiteral("Non-local Dataset URL was accepted."))) {
        return 6;
    }

    controller.setDatasetManifestUrl(QUrl::fromLocalFile(emptyDatasetPath));
    if (!check(controller.presentation() == QStringLiteral("unavailable")
                   && controller.errorMessage()
                       == QStringLiteral("No Labeled Droplet Crops")
                   && !controller.start(),
               QStringLiteral("Empty Dataset was available for Training."))) {
        return 27;
    }

    controller.setDatasetManifestUrl(QUrl::fromLocalFile(datasetPath));
    controller.setRequestedDevice(QStringLiteral("other"));
    if (!check(controller.requestedDevice() == QStringLiteral("gpu")
                   && !controller.errorMessage().isEmpty(),
               QStringLiteral("Unsupported Compute Device replaced the valid selection."))) {
        return 8;
    }

    const QString successOutput =
        QDir(temporary.path()).filePath(QStringLiteral("success"));
    controller.setRequestedDevice(QStringLiteral("cpu"));
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(successOutput));
    const QString initialIdentityWeightPath =
        modelLibraryController.trainingModelRows().front().toMap()
            .value(QStringLiteral("weightPath")).toString();
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
                  QStringLiteral("Successful Training did not complete: %1 (%2)")
                      .arg(controller.presentation(), controller.errorMessage()))
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
                   == QStringLiteral("mobilenet_v3_small")
                   && config.value(QStringLiteral("device_request")).toString()
                       == QStringLiteral("cpu")
                   && config.value(QStringLiteral("initialization"))
                              .toObject()
                              .value(QStringLiteral("mode"))
                              .toString()
                       == QStringLiteral("imagenet")
                   && config.value(QStringLiteral("initialization"))
                              .toObject()
                              .value(QStringLiteral("weight_path"))
                              .toString()
                       == initialIdentityWeightPath,
               QStringLiteral("Architecture, device, or local ImageNet weight path was not forwarded.")) ||
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
    if (!check(modelLibraryController.addModel(
                   QStringLiteral("Retry model"), 0, 1),
               modelLibraryController.errorMessage())) {
        return 30;
    }
    const int modelCountBeforeRetry =
        modelLibraryController.modelRows().size();
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(retryOutput));
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
                      && modelLibraryController.modelRows().size() == modelCountBeforeRetry,
                  QStringLiteral("Save failure did not retain retryable Training artifacts."))) {
        return 14;
    }
    if (!check(!controller.retrySave()
                   && controller.presentation() == QStringLiteral("saveFailed")
                   && modelLibraryController.modelRows().size() == modelCountBeforeRetry,
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
    if (!check(modelLibraryController.addModel(
                   QStringLiteral("Cancelled model"), 0, 1),
               modelLibraryController.errorMessage())) {
        return 31;
    }
    controller.setOutputDirectoryUrl(QUrl::fromLocalFile(cancelOutput));
    if (!check(controller.start(), controller.errorMessage())
        || !check(waitForFile(
                      QDir(cancelOutput).filePath(QStringLiteral("trainer_started")), 3000),
                  QStringLiteral("Cancellation trainer did not start."))) {
        return 17;
    }
    if (!check(!controller.selectLibraryModel(0)
                   && controller.modelName() == QStringLiteral("Cancelled model"),
               QStringLiteral("Running Library selection was mutable."))) {
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
