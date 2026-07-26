#include "training_controller.h"

#include "../../desktop_app/pipeline_runner.h"
#include "../model/model_library_controller.h"
#include "../model/model_load_service.h"
#include "../state/application_state_store.h"
#include "../dataset/dataset_manifest_v2.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStandardPaths>
#include <QVariantMap>

#include <utility>

namespace desktop_app::v2::training {
namespace {

QUrl localUrl(const QString &path)
{
    return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    return parseError.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

desktop_app::v2::TrainingStatus applicationStatus(TrainingState state)
{
    switch (state) {
    case TrainingState::Ready:
        return desktop_app::v2::TrainingStatus::Idle;
    case TrainingState::Running:
        return desktop_app::v2::TrainingStatus::Running;
    case TrainingState::Completed:
        return desktop_app::v2::TrainingStatus::Completed;
    case TrainingState::Failed:
        return desktop_app::v2::TrainingStatus::Failed;
    case TrainingState::Interrupted:
        return desktop_app::v2::TrainingStatus::Interrupted;
    }
    return desktop_app::v2::TrainingStatus::Idle;
}

} // namespace

TrainingController::TrainingController(OperationCoordinator &operations,
                                       ApplicationStateStore &stateStore,
                                       ModelLoadService &modelLoadService,
                                       PipelineRunner &pipeline,
                                       ModelLibraryController &modelLibraryController,
                                       QString pythonExecutable,
                                       QString repositoryRoot,
                                       QString modelsRoot,
                                       QObject *parent)
    : QObject(parent)
    , service_(operations)
    , stateStore_(stateStore)
    , modelLoadService_(modelLoadService)
    , pipeline_(pipeline)
    , modelLibraryController_(modelLibraryController)
    , pythonExecutable_(std::move(pythonExecutable))
    , repositoryRoot_(std::move(repositoryRoot))
    , modelsRoot_(
          modelsRoot.trimmed().isEmpty()
              ? QDir(QStandardPaths::writableLocation(
                         QStandardPaths::DocumentsLocation))
                    .filePath(QStringLiteral(
                        "OpenDropletSortingSuite/models"))
              : QFileInfo(modelsRoot).absoluteFilePath())
{
    connect(&service_, &TrainingService::changed,
            this, &TrainingController::handleServiceChanged);
    refreshWeightOptions();
    publishTrainingState();
}

QUrl TrainingController::datasetManifestUrl() const { return datasetManifestUrl_; }
QString TrainingController::architecture() const { return architecture_; }
QString TrainingController::modelName() const { return modelName_; }
QUrl TrainingController::outputDirectoryUrl() const { return outputDirectoryUrl_; }
QString TrainingController::requestedDevice() const { return requestedDevice_; }
QString TrainingController::stage() const { return service_.progress().stage; }
int TrainingController::stageEpochs() const { return service_.progress().stageEpochs; }
int TrainingController::epoch() const { return service_.progress().epoch; }
int TrainingController::globalEpoch() const { return service_.progress().globalEpoch; }
QUrl TrainingController::resultDirectoryUrl() const
{
    return localUrl(service_.result().runDirectory);
}
QUrl TrainingController::modelOnnxUrl() const { return localUrl(service_.result().modelOnnx); }
QUrl TrainingController::metadataUrl() const { return localUrl(service_.result().metadataJson); }
QUrl TrainingController::registeredPackageUrl() const { return registeredPackageUrl_; }
bool TrainingController::retrySaveAvailable() const
{
    return registrationState_ == RegistrationState::SaveFailed;
}

QStringList TrainingController::weightOptions() const
{
    QStringList labels;
    labels.reserve(weightOptions_.size());
    for (const auto &option : weightOptions_)
        labels.push_back(option.label);
    return labels;
}

int TrainingController::selectedWeightIndex() const { return selectedWeightIndex_; }

QString TrainingController::selectedWeightPath() const
{
    return selectedWeightIndex_ >= 0 && selectedWeightIndex_ < weightOptions_.size()
        ? weightOptions_[selectedWeightIndex_].path : QString{};
}

QString TrainingController::presentation() const
{
    if (service_.state() == TrainingState::Completed) {
        switch (registrationState_) {
        case RegistrationState::Saving:
            return QStringLiteral("saving");
        case RegistrationState::SaveFailed:
            return QStringLiteral("saveFailed");
        case RegistrationState::Completed:
            return QStringLiteral("completed");
        case RegistrationState::NotStarted:
            break;
        }
    }

    switch (service_.state()) {
    case TrainingState::Running:
        return QStringLiteral("running");
    case TrainingState::Completed:
        return QStringLiteral("saving");
    case TrainingState::Failed:
        return QStringLiteral("failed");
    case TrainingState::Interrupted:
        return QStringLiteral("interrupted");
    case TrainingState::Ready:
        if (datasetManifestUrl_.isEmpty())
            return QStringLiteral("empty");
        return inputError().isEmpty() ? QStringLiteral("ready")
                                      : QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QString TrainingController::inputError() const
{
    if (!controllerError_.isEmpty())
        return controllerError_;
    if (datasetManifestUrl_.isEmpty())
        return QStringLiteral("No dataset selected.");
    if (!datasetManifestUrl_.isValid() || !datasetManifestUrl_.isLocalFile())
        return QStringLiteral("Dataset must be a local file URL.");
    if (selectedWeightIndex_ < 0
        || selectedWeightIndex_ >= weightOptions_.size()
        || !QFileInfo(selectedWeightPath()).isFile()) {
        return QStringLiteral(
            "No compatible local Weights are available for the selected Architecture.");
    }
    if (modelName_.trimmed().isEmpty())
        return QStringLiteral("Model name is required.");
    if (outputDirectoryUrl_.isEmpty())
        return QStringLiteral("Output directory is required.");
    if (!outputDirectoryUrl_.isValid() || !outputDirectoryUrl_.isLocalFile())
        return QStringLiteral("Output directory must be a local file URL.");
    if (pythonExecutable_.trimmed().isEmpty()
        || !QFileInfo(repositoryRoot_).isDir()) {
        return QStringLiteral("Training environment unavailable.");
    }
    return {};
}

QString TrainingController::errorMessage() const
{
    if (registrationState_ == RegistrationState::SaveFailed)
        return registrationError_;
    if (service_.state() == TrainingState::Failed
        || service_.state() == TrainingState::Interrupted) {
        return service_.lastError();
    }
    return inputError();
}

bool TrainingController::selectionsLocked() const
{
    return service_.state() == TrainingState::Running
        || registrationState_ == RegistrationState::Saving
        || registrationState_ == RegistrationState::SaveFailed;
}

void TrainingController::setDatasetManifestUrl(const QUrl &url)
{
    if (selectionsLocked() || datasetManifestUrl_ == url)
        return;
    datasetManifestUrl_ = url;
    controllerError_.clear();
    refreshWeightOptions();
    emit changed();
}

void TrainingController::setArchitecture(const QString &architecture)
{
    if (selectionsLocked())
        return;
    const QString normalized = architecture.trimmed().toLower();
    if (normalized != QStringLiteral("mobilenet")
        && normalized != QStringLiteral("efficientnet")) {
        controllerError_ = QStringLiteral("Architecture must be MobileNet or EfficientNet.");
        emit changed();
        return;
    }
    if (architecture_ == normalized && controllerError_.isEmpty())
        return;
    architecture_ = normalized;
    controllerError_.clear();
    refreshWeightOptions();
    emit changed();
}

void TrainingController::setModelName(const QString &name)
{
    if (selectionsLocked() || modelName_ == name)
        return;
    modelName_ = name;
    controllerError_.clear();
    emit changed();
}

void TrainingController::setOutputDirectoryUrl(const QUrl &url)
{
    if (selectionsLocked() || outputDirectoryUrl_ == url)
        return;
    outputDirectoryUrl_ = url;
    controllerError_.clear();
    emit changed();
}

void TrainingController::setRequestedDevice(const QString &device)
{
    if (selectionsLocked())
        return;
    const QString normalized = device.trimmed().toLower();
    if (normalized != QStringLiteral("gpu") && normalized != QStringLiteral("cpu")) {
        controllerError_ = QStringLiteral("Compute Device must be GPU or CPU.");
        emit changed();
        return;
    }
    if (requestedDevice_ == normalized && controllerError_.isEmpty())
        return;
    requestedDevice_ = normalized;
    controllerError_.clear();
    emit changed();
}

bool TrainingController::start()
{
    const QString validationError = inputError();
    if (!validationError.isEmpty()) {
        controllerError_ = validationError;
        emit changed();
        return false;
    }

    controllerError_.clear();
    registrationError_.clear();
    registeredPackageUrl_ = {};
    registrationState_ = RegistrationState::NotStarted;
    QString error;
    const WeightOption &weights = weightOptions_.at(selectedWeightIndex_);
    const TrainingRequest request{
        datasetManifestUrl_.toLocalFile(),
        architecture_ == QStringLiteral("mobilenet")
            ? TrainingProfile::Faster
            : TrainingProfile::MoreAccurate,
        modelName_.trimmed(),
        outputDirectoryUrl_.toLocalFile(),
        pythonExecutable_,
        requestedDevice_ == QStringLiteral("gpu") ? QStringLiteral("cuda")
                                                   : QStringLiteral("cpu"),
        repositoryRoot_,
        weights.initializationMode,
        weights.path,
    };
    const bool started = service_.start(request, &error);
    if (!started && service_.state() == TrainingState::Ready) {
        controllerError_ = error;
        emit changed();
    }
    return started;
}

bool TrainingController::loadWeights(int index)
{
    if (selectionsLocked())
        return false;
    if (index < 0 || index >= weightOptions_.size()) {
        controllerError_ = QStringLiteral("Select an available weights source.");
        emit changed();
        return false;
    }
    if (!QFileInfo(weightOptions_[index].path).isFile()) {
        controllerError_ =
            QStringLiteral("The selected local Weights are no longer available.");
        refreshWeightOptions();
        emit changed();
        return false;
    }
    selectedWeightIndex_ = index;
    controllerError_.clear();
    emit changed();
    return true;
}

void TrainingController::refreshWeightOptions()
{
    const QString selectedPath = selectedWeightPath();
    weightOptions_.clear();

    const QString architecture = architecture_ == QStringLiteral("efficientnet")
        ? QStringLiteral("efficientnet_b0") : QStringLiteral("mobilenet_v3_small");
    int datasetClassCount = 0;
    if (datasetManifestUrl_.isLocalFile()) {
        QString ignored;
        if (const auto manifest =
                dataset::DatasetManifestV2::load(datasetManifestUrl_.toLocalFile(), &ignored)) {
            datasetClassCount = manifest->classes().size();
        }
    }

    const QString imagenetFile = architecture == QStringLiteral("efficientnet_b0")
        ? QStringLiteral("efficientnet_b0_rwightman-7f5810bc.pth")
        : QStringLiteral("mobilenet_v3_small-047dcff4.pth");
    const QFileInfo imageNetWeight(
        QDir(modelsRoot_).filePath(
            QStringLiteral("weights/imagenet/") + imagenetFile));
    if (imageNetWeight.isFile() && imageNetWeight.isReadable()) {
        weightOptions_.push_back({
            QStringLiteral("ImageNet-pretrained — %1").arg(imagenetFile),
            imageNetWeight.absoluteFilePath(),
            QStringLiteral("imagenet"),
        });
    }

    QStringList metadataPaths;
    QDirIterator metadataFiles(modelsRoot_, {QStringLiteral("metadata.json")},
                             QDir::Files, QDirIterator::Subdirectories);
    while (metadataFiles.hasNext())
        metadataPaths.push_back(metadataFiles.next());
    metadataPaths.sort(Qt::CaseInsensitive);

    QSet<QString> seenPaths;
    for (const QString &metadataPath : std::as_const(metadataPaths)) {
        const QJsonObject metadata = readObject(metadataPath);
        const QString schema =
            metadata.value(QStringLiteral("schema_version")).toString();
        if (schema != QStringLiteral("model-metadata-v1")
            && schema != QStringLiteral("model-metadata-v2")) {
            continue;
        }
        if (metadata.value(QStringLiteral("model_id")).toString().trimmed().isEmpty())
            continue;

        const QJsonObject architectureMetadata =
            metadata.value(QStringLiteral("architecture")).toObject();
        if (architectureMetadata.value(QStringLiteral("id")).toString()
                != architecture) {
            continue;
        }
        const int checkpointClassCount =
            architectureMetadata.value(QStringLiteral("num_classes")).toInt();
        if (checkpointClassCount <= 0
            || (datasetClassCount > 0
                && datasetClassCount != checkpointClassCount)) {
            continue;
        }

        const QString checkpointFile =
            metadata.value(QStringLiteral("artifact"))
                .toObject()
                .value(QStringLiteral("checkpoint_file"))
                .toString()
                .trimmed();
        if (checkpointFile.isEmpty()
            || QFileInfo(checkpointFile).isAbsolute()
            || QFileInfo(checkpointFile).fileName() != checkpointFile) {
            continue;
        }
        const QString path =
            QDir(QFileInfo(metadataPath).absolutePath()).filePath(checkpointFile);
        const QString canonicalPath = QFileInfo(path).canonicalFilePath();
        if (canonicalPath.isEmpty() || !QFileInfo(canonicalPath).isFile()
            || !QFileInfo(canonicalPath).isReadable()
            || seenPaths.contains(canonicalPath)) {
            continue;
        }
        seenPaths.insert(canonicalPath);

        QString label = metadata.value(QStringLiteral("model_name")).toString().trimmed();
        if (label.isEmpty())
            label = metadata.value(QStringLiteral("user_facing_label"))
                        .toString().trimmed();
        if (label.isEmpty())
            label = metadata.value(QStringLiteral("model_id")).toString();
        label += QStringLiteral(" — OpenDSS checkpoint");
        weightOptions_.push_back(
            {label, canonicalPath, QStringLiteral("checkpoint")});
    }

    selectedWeightIndex_ = weightOptions_.isEmpty() ? -1 : 0;
    if (!selectedPath.isEmpty()) {
        for (int index = 0; index < weightOptions_.size(); ++index) {
            if (QFileInfo(weightOptions_[index].path).absoluteFilePath()
                == QFileInfo(selectedPath).absoluteFilePath()) {
                selectedWeightIndex_ = index;
                break;
            }
        }
    }
}

void TrainingController::stop()
{
    if (service_.state() == TrainingState::Running)
        service_.cancel();
}

bool TrainingController::retrySave()
{
    if (registrationState_ != RegistrationState::SaveFailed
        || service_.state() != TrainingState::Completed) {
        return false;
    }
    return saveCompletedTraining();
}

bool TrainingController::saveCompletedTraining()
{
    if (service_.state() != TrainingState::Completed
        || registrationState_ == RegistrationState::Saving
        || registrationState_ == RegistrationState::Completed) {
        return false;
    }

    registrationState_ = RegistrationState::Saving;
    registrationError_.clear();
    registeredPackageUrl_ = {};
    publishTrainingState();
    emit changed();

    const TrainingResult result = service_.result();
    QString registeredEntryId;
    QString warning;
    QString error;
    const bool saved = modelLoadService_.saveAndActivateTrainedModel(
        result.runDirectory, result.modelOnnx, result.metadataJson,
        modelName_.trimmed(), outputDirectoryUrl_.toLocalFile(),
        QStringLiteral("cpu"), pipeline_, &registeredEntryId, &warning, &error);
    Q_UNUSED(warning);

    if (!saved) {
        registrationError_ = error.isEmpty()
            ? QStringLiteral("The completed model could not be saved and activated.")
            : error;
        registrationState_ = RegistrationState::SaveFailed;
        publishTrainingState();
        emit changed();
        return false;
    }

    modelLibraryController_.refresh();
    const QVariantList rows = modelLibraryController_.modelRows();
    for (int index = 0; index < rows.size(); ++index) {
        if (rows.at(index).toMap().value(QStringLiteral("id")).toString()
                .compare(registeredEntryId, Qt::CaseInsensitive) == 0) {
            modelLibraryController_.select(index);
            registeredPackageUrl_ = localUrl(
                modelLibraryController_.selectedDetail()
                    .value(QStringLiteral("packageLocation")).toString());
            break;
        }
    }

    registrationState_ = RegistrationState::Completed;
    publishTrainingState();
    emit changed();
    return true;
}

void TrainingController::handleServiceChanged()
{
    controllerError_.clear();
    if (service_.state() == TrainingState::Completed
        && registrationState_ == RegistrationState::NotStarted) {
        saveCompletedTraining();
        return;
    }
    publishTrainingState();
    emit changed();
}

void TrainingController::publishTrainingState()
{
    desktop_app::v2::TrainingState published;
    if (registrationState_ == RegistrationState::SaveFailed) {
        published.status = desktop_app::v2::TrainingStatus::Failed;
        published.fault = registrationError_;
    } else if (registrationState_ == RegistrationState::Saving) {
        published.status = desktop_app::v2::TrainingStatus::Running;
    } else {
        published.status = applicationStatus(service_.state());
        published.fault =
            service_.state() == TrainingState::Failed
                || service_.state() == TrainingState::Interrupted
            ? service_.lastError()
            : QString{};
    }

    const auto current = stateStore_.snapshot().training;
    if (current.executionId != published.executionId
        || current.status != published.status || current.fault != published.fault) {
        stateStore_.publishTraining(std::move(published));
    }
}

} // namespace desktop_app::v2::training
