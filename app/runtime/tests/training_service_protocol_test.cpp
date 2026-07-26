#include "v2/training/training_service.h"

#include "v2/dataset/dataset_manifest_v2.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>

#include <iostream>

namespace {

using desktop_app::v2::dataset::DatasetManifestV2;
using desktop_app::v2::DatasetAccess;
using desktop_app::v2::OperationCoordinator;
using desktop_app::v2::training::TrainingProfile;
using desktop_app::v2::training::TrainingRequest;
using desktop_app::v2::training::TrainingService;
using desktop_app::v2::training::TrainingState;

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    return writeBytes(path, QJsonDocument(object).toJson(QJsonDocument::Indented));
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString argumentValue(const QStringList &arguments, const QString &name)
{
    const qsizetype index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
}

void emitJsonLine(const QJsonObject &object)
{
    std::cout << QJsonDocument(object).toJson(QJsonDocument::Compact).constData() << '\n' << std::flush;
}

void emitFragmentedJson(const QJsonObject &object, bool newline)
{
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    const qsizetype midpoint = bytes.size() / 2;
    std::cout.write(bytes.constData(), midpoint) << std::flush;
    QThread::msleep(20);
    std::cout.write(bytes.constData() + midpoint, bytes.size() - midpoint);
    if (newline)
        std::cout << '\n';
    std::cout << std::flush;
}

int runFakeTrainer(const QStringList &arguments)
{
    const QString outputDirectory = argumentValue(arguments, QStringLiteral("--output"));
    const QString device = argumentValue(arguments, QStringLiteral("--device"));
    QDir().mkpath(outputDirectory);

    QJsonArray recordedArguments;
    for (const QString &argument : arguments)
        recordedArguments.append(argument);
    writeJson(
        QDir(outputDirectory).filePath(QStringLiteral("fake_observation.json")),
        QJsonObject{
            {QStringLiteral("arguments"), recordedArguments},
            {QStringLiteral("working_directory"), QDir::currentPath()},
            {QStringLiteral("pythonpath"),
             QProcessEnvironment::systemEnvironment().value(QStringLiteral("PYTHONPATH"))},
        });

    if (device == QStringLiteral("fake-cancel")) {
        std::cerr << "fake cancel stderr evidence\n" << std::flush;
        QThread::sleep(10);
        return 0;
    }
    if (device == QStringLiteral("fake-failure")) {
        std::cerr << "fake trainer stderr evidence\n" << std::flush;
        emitJsonLine(QJsonObject{
            {QStringLiteral("event"), QStringLiteral("error")},
            {QStringLiteral("error"),
             QJsonObject{
                 {QStringLiteral("code"), QStringLiteral("FAKE_FAILURE")},
                 {QStringLiteral("message"), QStringLiteral("deliberate trainer failure")},
             }},
        });
        return 7;
    }

    emitFragmentedJson(QJsonObject{
        {QStringLiteral("event"), QStringLiteral("stage_started")},
        {QStringLiteral("stage"), QStringLiteral("head_and_late_blocks")},
        {QStringLiteral("epochs"), 20},
    }, true);
    emitJsonLine(QJsonObject{
        {QStringLiteral("event"), QStringLiteral("epoch_metrics")},
        {QStringLiteral("stage"), QStringLiteral("head_and_late_blocks")},
        {QStringLiteral("epoch"), 3},
        {QStringLiteral("global_epoch"), 3},
    });
    emitJsonLine(QJsonObject{
        {QStringLiteral("event"), QStringLiteral("stage_started")},
        {QStringLiteral("stage"), QStringLiteral("controlled_fine_tune")},
        {QStringLiteral("epochs"), 15},
    });
    emitJsonLine(QJsonObject{
        {QStringLiteral("event"), QStringLiteral("epoch_metrics")},
        {QStringLiteral("stage"), QStringLiteral("controlled_fine_tune")},
        {QStringLiteral("epoch"), 2},
        {QStringLiteral("global_epoch"), 22},
    });

    QString runDirectory = QDir(outputDirectory).filePath(QStringLiteral("fake-run"));
    QString reportedRunDirectory = runDirectory;
    if (device == QStringLiteral("fake-outside-run")) {
        runDirectory = QDir(outputDirectory).absoluteFilePath(QStringLiteral("../outside-run"));
        reportedRunDirectory = runDirectory;
    }
    QString modelPath = QDir(runDirectory).filePath(QStringLiteral("model.onnx"));
    const QString metadataPath = QDir(runDirectory).filePath(QStringLiteral("metadata.json"));
    QString reportedModelPath = modelPath;
    if (device == QStringLiteral("fake-outside-artifact")) {
        modelPath = QDir(runDirectory).absoluteFilePath(QStringLiteral("../outside-model.onnx"));
        reportedModelPath = QStringLiteral("../outside-model.onnx");
    }
    QDir().mkpath(runDirectory);
    if (device != QStringLiteral("fake-missing")) {
        writeBytes(modelPath, QByteArrayLiteral("fake onnx"));
        writeBytes(metadataPath, QByteArrayLiteral("{}"));
    }
    if (device == QStringLiteral("fake-malformed"))
        std::cout << "not-json\n" << std::flush;
    emitFragmentedJson(QJsonObject{
        {QStringLiteral("event"), QStringLiteral("run_finished")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("run_dir"), reportedRunDirectory},
        {QStringLiteral("artifacts"),
         QJsonObject{
             {QStringLiteral("model_onnx"), reportedModelPath},
             {QStringLiteral("metadata_json"), metadataPath},
         }},
    }, false);
    return 0;
}

bool waitForTerminalState(TrainingService &service, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (service.state() == TrainingState::Running && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return service.state() != TrainingState::Running;
}

bool datasetWriteAvailable(OperationCoordinator &operations, const QString &path)
{
    auto access = operations.acquireDataset(path, DatasetAccess::Write);
    return access.acquired();
}

desktop_app::v2::dataset::DatasetManifestData datasetManifest(
    const QString &firstHash,
    const QString &secondHash,
    const QString &thirdHash = {})
{
    using namespace desktop_app::v2::dataset;
    auto record = [](const QString &id, const QString &path, const QString &hash,
                     qint64 frameIndex) {
        return DatasetRecord{
            id,
            path,
            hash,
            QStringLiteral("frame-") + id,
            QStringLiteral("event-") + id,
            QStringLiteral("2026-07-24T10:15:30Z"),
            QRect(1, 2, 64, 64),
            frameIndex,
        };
    };

    DatasetManifestData data;
    data.datasetId = QStringLiteral("training-fixture");
    data.provenance.name = QStringLiteral("Training fixture");
    data.provenance.opendssVersion = QStringLiteral("v2-test");
    data.provenance.createdAt = QStringLiteral("2026-07-24T10:15:30Z");
    data.provenance.updatedAt = QStringLiteral("2026-07-24T10:15:30Z");
    data.provenance.captureStartedAt = QStringLiteral("2026-07-24T10:15:30Z");
    data.provenance.captureEndedAt = QStringLiteral("2026-07-24T10:15:31Z");
    data.provenance.stopReason = QStringLiteral("test");
    data.provenance.status = QStringLiteral("completed");
    data.provenance.sequence.frameCount = thirdHash.isEmpty() ? 2 : 3;
    data.provenance.sequence.imageWidth = 128;
    data.provenance.sequence.imageHeight = 128;
    data.provenance.sequence.bitDepth = 8;
    data.provenance.sequence.nominalFps = 100.0;
    data.classes = {{QStringLiteral("0"), QStringLiteral("Empty")},
                    {QStringLiteral("1"), QStringLiteral("Target")}};
    data.records = {
        record(QStringLiteral("empty"), QStringLiteral("crops/empty.png"), firstHash, 1),
        record(QStringLiteral("target"), QStringLiteral("crops/target.png"), secondHash, 2),
    };
    data.labels = {
        {QStringLiteral("label-empty"), QStringLiteral("empty"), QStringLiteral("0"), false},
        {QStringLiteral("label-target"), QStringLiteral("target"), QStringLiteral("1"), false},
    };
    if (!thirdHash.isEmpty()) {
        data.classes.append({QStringLiteral("2"), QStringLiteral("Multiple")});
        data.records.append(record(QStringLiteral("multiple"),
                                   QStringLiteral("crops/multiple.png"), thirdHash, 3));
        data.labels.append({QStringLiteral("label-multiple"), QStringLiteral("multiple"),
                            QStringLiteral("2"), false});
    }
    return data;
}

TrainingRequest requestFor(
    const QString &datasetPath,
    const QString &outputDirectory,
    const QString &repositoryRoot,
    TrainingProfile profile,
    const QString &device,
    const QString &initializationMode = QStringLiteral("imagenet"),
    const QString &initializationPath = {})
{
    const QString localInitializationPath =
        initializationPath.isEmpty()
            ? QDir(QFileInfo(outputDirectory).absolutePath())
                  .filePath(QStringLiteral("imagenet-weight.pth"))
            : initializationPath;
    return TrainingRequest{
        datasetPath,
        profile,
        QStringLiteral("Fixture model"),
        outputDirectory,
        QCoreApplication::applicationFilePath(),
        device,
        repositoryRoot,
        initializationMode,
        localInitializationPath,
    };
}

int fail(int code, const QString &message)
{
    std::cerr << message.toStdString() << '\n';
    return code;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = application.arguments().mid(1);
    if (arguments.contains(QStringLiteral("-m")))
        return runFakeTrainer(arguments);

    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
        return fail(1, QStringLiteral("Could not create test directory."));

    const QString cropsDirectory = QDir(temporaryDirectory.path()).filePath(QStringLiteral("crops"));
    if (!QDir().mkpath(cropsDirectory))
        return fail(2, QStringLiteral("Could not create crop directory."));
    const QByteArray emptyBytes("empty crop");
    const QByteArray targetBytes("target crop");
    const QByteArray multipleBytes("multiple crop");
    const QString emptyPath = QDir(cropsDirectory).filePath(QStringLiteral("empty.png"));
    const QString targetPath = QDir(cropsDirectory).filePath(QStringLiteral("target.png"));
    const QString multiplePath = QDir(cropsDirectory).filePath(QStringLiteral("multiple.png"));
    if (!writeBytes(emptyPath, emptyBytes) || !writeBytes(targetPath, targetBytes)
        || !writeBytes(multiplePath, multipleBytes)) {
        return fail(3, QStringLiteral("Could not write crop fixtures."));
    }

    const QString datasetPath = QDir(temporaryDirectory.path()).filePath(QStringLiteral("dataset.json"));
    const auto hash = [](const QByteArray &bytes) {
        return QString::fromLatin1(
            QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    };
    QString manifestError;
    if (!DatasetManifestV2::save(
            datasetPath, datasetManifest(hash(emptyBytes), hash(targetBytes)),
            &manifestError))
        return fail(4, QStringLiteral("Could not write Dataset fixture."));
    const QString threeClassDatasetPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("dataset-three-class.json"));
    if (!DatasetManifestV2::save(
            threeClassDatasetPath,
            datasetManifest(hash(emptyBytes), hash(targetBytes), hash(multipleBytes)),
            &manifestError)) {
        return fail(5, QStringLiteral("Could not write three-class Dataset fixture."));
    }

    const QString repositoryRoot = QFileInfo(QStringLiteral(OPENDSS_TEST_REPOSITORY_ROOT)).absoluteFilePath();
    const QString imagenetWeightPath =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("imagenet-weight.pth"));
    const QString checkpointPath =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("user-checkpoint.pth"));
    if (!writeBytes(imagenetWeightPath, QByteArrayLiteral("imagenet"))
        || !writeBytes(checkpointPath, QByteArrayLiteral("checkpoint"))) {
        return fail(5, QStringLiteral("Could not write local weight fixtures."));
    }
    OperationCoordinator operations;
    TrainingService service(operations);
    QString error;
    bool sawFirstStageEpoch = false;
    bool sawSecondStageEpoch = false;
    QObject::connect(&service, &TrainingService::changed, &service, [&] {
        const auto &progress = service.progress();
        sawFirstStageEpoch = sawFirstStageEpoch
            || (progress.stage == QStringLiteral("head_and_late_blocks")
                && progress.stageEpochs == 20 && progress.epoch == 3);
        sawSecondStageEpoch = sawSecondStageEpoch
            || (progress.stage == QStringLiteral("controlled_fine_tune")
                && progress.stageEpochs == 15 && progress.epoch == 2
                && progress.globalEpoch == 22);
    });

