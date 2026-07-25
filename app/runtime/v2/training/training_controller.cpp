#include "training_controller.h"

#include "../state/application_state_store.h"

#include <QFileInfo>

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
                                       QString pythonExecutable,
                                       QString repositoryRoot,
                                       QObject *parent)
    : QObject(parent)
    , service_(operations)
    , stateStore_(stateStore)
    , pythonExecutable_(std::move(pythonExecutable))
    , repositoryRoot_(std::move(repositoryRoot))
{
    connect(&service_, &TrainingService::changed, this, [this] {
        controllerError_.clear();
        publishTrainingState();
        emit changed();
    });
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

QString TrainingController::presentation() const
{
    switch (service_.state()) {
    case TrainingState::Running:
        return QStringLiteral("running");
    case TrainingState::Completed:
        return QStringLiteral("completed");
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
    if (service_.state() == TrainingState::Failed
        || service_.state() == TrainingState::Interrupted) {
        return service_.lastError();
    }
    return inputError();
}

bool TrainingController::selectionsLocked() const
{
    return service_.state() == TrainingState::Running;
}

void TrainingController::setDatasetManifestUrl(const QUrl &url)
{
    if (selectionsLocked() || datasetManifestUrl_ == url)
        return;
    datasetManifestUrl_ = url;
    controllerError_.clear();
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
    QString error;
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
    };
    const bool started = service_.start(request, &error);
    if (!started && service_.state() == TrainingState::Ready) {
        controllerError_ = error;
        emit changed();
    }
    return started;
}

void TrainingController::stop()
{
    service_.cancel();
}

void TrainingController::publishTrainingState()
{
    desktop_app::v2::TrainingState published;
    published.status = applicationStatus(service_.state());
    published.fault =
        service_.state() == TrainingState::Failed
            || service_.state() == TrainingState::Interrupted
        ? service_.lastError()
        : QString{};

    const auto current = stateStore_.snapshot().training;
    if (current.executionId != published.executionId
        || current.status != published.status || current.fault != published.fault) {
        stateStore_.publishTraining(std::move(published));
    }
}

} // namespace desktop_app::v2::training
