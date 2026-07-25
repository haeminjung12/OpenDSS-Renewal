#include "single_image_capture_controller.h"

#include "camera_service.h"
#include "single_image_capture_service.h"
#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <QFile>
#include <QFileInfo>

namespace desktop_app::v2 {
namespace {

bool validOutputFolder(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || !url.isLocalFile()
        || url.hasQuery() || url.hasFragment()) {
        return false;
    }

#ifdef Q_OS_WIN
    const QNtfsPermissionCheckGuard permissionCheck;
#endif
    const QFileInfo folder(url.toLocalFile());
    return folder.exists() && folder.isDir() && folder.isWritable();
}

} // namespace

SingleImageCaptureController::SingleImageCaptureController(
    SingleImageCaptureService &captureService,
    CameraService &cameraService,
    ApplicationStateStore &stateStore,
    OperationCoordinator &operations,
    QObject *parent)
    : QObject(parent)
    , captureService_(captureService)
    , cameraService_(cameraService)
    , operations_(operations)
{
    connect(&stateStore, &ApplicationStateStore::changed, this,
            [this]() { emit stateChanged(); });
    connect(&operations_, &OperationCoordinator::resourcesChanged, this,
            [this]() { emit stateChanged(); });
}

QUrl SingleImageCaptureController::outputFolder() const
{
    return outputFolder_;
}

void SingleImageCaptureController::setOutputFolder(const QUrl &folder)
{
    if (outputFolder_ == folder) {
        return;
    }
    outputFolder_ = folder;
    clearOutcome();
    emit outputFolderChanged();
}

QString SingleImageCaptureController::fileName() const
{
    return fileName_;
}

void SingleImageCaptureController::setFileName(const QString &fileName)
{
    if (fileName_ == fileName) {
        return;
    }
    fileName_ = fileName;
    clearOutcome();
    emit fileNameChanged();
}

bool SingleImageCaptureController::canCapture() const
{
    return !capturing_
        && cameraService_.state().status == CameraStatus::Streaming
        && validOutputFolder(outputFolder_)
        && operations_.momentaryAvailable(ResourceLock::Camera | ResourceLock::Storage);
}

QString SingleImageCaptureController::presentation() const
{
    if (capturing_) {
        return QStringLiteral("capturing");
    }
    if (!error_.isEmpty()) {
        return QStringLiteral("error");
    }
    if (!savedArtifactUrl_.isEmpty()) {
        return QStringLiteral("completed");
    }
    return canCapture() ? QStringLiteral("ready") : QStringLiteral("unavailable");
}

QString SingleImageCaptureController::error() const
{
    return error_;
}

QUrl SingleImageCaptureController::savedArtifactUrl() const
{
    return savedArtifactUrl_;
}

bool SingleImageCaptureController::capture()
{
    if (capturing_) {
        return false;
    }

    setSavedArtifactUrl({});
    setError({});

    if (!outputFolder_.isLocalFile()) {
        setError(outputFolder_.isEmpty()
                     ? QStringLiteral("No save location is selected.")
                     : QStringLiteral("The selected save location must be a local folder."));
        emit stateChanged();
        return false;
    }

    capturing_ = true;
    emit stateChanged();

    QString savedPath;
    QString captureError;
    const bool captured =
        captureService_.capture(outputFolder_.toLocalFile(), fileName_,
                                &savedPath, &captureError);

    capturing_ = false;
    if (!captured) {
        if (captureError.isEmpty()) {
            captureError = QStringLiteral("Camera did not provide a frame.");
        }
        setError(captureError);
        emit stateChanged();
        return false;
    }

    setSavedArtifactUrl(QUrl::fromLocalFile(savedPath));
    emit stateChanged();
    return true;
}

void SingleImageCaptureController::clearOutcome()
{
    setError({});
    setSavedArtifactUrl({});
    emit stateChanged();
}

void SingleImageCaptureController::setError(const QString &error)
{
    if (error_ == error) {
        return;
    }
    error_ = error;
    emit errorChanged();
}

void SingleImageCaptureController::setSavedArtifactUrl(const QUrl &url)
{
    if (savedArtifactUrl_ == url) {
        return;
    }
    savedArtifactUrl_ = url;
    emit savedArtifactUrlChanged();
}

} // namespace desktop_app::v2
