#include "v2/training/training_service.h"

#include "v2/dataset/dataset_manifest_v2.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>

#include <utility>

namespace desktop_app::v2::training {
namespace {

bool writeJsonFile(const QString &path, const QJsonObject &object, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Could not open %1: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

QJsonObject fixedTrainerConfig(
    const dataset::DatasetManifestV2 &manifest,
    const TrainingRequest &request)
{
    QJsonArray classIds;
    QJsonObject displayLabels;
    for (const auto &datasetClass : manifest.classes()) {
        classIds.append(datasetClass.id);
        displayLabels.insert(datasetClass.id, datasetClass.name);
    }

    QJsonArray stages;
    stages.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("head_and_late_blocks")},
        {QStringLiteral("epochs"), 20},
        {QStringLiteral("learning_rate"), 0.0001},
        {QStringLiteral("trainable"), QStringLiteral("classifier_and_last_blocks")},
    });
    stages.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("controlled_fine_tune")},
        {QStringLiteral("epochs"), 15},
        {QStringLiteral("learning_rate"), 0.00001},
        {QStringLiteral("trainable"), QStringLiteral("controlled_fine_tune")},
    });

    return QJsonObject{
        {QStringLiteral("schema_version"), 2},
        {QStringLiteral("model_name"), request.modelName},
        {QStringLiteral("architecture"),
         request.profile == TrainingProfile::Faster
             ? QStringLiteral("mobilenet_v3_small")
             : QStringLiteral("efficientnet_b0")},
        {QStringLiteral("input_size"), QJsonArray{96, 96, 3}},
        {QStringLiteral("batch_size"), 64},
        {QStringLiteral("num_workers"), 0},
        {QStringLiteral("epochs"), 35},
        {QStringLiteral("stages"), stages},
        {QStringLiteral("weight_decay"), 0.0001},
        {QStringLiteral("patience"), 5},
        {QStringLiteral("min_delta"), 0.000001},
        {QStringLiteral("seed"), 1729},
        {QStringLiteral("deterministic"), true},
        {QStringLiteral("use_amp"), true},
        {QStringLiteral("classifier_output"), QStringLiteral("signed_logits")},
        {QStringLiteral("optimizer"), QJsonObject{{QStringLiteral("name"), QStringLiteral("adam")}}},
        {QStringLiteral("scheduler"),
         QJsonObject{
             {QStringLiteral("name"), QStringLiteral("reduce_on_plateau")},
             {QStringLiteral("factor"), 0.5},
             {QStringLiteral("patience"), 2},
             {QStringLiteral("min_lr"), 0.0000001},
         }},
        {QStringLiteral("augmentation"),
         QJsonObject{
             {QStringLiteral("random_resized_crop"), true},
             {QStringLiteral("affine"), true},
             {QStringLiteral("color_jitter"), true},
             {QStringLiteral("horizontal_flip"), false},
         }},
        {QStringLiteral("imbalance"),
         QJsonObject{
             {QStringLiteral("mode"), QStringLiteral("balanced_sampler")},
             {QStringLiteral("sampler_alpha"), 0.65},
             {QStringLiteral("computed_from"), QStringLiteral("training_split_only")},
         }},
        {QStringLiteral("split_mode"), QStringLiteral("auto")},
        {QStringLiteral("export_onnx"), true},
        {QStringLiteral("onnx_opset"), 18},
        {QStringLiteral("classes"), classIds},
        {QStringLiteral("display_labels"), displayLabels},
        {QStringLiteral("initialization"), QJsonObject{{QStringLiteral("mode"), QStringLiteral("imagenet")}}},
        {QStringLiteral("device_request"), request.device},
    };
}

