#include "training_controller.h"

#include "../../desktop_app/pipeline_runner.h"
#include "../model/model_library_controller.h"
#include "../model/model_load_service.h"
#include "../state/application_state_store.h"
#include "../dataset/dataset_manifest_v2.h"

#include <QFileInfo>
#include <QVariantMap>

#include <utility>

namespace desktop_app::v2::training {
namespace {

QUrl localUrl(const QString &path)
{
    return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
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
                                       QString workingDirectory,
                                       QObject *parent)
      : QObject(parent)
      , service_(operations)
      , operations_(operations)
    , stateStore_(stateStore)
    , modelLoadService_(modelLoadService)
    , pipeline_(pipeline)
    , modelLibraryController_(modelLibraryController)
    , pythonExecutable_(std::move(pythonExecutable))
    , workingDirectory_(std::move(workingDirectory))
{
    connect(&service_, &TrainingService::changed,
            this, &TrainingController::handleServiceChanged);
    connect(&modelLibraryController_, &ModelLibraryController::changed,
            this, &TrainingController::refreshLibraryModels);
    refreshLibraryModels();
    publishTrainingState();
}

QUrl TrainingController::datasetManifestUrl() const { return datasetManifestUrl_; }
QString TrainingController::architecture() const
{
    if (architecture_ == QStringLiteral("mobilenet"))
        return QStringLiteral("MobileNetV3-Small");
    if (architecture_ == QStringLiteral("efficientnet"))
        return QStringLiteral("EfficientNet-B0");
    return {};
}
QString TrainingController::modelName() const { return modelName_; }
QString TrainingController::startingWeights() const
{
    return selectedWeightIndex_ >= 0 && selectedWeightIndex_ < weightOptions_.size()
        ? weightOptions_.at(selectedWeightIndex_).startingWeights : QString{};
}
QStringList TrainingController::libraryModelOptions() const
{
    QStringList options;
    options.reserve(weightOptions_.size());
    for (const WeightOption &option : weightOptions_)
        options.append(option.label);
    return options;
}
int TrainingController::selectedLibraryModelIndex() const
{
    return selectedWeightIndex_;
}
QString TrainingController::selectedLibraryModelId() const
{
    return selectedWeightIndex_ >= 0 && selectedWeightIndex_ < weightOptions_.size()
        ? weightOptions_.at(selectedWeightIndex_).id : QString{};
}
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
    QString datasetError;
    const auto dataset =
        dataset::DatasetManifestV2::load(datasetManifestUrl_.toLocalFile(),
                                         &datasetError);
    if (dataset && dataset->counts().labeled == 0)
        return QStringLiteral("No Labeled Droplet Crops");
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
        || !QFileInfo(workingDirectory_).isDir()) {
        return QStringLiteral("Training environment unavailable.");
    }
    return {};
}

QString TrainingController::errorMessage() const
{
    if (registrationState_ == RegistrationState::Completed)
        return {};
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
    refreshLibraryModels();
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
    auto modelLease = operations_.acquireModel(weights.packagePath, ModelAccess::Read);
    if (!modelLease.acquired()) {
        controllerError_ = modelLease.fault
            ? modelLease.fault->reason
            : QStringLiteral("The selected Library model is in use.");
        emit changed();
        return false;
    }
    selectedModelLease_ = std::move(modelLease.lease);
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
        workingDirectory_,
        weights.initializationMode,
        weights.path,
    };
    const bool started = service_.start(request, &error);
    if (!started && service_.state() == TrainingState::Ready) {
        selectedModelLease_ = {};
        controllerError_ = error;
        emit changed();
    }
    return started;
}

bool TrainingController::selectLibraryModel(int index)
{
    if (selectionsLocked())
        return false;
    refreshLibraryModels();
    if (index < 0 || index >= weightOptions_.size()) {
        controllerError_ = QStringLiteral("Select an existing Library model.");
        emit changed();
        return false;
    }
    if (!QFileInfo(weightOptions_[index].path).isFile()) {
        controllerError_ =
            QStringLiteral("The selected Library Starting Weights are no longer available.");
        refreshLibraryModels();
        return false;
    }
    selectedWeightIndex_ = index;
    modelName_ = weightOptions_.at(index).label;
    architecture_ =
        weightOptions_.at(index).architecture == QStringLiteral("efficientnet_b0")
        ? QStringLiteral("efficientnet") : QStringLiteral("mobilenet");
    controllerError_.clear();
    emit changed();
    return true;
}

void TrainingController::refreshLibraryModels()
{
    if (selectionsLocked()) {
        emit changed();
        return;
    }
    const QString selectedId = selectedLibraryModelId();
    weightOptions_.clear();
    const QVariantList rows = modelLibraryController_.trainingModelRows();
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        weightOptions_.push_back({
            row.value(QStringLiteral("name")).toString(),
            row.value(QStringLiteral("weightPath")).toString(),
            row.value(QStringLiteral("initializationMode")).toString(),
            row.value(QStringLiteral("id")).toString(),
            row.value(QStringLiteral("architecture")).toString(),
            row.value(QStringLiteral("startingWeights")).toString(),
            row.value(QStringLiteral("packagePath")).toString(),
        });
    }

    selectedWeightIndex_ = weightOptions_.isEmpty() ? -1 : 0;
    if (!selectedId.isEmpty()) {
        for (int index = 0; index < weightOptions_.size(); ++index) {
            if (weightOptions_[index].id == selectedId) {
                selectedWeightIndex_ = index;
                break;
            }
        }
    }
    if (selectedWeightIndex_ >= 0) {
        const WeightOption &selected = weightOptions_.at(selectedWeightIndex_);
        modelName_ = selected.label;
        architecture_ =
            selected.architecture == QStringLiteral("efficientnet_b0")
            ? QStringLiteral("efficientnet") : QStringLiteral("mobilenet");
    } else {
        modelName_.clear();
        architecture_.clear();
    }
    emit changed();
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
        QStringLiteral("cpu"), pipeline_, &registeredEntryId, &warning, &error,
        selectedLibraryModelId());
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
    selectedModelLease_ = {};
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
    if (service_.state() == TrainingState::Failed
        || service_.state() == TrainingState::Interrupted) {
        selectedModelLease_ = {};
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
