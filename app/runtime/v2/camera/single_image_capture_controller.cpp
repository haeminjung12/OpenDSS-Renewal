#include "single_image_capture_controller.h"

#include "camera_controller.h"
#include "single_image_capture_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtConcurrent>

namespace desktop_app::v2 {
namespace {

QString outputFolderError(const QUrl &url)
{
    if (url.isEmpty())
        return QStringLiteral("No save location is selected.");
    if (!url.isValid() || !url.isLocalFile() || url.hasQuery() || url.hasFragment())
        return QStringLiteral("The selected save location must be a local folder.");

#ifdef Q_OS_WIN
    const QNtfsPermissionCheckGuard permissionCheck;
#endif
    const QFileInfo folder(url.toLocalFile());
    if (!folder.exists() || !folder.isDir())
        return QStringLiteral("The selected save location is not a directory.");
    if (!folder.isWritable())
        return QStringLiteral("The selected save location is not writable.");
    return {};
}

QString operationConflict(const OperationFault &fault)
{
    QString message = fault.reason.trimmed();
    if (!fault.recovery.trimmed().isEmpty()) {
        if (!message.isEmpty())
            message += QLatin1Char(' ');
        message += fault.recovery.trimmed();
    }
    return message.isEmpty() ? QStringLiteral("Camera or storage is in use by another operation.")
                             : message;
}

} // namespace

SingleImageCaptureController::SingleImageCaptureController(
    SingleImageCaptureService &captureService,
    CameraController &cameraController,
    OperationCoordinator &operations,
    QObject *parent)
    : QObject(parent)
    , captureService_(captureService)
    , cameraController_(cameraController)
    , operations_(operations)
{
    frameTimeout_.setSingleShot(true);
    frameTimeout_.setInterval(2000);
    connect(&frameTimeout_, &QTimer::timeout,
            this, &SingleImageCaptureController::timeoutWaitingForFrame);
    connect(&saveWatcher_, &QFutureWatcher<CaptureResult>::finished,
            this, &SingleImageCaptureController::finishCapture);
    connect(&cameraController_, &CameraController::frameReady,
            this, &SingleImageCaptureController::acceptFrame);
    connect(&cameraController_, &CameraController::stateChanged,
            this, &SingleImageCaptureController::stateChanged);
    connect(&operations_, &OperationCoordinator::resourcesChanged,
            this, &SingleImageCaptureController::stateChanged);
}

SingleImageCaptureController::~SingleImageCaptureController()
{
    frameTimeout_.stop();
    if (saveWatcher_.isRunning())
        saveWatcher_.waitForFinished();
    lease_.release();
}

bool SingleImageCaptureController::initializeDefaultOutputFolder(
    const QString &documentsFolder)
{
    const QString path =
        QDir(documentsFolder).filePath(QStringLiteral("OpenDropletSortingSuite/Images"));
    setOutputFolder(QUrl::fromLocalFile(path));
    if (documentsFolder.trimmed().isEmpty() || !QDir().mkpath(path)) {
        setError(QStringLiteral("The default image save location could not be created."));
        emit stateChanged();
        return false;
    }

    const QString validationError = outputFolderError(outputFolder_);
    if (!validationError.isEmpty()) {
        setError(validationError);
        emit stateChanged();
        return false;
    }
    return true;
}

QUrl SingleImageCaptureController::outputFolder() const
{
    return outputFolder_;
}

void SingleImageCaptureController::setOutputFolder(const QUrl &folder)
{
    if (capturing_ || outputFolder_ == folder)
        return;
    outputFolder_ = folder;
    clearOutcome();
    emit outputFolderChanged();
}

void SingleImageCaptureController::setOutputFolderPath(const QString &path)
{
    setOutputFolder(path.trimmed().isEmpty() ? QUrl{} : QUrl::fromLocalFile(path.trimmed()));
}

QString SingleImageCaptureController::fileName() const
{
    return fileName_;
}

void SingleImageCaptureController::setFileName(const QString &fileName)
{
    if (capturing_ || fileName_ == fileName)
        return;
    fileName_ = fileName;
    clearOutcome();
    emit fileNameChanged();
}

bool SingleImageCaptureController::canCapture() const
{
    return !capturing_
        && cameraController_.streaming()
        && outputFolderError(outputFolder_).isEmpty()
        && operations_.momentaryAvailable(ResourceLock::Camera | ResourceLock::Storage);
}

QString SingleImageCaptureController::disabledReason() const
{
    if (capturing_)
        return {};
    if (!cameraController_.streaming()) {
        return cameraController_.cameraStatus() == QStringLiteral("Unavailable")
            ? QStringLiteral("Camera unavailable")
            : QStringLiteral("Start Camera");
    }
    const QString folderError = outputFolderError(outputFolder_);
    if (!folderError.isEmpty())
        return folderError;
    if (!operations_.momentaryAvailable(ResourceLock::Camera | ResourceLock::Storage))
        return QStringLiteral("Another operation is active");
    return {};
}

QString SingleImageCaptureController::presentation() const
{
    if (capturing_)
        return QStringLiteral("capturing");
    if (!error_.isEmpty())
        return QStringLiteral("error");
    if (!savedArtifactUrl_.isEmpty())
        return QStringLiteral("completed");
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
    if (capturing_)
        return false;

    setSavedArtifactUrl({});
    setError({});

    const QString folderError = outputFolderError(outputFolder_);
    if (!folderError.isEmpty()) {
        setError(folderError);
        emit stateChanged();
        return false;
    }
    if (!cameraController_.streaming()) {
        setError(cameraController_.cameraStatus() == QStringLiteral("Unavailable")
                     ? QStringLiteral("No camera is connected.")
                     : QStringLiteral("The camera is not streaming."));
        emit stateChanged();
        return false;
    }

    auto acquisition =
        operations_.acquireMomentary(ResourceLock::Camera | ResourceLock::Storage);
    if (!acquisition.acquired()) {
        setError(acquisition.fault
                     ? operationConflict(*acquisition.fault)
                     : QStringLiteral("Camera or storage is in use by another operation."));
        emit stateChanged();
        return false;
    }

    lease_ = std::move(acquisition.lease);
    acceptedOutputFolder_ = outputFolder_.toLocalFile();
    acceptedFileName_ = fileName_;
    baselineHadFrame_ = cameraController_.hasFrame();
    baselineDeliveryId_ = cameraController_.latestDeliveryId();
    capturing_ = true;
    writing_ = false;
    frameTimeout_.start();
    emit stateChanged();
    return true;
}

void SingleImageCaptureController::acceptFrame(CameraFrame frame)
{
    if (!capturing_ || writing_
        || (baselineHadFrame_ && frame.deliveryId <= baselineDeliveryId_)) {
        return;
    }

    frameTimeout_.stop();
    writing_ = true;
    const QString saveDirectory = acceptedOutputFolder_;
    const QString requestedFileName = acceptedFileName_;
    SingleImageCaptureService *service = &captureService_;
    saveWatcher_.setFuture(QtConcurrent::run(
        [service, frame = std::move(frame), saveDirectory, requestedFileName]() {
            CaptureResult result;
            result.saved = service->save(frame, saveDirectory, requestedFileName,
                                         &result.path, &result.error);
            return result;
        }));
}

void SingleImageCaptureController::finishCapture()
{
    const CaptureResult result = saveWatcher_.result();
    capturing_ = false;
    writing_ = false;
    lease_.release();
    if (result.saved)
        setSavedArtifactUrl(QUrl::fromLocalFile(result.path));
    else
        setError(result.error.isEmpty()
                     ? QStringLiteral("The TIFF image could not be saved.")
                     : result.error);
    emit stateChanged();
}

void SingleImageCaptureController::timeoutWaitingForFrame()
{
    if (!capturing_ || writing_)
        return;
    capturing_ = false;
    lease_.release();
    setError(QStringLiteral("Camera did not provide a newer frame within 2 seconds."));
    emit stateChanged();
}

void SingleImageCaptureController::clearOutcome()
{
    if (capturing_)
        return;
    setError({});
    setSavedArtifactUrl({});
    emit stateChanged();
}

void SingleImageCaptureController::setError(const QString &error)
{
    if (error_ == error)
        return;
    error_ = error;
    emit errorChanged();
}

void SingleImageCaptureController::setSavedArtifactUrl(const QUrl &url)
{
    if (savedArtifactUrl_ == url)
        return;
    savedArtifactUrl_ = url;
    emit savedArtifactUrlChanged();
}

} // namespace desktop_app::v2