bool resolveExistingContainedPath(
    const QString &root,
    const QString &requestedPath,
    QString &resolvedPath)
{
    if (requestedPath.trimmed().isEmpty())
        return false;

    QString candidatePath = requestedPath;
    candidatePath.replace('\\', '/');
    const QFileInfo requestedInfo(candidatePath);
    if (requestedInfo.isRelative()) {
        const QString cleanRelative = QDir::cleanPath(candidatePath);
        if (cleanRelative == QStringLiteral("..") || cleanRelative.startsWith(QStringLiteral("../")))
            return false;
        candidatePath = QDir(root).absoluteFilePath(cleanRelative);
    }

    const QString absoluteRoot = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
    const QString absoluteCandidate = QDir::cleanPath(QFileInfo(candidatePath).absoluteFilePath());
    const QString lexicalRelative = QDir(absoluteRoot).relativeFilePath(absoluteCandidate);
    if (QFileInfo(lexicalRelative).isAbsolute() || lexicalRelative == QStringLiteral("..")
        || lexicalRelative.startsWith(QStringLiteral("../"))) {
        return false;
    }

    QString canonicalRoot = QDir::fromNativeSeparators(QFileInfo(absoluteRoot).canonicalFilePath());
    QString canonicalCandidate =
        QDir::fromNativeSeparators(QFileInfo(absoluteCandidate).canonicalFilePath());
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty())
        return false;
    canonicalRoot = QDir::cleanPath(canonicalRoot);
    canonicalCandidate = QDir::cleanPath(canonicalCandidate);
    const QString rootPrefix = canonicalRoot.endsWith('/') ? canonicalRoot : canonicalRoot + '/';
#ifdef Q_OS_WIN
    const bool contained = canonicalCandidate.compare(canonicalRoot, Qt::CaseInsensitive) == 0
        || canonicalCandidate.startsWith(rootPrefix, Qt::CaseInsensitive);
#else
    const bool contained = canonicalCandidate == canonicalRoot || canonicalCandidate.startsWith(rootPrefix);
#endif
    if (!contained)
        return false;
    resolvedPath = canonicalCandidate;
    return true;
}

} // namespace

TrainingService::TrainingService(OperationCoordinator &operations, QObject *parent)
    : QObject(parent)
    , operations_(operations)
{
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    killTimer_.setSingleShot(true);
    killTimer_.setInterval(3000);

    connect(&process_, &QProcess::readyReadStandardOutput, this, &TrainingService::consumeStandardOutput);
    connect(&process_, &QProcess::readyReadStandardError, this, &TrainingService::consumeStandardError);
    connect(
        &process_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        &TrainingService::finish);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && state_ == TrainingState::Running)
            failToStart(process_.errorString());
    });
    connect(&killTimer_, &QTimer::timeout, this, [this] {
        if (process_.state() != QProcess::NotRunning)
            process_.kill();
    });
}

TrainingState TrainingService::state() const
{
    return state_;
}

const TrainingProgress &TrainingService::progress() const
{
    return progress_;
}

const QString &TrainingService::lastError() const
{
    return lastError_;
}

const QString &TrainingService::standardError() const
{
    return standardError_;
}

const TrainingResult &TrainingService::result() const
{
    return result_;
}

const QString &TrainingService::preparedManifestPath() const
{
    return preparedManifestPath_;
}

const QString &TrainingService::configPath() const
{
    return configPath_;
}