    const QString fasterOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("faster"));
    TrainingRequest missingWeightRequest =
        requestFor(datasetPath, fasterOutput, repositoryRoot,
                   TrainingProfile::Faster, QStringLiteral("fake-success"));
    missingWeightRequest.initializationPath =
        QDir(temporaryDirectory.path())
            .filePath(QStringLiteral("missing-imagenet-weight.pth"));
    if (service.start(missingWeightRequest, &error)
        || service.state() != TrainingState::Failed
        || !error.contains(QStringLiteral("local ImageNet weight"))) {
        return fail(
            6,
            QStringLiteral(
                "Training accepted a missing local ImageNet weight path."));
    }
    if (!service.start(
            requestFor(datasetPath, fasterOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-success")),
            &error)) {
        return fail(6, QStringLiteral("Faster run failed: ") + error + service.lastError());
    }
    auto blockedDataset =
        operations.acquireDataset(datasetPath, DatasetAccess::Write);
    auto otherDataset =
        operations.acquireDataset(threeClassDatasetPath, DatasetAccess::Write);
    if (blockedDataset.acquired() || !otherDataset.acquired())
        return fail(6, QStringLiteral("Training did not retain only its Dataset read lock."));
    otherDataset.lease.release();
    if (!waitForTerminalState(service, 5000) || service.state() != TrainingState::Completed
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(6, QStringLiteral("Faster run did not release its Dataset lock."));
    }
    const QJsonObject fasterConfig = readJson(service.configPath());
    const QJsonObject prepared = readJson(service.preparedManifestPath());
    const QJsonObject firstPreparedItem =
        prepared.value(QStringLiteral("items")).toArray().first().toObject();
    if (fasterConfig.value(QStringLiteral("architecture")).toString()
            != QStringLiteral("mobilenet_v3_small")
        || fasterConfig.value(QStringLiteral("batch_size")).toInt() != 64
        || fasterConfig.value(QStringLiteral("stages")).toArray().size() != 2
        || fasterConfig.value(QStringLiteral("input_size")).toArray() != QJsonArray{96, 96, 3}
        || fasterConfig.value(QStringLiteral("initialization"))
                   .toObject()
                   .value(QStringLiteral("mode"))
                   .toString()
            != QStringLiteral("imagenet")
        || fasterConfig.value(QStringLiteral("initialization"))
                   .toObject()
                   .value(QStringLiteral("weight_path"))
                   .toString()
            != QFileInfo(imagenetWeightPath).absoluteFilePath()
        || fasterConfig.value(QStringLiteral("imbalance")).toObject().value(
               QStringLiteral("sampler_alpha")).toDouble()
            != 0.65
        || prepared.value(QStringLiteral("schema_version")).toString()
            != QStringLiteral("dataset-manifest-v1")
        || prepared.value(QStringLiteral("classes")).toArray().size() != 2
        || prepared.value(QStringLiteral("items")).toArray().size() != 2
        || firstPreparedItem.keys()
            != QStringList{QStringLiteral("class_id"), QStringLiteral("record_id"),
                           QStringLiteral("source_path"), QStringLiteral("status")}
        || !QFileInfo(firstPreparedItem.value(QStringLiteral("source_path")).toString()).isAbsolute()
        || !sawFirstStageEpoch || !sawSecondStageEpoch
        || service.progress().stage != QStringLiteral("controlled_fine_tune")
        || service.progress().stageEpochs != 15 || service.progress().epoch != 2
        || service.progress().globalEpoch != 22
        || !QFileInfo(service.result().modelOnnx).isFile()
        || !QFileInfo(service.result().metadataJson).isFile()) {
        return fail(7, QStringLiteral("Faster config, prepared manifest, progress, or result mismatch."));
    }
    const QJsonObject observation =
        readJson(QDir(fasterOutput).filePath(QStringLiteral("fake_observation.json")));
    const QJsonArray observedArguments = observation.value(QStringLiteral("arguments")).toArray();
    QStringList observedArgumentList;
    for (const auto &value : observedArguments)
        observedArgumentList.append(value.toString());
    if (observedArgumentList.value(0) != QStringLiteral("-m")
        || observedArgumentList.value(1) != QStringLiteral("droplet_trainer")
        || observedArgumentList.value(2) != QStringLiteral("train")
        || argumentValue(observedArgumentList, QStringLiteral("--dataset"))
            != service.preparedManifestPath()
        || argumentValue(observedArgumentList, QStringLiteral("--output")) != fasterOutput
        || argumentValue(observedArgumentList, QStringLiteral("--config")) != service.configPath()
        || !observedArgumentList.contains(QStringLiteral("--jsonl"))
        || argumentValue(observedArgumentList, QStringLiteral("--device"))
            != QStringLiteral("fake-success")
        || observation.value(QStringLiteral("working_directory")).toString() != repositoryRoot
        || observation.value(QStringLiteral("pythonpath")).toString()
            != QDir(repositoryRoot).filePath(QStringLiteral("training/python"))) {
        return fail(8, QStringLiteral("Trainer process command or environment mismatch."));
    }

    const QString oldResult = service.result().modelOnnx;
    if (service.start(
            requestFor(
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("missing-dataset.json")),
                QDir(temporaryDirectory.path()).filePath(QStringLiteral("failed-restart")),
                repositoryRoot,
                TrainingProfile::Faster,
                QStringLiteral("fake-success")),
            &error)
        || service.state() != TrainingState::Failed || error.isEmpty() || oldResult.isEmpty()
        || !service.result().modelOnnx.isEmpty() || !service.result().runDirectory.isEmpty()
        || !service.progress().stage.isEmpty() || service.progress().stageEpochs != 0
        || service.progress().epoch != 0 || service.progress().globalEpoch != 0
        || !service.preparedManifestPath().isEmpty() || !service.configPath().isEmpty()
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(9, QStringLiteral("Failed restart retained prior run state."));
    }

    const QString invalidDatasetPath =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("invalid-dataset.json"));
    if (!writeJson(invalidDatasetPath, QJsonObject{})
        || service.start(
            requestFor(invalidDatasetPath,
                       QDir(temporaryDirectory.path()).filePath(QStringLiteral("invalid-output")),
                       repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-success")),
            &error)
        || service.state() != TrainingState::Failed
        || !datasetWriteAvailable(operations, invalidDatasetPath)) {
        return fail(19, QStringLiteral("Synchronous Dataset load failure retained its lock."));
    }

    const QString accurateOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("accurate"));
    if (!service.start(
            requestFor(threeClassDatasetPath, accurateOutput, repositoryRoot, TrainingProfile::MoreAccurate,
                       QStringLiteral("fake-success"),
                       QStringLiteral("checkpoint"), checkpointPath),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Completed
        || readJson(service.configPath()).value(QStringLiteral("architecture")).toString()
            != QStringLiteral("efficientnet_b0")
        || readJson(service.configPath())
                   .value(QStringLiteral("initialization"))
                   .toObject()
                   .value(QStringLiteral("checkpoint_path"))
                   .toString()
            != QFileInfo(checkpointPath).absoluteFilePath()) {
        return fail(10, QStringLiteral("More Accurate profile run failed: ") + service.lastError());
    }
    const QJsonObject threeClassPrepared = readJson(service.preparedManifestPath());
    const QJsonArray threeClasses = threeClassPrepared.value(QStringLiteral("classes")).toArray();
    const QJsonArray threeItems = threeClassPrepared.value(QStringLiteral("items")).toArray();
    if (threeClasses.size() != 3 || threeItems.size() != 3
        || threeClasses.at(0).toObject().value(QStringLiteral("id")).toString() != QStringLiteral("0")
        || threeClasses.at(1).toObject().value(QStringLiteral("id")).toString() != QStringLiteral("1")
        || threeClasses.at(2).toObject().value(QStringLiteral("id")).toString() != QStringLiteral("2")
        || threeItems.at(0).toObject().value(QStringLiteral("class_id")).toString() != QStringLiteral("0")
        || threeItems.at(1).toObject().value(QStringLiteral("class_id")).toString() != QStringLiteral("1")
        || threeItems.at(2).toObject().value(QStringLiteral("class_id")).toString() != QStringLiteral("2")) {
        return fail(11, QStringLiteral("Ordered three-class prepared manifest mismatch."));
    }

    const QString missingOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("missing"));
    if (!service.start(
            requestFor(datasetPath, missingOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-missing")),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Failed
        || !service.lastError().contains(QStringLiteral("missing or out-of-run"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(12, QStringLiteral("Missing success artifacts were accepted."));
    }

    const QString outsideRunOutput =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("outside-run-output"));
    if (!service.start(
            requestFor(datasetPath, outsideRunOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-outside-run")),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Failed
        || !service.lastError().contains(QStringLiteral("outside the output root"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(13, QStringLiteral("Out-of-output run directory was accepted."));
    }

    const QString outsideArtifactOutput =
        QDir(temporaryDirectory.path()).filePath(QStringLiteral("outside-artifact-output"));
    if (!service.start(
            requestFor(datasetPath, outsideArtifactOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-outside-artifact")),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Failed
        || !service.lastError().contains(QStringLiteral("out-of-run"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(14, QStringLiteral("Out-of-run artifact was accepted."));
    }

    const QString malformedOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("malformed"));
    if (!service.start(
            requestFor(datasetPath, malformedOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-malformed")),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Failed
        || !service.lastError().contains(QStringLiteral("Malformed trainer JSONL"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(15, QStringLiteral("Malformed JSONL was accepted."));
    }

    const QString failureOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("failure"));
    if (!service.start(
            requestFor(datasetPath, failureOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-failure")),
            &error)
        || !waitForTerminalState(service, 5000) || service.state() != TrainingState::Failed
        || !service.lastError().contains(QStringLiteral("deliberate trainer failure"))
        || !service.standardError().contains(QStringLiteral("fake trainer stderr evidence"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(16, QStringLiteral("Trainer failure or stderr evidence mismatch."));
    }

    const QString cancelOutput = QDir(temporaryDirectory.path()).filePath(QStringLiteral("cancel"));
    if (!service.start(
            requestFor(datasetPath, cancelOutput, repositoryRoot, TrainingProfile::Faster,
                       QStringLiteral("fake-cancel")),
            &error)) {
        return fail(17, QStringLiteral("Could not start cancellation fixture: ") + error);
    }
    QElapsedTimer startTimer;
    startTimer.start();
    while (!QFileInfo(QDir(cancelOutput).filePath(QStringLiteral("fake_observation.json"))).isFile()
           && startTimer.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    service.cancel();
    if (!waitForTerminalState(service, 7000) || service.state() != TrainingState::Interrupted
        || !service.standardError().contains(QStringLiteral("fake cancel stderr evidence"))
        || !datasetWriteAvailable(operations, datasetPath)) {
        return fail(18, QStringLiteral("Cancellation did not finish as Interrupted with stderr evidence."));
    }

    return 0;
}