bool TrainingService::start(const TrainingRequest &request, QString *error)
{
    if (state_ == TrainingState::Running) {
        if (error)
            *error = QStringLiteral("Training is already running.");
        return false;
    }

    if (error)
        error->clear();
    progress_ = {};
    result_ = {};
    lastError_.clear();
    standardError_.clear();
    preparedManifestPath_.clear();
    configPath_.clear();
    activeOutputRoot_.clear();
    outputBuffer_.clear();
    protocolError_.clear();
    trainerError_.clear();
    runFinished_ = {};
    cancelRequested_ = false;

    auto acquired = operations_.acquireWithDataset(
        OperationKind::Training, ResourceLock::Training | ResourceLock::Storage,
        request.datasetJsonPath, DatasetAccess::Read);
    if (!acquired.acquired()) {
        lastError_ = acquired.fault ? acquired.fault->reason
                                    : QStringLiteral("Training resources are in use.");
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }
    operationLease_ = std::move(acquired.lease);

    QString loadError;
    const auto manifest = dataset::DatasetManifestV2::load(request.datasetJsonPath, &loadError);
    if (!manifest) {
        lastError_ = loadError;
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }
    const auto samples = manifest->trainingSamples(&loadError);
    if (!loadError.isEmpty()) {
        lastError_ = loadError;
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }

    const QFileInfo repositoryInfo(request.repositoryRoot);
    const QString repositoryRoot = repositoryInfo.absoluteFilePath();
    if (request.pythonExecutable.trimmed().isEmpty() || !repositoryInfo.isDir()) {
        lastError_ = QStringLiteral("Python executable and repository root are required.");
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }

    QDir outputDirectory(QFileInfo(request.outputDirectory).absoluteFilePath());
    if (!outputDirectory.exists() && !outputDirectory.mkpath(QStringLiteral("."))) {
        lastError_ = QStringLiteral("Could not create output directory %1.").arg(outputDirectory.absolutePath());
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }
    activeOutputRoot_ = outputDirectory.absolutePath();

    QJsonArray classes;
    for (const auto &datasetClass : manifest->classes()) {
        classes.append(QJsonObject{
            {QStringLiteral("id"), datasetClass.id},
            {QStringLiteral("display_name"), datasetClass.name},
        });
    }
    QJsonArray items;
    for (const auto &sample : samples) {
        items.append(QJsonObject{
            {QStringLiteral("record_id"), sample.recordId},
            {QStringLiteral("source_path"), QFileInfo(sample.cropPath).absoluteFilePath()},
            {QStringLiteral("class_id"), sample.classId},
            {QStringLiteral("status"), QStringLiteral("included")},
        });
    }

    preparedManifestPath_ = outputDirectory.filePath(QStringLiteral("prepared_dataset_manifest.json"));
    configPath_ = outputDirectory.filePath(QStringLiteral("training_config.json"));
    const QJsonObject preparedManifest{
        {QStringLiteral("schema_version"), QStringLiteral("dataset-manifest-v1")},
        {QStringLiteral("dataset_id"), manifest->datasetId()},
        {QStringLiteral("classes"), classes},
        {QStringLiteral("items"), items},
    };
    if (!writeJsonFile(preparedManifestPath_, preparedManifest, &loadError)
        || !writeJsonFile(configPath_, fixedTrainerConfig(*manifest, request), &loadError)) {
        lastError_ = loadError;
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(
        QStringLiteral("PYTHONPATH"),
        QDir(repositoryRoot).filePath(QStringLiteral("training/python")));
    process_.setProcessEnvironment(environment);
    process_.setWorkingDirectory(repositoryRoot);
    process_.setProgram(request.pythonExecutable);
    process_.setArguments({
        QStringLiteral("-m"),
        QStringLiteral("droplet_trainer"),
        QStringLiteral("train"),
        QStringLiteral("--dataset"),
        preparedManifestPath_,
        QStringLiteral("--output"),
        outputDirectory.absolutePath(),
        QStringLiteral("--config"),
        configPath_,
        QStringLiteral("--jsonl"),
        QStringLiteral("--device"),
        request.device,
    });
    if (!operationLease_.transition(OperationLifecycle::Running)) {
        lastError_ = QStringLiteral("Training could not enter Running state.");
        operationLease_.release();
        setState(TrainingState::Failed);
        if (error)
            *error = lastError_;
        return false;
    }
    setState(TrainingState::Running);
    process_.start();
    return true;
}

void TrainingService::cancel()
{
    if (state_ != TrainingState::Running)
        return;
    cancelRequested_ = true;
    process_.terminate();
    killTimer_.start();
}

void TrainingService::setState(TrainingState state)
{
    state_ = state;
    emit changed();
}

void TrainingService::consumeStandardOutput()
{
    outputBuffer_.append(process_.readAllStandardOutput());
    qsizetype newline = -1;
    while ((newline = outputBuffer_.indexOf('\n')) >= 0) {
        const QByteArray line = outputBuffer_.left(newline).trimmed();
        outputBuffer_.remove(0, newline + 1);
        if (!line.isEmpty())
            processOutputLine(line);
    }
}

void TrainingService::consumeStandardError()
{
    standardError_.append(QString::fromUtf8(process_.readAllStandardError()));
    emit changed();
}

void TrainingService::processOutputLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (protocolError_.isEmpty())
            protocolError_ = QStringLiteral("Malformed trainer JSONL output: %1").arg(parseError.errorString());
        return;
    }

    const QJsonObject object = document.object();
    const QString event = object.value(QStringLiteral("event")).toString();
    if (event == QStringLiteral("stage_started")) {
        progress_.stage = object.value(QStringLiteral("stage")).toString();
        progress_.stageEpochs = object.value(QStringLiteral("epochs")).toInt();
        progress_.epoch = 0;
        emit changed();
    } else if (event == QStringLiteral("epoch_metrics")) {
        progress_.stage = object.value(QStringLiteral("stage")).toString(progress_.stage);
        progress_.epoch = object.value(QStringLiteral("epoch")).toInt();
        progress_.globalEpoch = object.value(QStringLiteral("global_epoch")).toInt();
        emit changed();
    } else if (event == QStringLiteral("error")) {
        trainerError_ = object.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
    } else if (event == QStringLiteral("run_finished")
               && object.value(QStringLiteral("status")).toString() == QStringLiteral("ok")) {
        runFinished_ = object;
    }
}

void TrainingService::finish(int exitCode, QProcess::ExitStatus exitStatus)
{
    killTimer_.stop();
    consumeStandardOutput();
    consumeStandardError();
    if (!outputBuffer_.trimmed().isEmpty()) {
        processOutputLine(outputBuffer_.trimmed());
        outputBuffer_.clear();
    }

    if (cancelRequested_) {
        lastError_ = QStringLiteral("Training was interrupted.");
        operationLease_.transition(OperationLifecycle::Interrupted);
        operationLease_.release();
        setState(TrainingState::Interrupted);
        return;
    }

    QString failure;
    if (exitStatus != QProcess::NormalExit || exitCode != 0)
        failure = trainerError_.isEmpty()
            ? QStringLiteral("Trainer exited with code %1.").arg(exitCode)
            : trainerError_;
    else if (!protocolError_.isEmpty())
        failure = protocolError_;
    else if (runFinished_.isEmpty())
        failure = QStringLiteral("Trainer did not report a successful run_finished event.");

    if (failure.isEmpty()) {
        QString runDirectory;
        const QJsonObject artifacts = runFinished_.value(QStringLiteral("artifacts")).toObject();
        QString modelOnnx;
        QString metadataJson;
        if (!resolveExistingContainedPath(
                activeOutputRoot_,
                runFinished_.value(QStringLiteral("run_dir")).toString(),
                runDirectory)
            || !QFileInfo(runDirectory).isDir()) {
            failure = QStringLiteral("Trainer success event referenced a run directory outside the output root.");
        } else if (!resolveExistingContainedPath(
                       runDirectory,
                       artifacts.value(QStringLiteral("model_onnx")).toString(),
                       modelOnnx)
                   || !QFileInfo(modelOnnx).isFile()
                   || !resolveExistingContainedPath(
                       runDirectory,
                       artifacts.value(QStringLiteral("metadata_json")).toString(),
                       metadataJson)
                   || !QFileInfo(metadataJson).isFile()) {
            failure = QStringLiteral("Trainer success event referenced missing or out-of-run result artifacts.");
        } else {
            result_ = {runDirectory, modelOnnx, metadataJson};
        }
    }

    if (!failure.isEmpty()) {
        if (!standardError_.trimmed().isEmpty())
            failure += QStringLiteral(" stderr: %1").arg(standardError_.trimmed());
        lastError_ = failure;
        operationLease_.transition(OperationLifecycle::Failed);
        operationLease_.release();
        setState(TrainingState::Failed);
        return;
    }

    operationLease_.transition(OperationLifecycle::Completed);
    operationLease_.release();
    setState(TrainingState::Completed);
}

void TrainingService::failToStart(const QString &message)
{
    killTimer_.stop();
    lastError_ = QStringLiteral("Could not start trainer: %1").arg(message);
    operationLease_.transition(OperationLifecycle::Failed);
    operationLease_.release();
    setState(TrainingState::Failed);
}

} // namespace desktop_app::v2::training
