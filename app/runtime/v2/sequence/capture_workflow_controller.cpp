#include "capture_workflow_controller.h"

#include "../camera/camera_controller.h"
#include "../camera/camera_service.h"
#include "../camera/frame_conversion.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <cmath>
#include <chrono>
#include <exception>
#include <utility>

namespace desktop_app::v2::sequence {
namespace {

bool isActive(OperationLifecycle lifecycle)
{
    return lifecycle == OperationLifecycle::Starting
        || lifecycle == OperationLifecycle::Running
        || lifecycle == OperationLifecycle::Paused
        || lifecycle == OperationLifecycle::Stopping;
}

QString defaultName(const QString &prefix)
{
    return prefix + QLatin1Char('-')
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
}

} // namespace

CaptureWorkflowController::CaptureWorkflowController(
    CameraService &cameraService,
    CameraController &cameraController,
    OperationCoordinator &operations,
    DropletFrameProcessor &processor,
    MonotonicNow monotonicNow,
    CameraSettingsProvider cameraSettingsProvider,
    QString opendssVersion,
    QObject *parent)
    : QObject(parent)
    , cameraService_(cameraService)
    , cameraController_(cameraController)
    , operations_(operations)
    , processor_(processor)
    , monotonicNow_(std::move(monotonicNow))
    , cameraSettingsProvider_(std::move(cameraSettingsProvider))
    , opendssVersion_(std::move(opendssVersion))
{
    pollTimer_.setInterval(100);
    connect(&pollTimer_, &QTimer::timeout, this, &CaptureWorkflowController::refresh);
    connect(&operations_, &OperationCoordinator::resourcesChanged,
            this, &CaptureWorkflowController::changed);
    connect(&cameraController_, &CameraController::frameReady,
            this, &CaptureWorkflowController::acceptFrame);
    pollTimer_.start();
}

CaptureWorkflowController::~CaptureWorkflowController()
{
    collectSequenceStop(true);
    QString ignored;
    if (sequenceService_ && isActive(sequenceService_->snapshot().lifecycle))
        sequenceService_->stop(&ignored);
    if (datasetService_ && isActive(datasetService_->snapshot().lifecycle))
        datasetService_->stop(&ignored);
}

QString CaptureWorkflowController::presentation(OperationLifecycle lifecycle)
{
    switch (lifecycle) {
    case OperationLifecycle::Idle: return QStringLiteral("ready");
    case OperationLifecycle::Starting: return QStringLiteral("starting");
    case OperationLifecycle::Running: return QStringLiteral("running");
    case OperationLifecycle::Paused: return QStringLiteral("paused");
    case OperationLifecycle::Stopping: return QStringLiteral("stopping");
    case OperationLifecycle::Completed: return QStringLiteral("completed");
    case OperationLifecycle::Interrupted:
    case OperationLifecycle::Failed: return QStringLiteral("error");
    }
    return QStringLiteral("error");
}

QString CaptureWorkflowController::sequencePresentation() const
{
    return sequenceService_ ? presentation(sequenceSnapshot_.lifecycle)
                            : QStringLiteral("ready");
}

QString CaptureWorkflowController::datasetPresentation() const
{
    return datasetService_ ? presentation(datasetSnapshot_.lifecycle)
                           : QStringLiteral("ready");
}

qint64 CaptureWorkflowController::sequenceFrameCount() const
{
    return sequenceSnapshot_.capturedFrameCount;
}

qint64 CaptureWorkflowController::sequenceFinalizedFrameCount() const
{
    return sequenceSnapshot_.savedFrameCount;
}

qint64 CaptureWorkflowController::datasetFrameCount() const
{
    return datasetSnapshot_.savedFrameCount;
}

qint64 CaptureWorkflowController::datasetCropCount() const
{
    return datasetSnapshot_.savedCropCount;
}

QString CaptureWorkflowController::sequenceLocation() const { return sequenceLocation_; }
QString CaptureWorkflowController::datasetLocation() const { return datasetLocation_; }
QString CaptureWorkflowController::sequenceFolder() const { return sequenceSnapshot_.folder; }
QString CaptureWorkflowController::datasetFolder() const { return datasetSnapshot_.folder; }

QString CaptureWorkflowController::sequenceError() const
{
    return sequenceActionError_.isEmpty() ? sequenceSnapshot_.error : sequenceActionError_;
}

QString CaptureWorkflowController::datasetError() const
{
    return datasetActionError_.isEmpty() ? datasetSnapshot_.error : datasetActionError_;
}

bool CaptureWorkflowController::captureActive() const
{
    return isActive(sequenceSnapshot_.lifecycle) || isActive(datasetSnapshot_.lifecycle);
}

bool CaptureWorkflowController::captureStartAvailable() const
{
    const OperationLifecycle lifecycle = operations_.snapshot().lifecycle;
    return !captureActive() && !sequenceStopFuture_.valid() &&
           lifecycle != OperationLifecycle::Starting &&
           lifecycle != OperationLifecycle::Running &&
           lifecycle != OperationLifecycle::Paused &&
           lifecycle != OperationLifecycle::Stopping;
}

void CaptureWorkflowController::setSequenceLocation(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path.trimmed());
    if (sequenceLocation_ == cleaned)
        return;
    sequenceLocation_ = cleaned;
    sequenceActionError_.clear();
    emit changed();
}

void CaptureWorkflowController::setDatasetLocation(const QString &path)
{
    const QString cleaned = QDir::cleanPath(path.trimmed());
    if (datasetLocation_ == cleaned)
        return;
    datasetLocation_ = cleaned;
    datasetActionError_.clear();
    emit changed();
}

std::optional<double> CaptureWorkflowController::parseDuration(const QString &text,
                                                               QString *error)
{
    if (error)
        error->clear();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;
    bool ok = false;
    const double value = trimmed.toDouble(&ok);
    if (!ok || !std::isfinite(value) || value <= 0.0) {
        if (error)
            *error = QStringLiteral("Duration must be blank or a positive number of seconds.");
        return std::nullopt;
    }
    return value;
}

bool CaptureWorkflowController::startSequence(const QString &name,
                                              const QString &experimentType,
                                              const QString &notes,
                                              const QString &duration)
{
    sequenceActionError_.clear();
    if (!captureStartAvailable()) {
        sequenceActionError_ = QStringLiteral("Another operation is active.");
        emit changed();
        return false;
    }
    if (!cameraController_.streaming()) {
        sequenceActionError_ = QStringLiteral("Start the camera before recording an Image Sequence.");
        emit changed();
        return false;
    }
    if (!QDir().mkpath(sequenceLocation_) || !QFileInfo(sequenceLocation_).isWritable()) {
        sequenceActionError_ = QStringLiteral("Choose a writable Image Sequence save location.");
        emit changed();
        return false;
    }
    QString durationError;
    const auto durationSeconds = parseDuration(duration, &durationError);
    if (!duration.trimmed().isEmpty() && !durationSeconds) {
        sequenceActionError_ = durationError;
        emit changed();
        return false;
    }

    sequenceService_ = std::make_unique<ImageSequenceCaptureService>(
        cameraService_, operations_, processor_, monotonicNow_);
    activeCaptureFps_ = stableNominalFps();
    ImageSequenceCaptureRequest request;
    request.saveRoot = sequenceLocation_;
    request.name = name.trimmed().isEmpty() ? defaultName(QStringLiteral("Sequence")) : name;
    request.experimentType = experimentType;
    request.notes = notes;
    request.durationSeconds = durationSeconds;
    request.opendssVersion = opendssVersion_;
    request.cameraSettings = cameraSettingsProvider_ ? cameraSettingsProvider_() : QJsonObject{};
    if (!sequenceService_->start(request, &sequenceActionError_)) {
        sequenceSnapshot_ = sequenceService_->snapshot();
        emit changed();
        return false;
    }
    sequenceSnapshot_ = sequenceService_->snapshot();
    emit changed();
    return true;
}

bool CaptureWorkflowController::pauseOrResumeSequence()
{
    if (!sequenceService_ || sequenceStopFuture_.valid())
        return false;
    sequenceActionError_.clear();
    const bool ok = sequenceSnapshot_.lifecycle == OperationLifecycle::Paused
        ? sequenceService_->resume(&sequenceActionError_)
        : sequenceService_->pause(&sequenceActionError_);
    refresh();
    return ok;
}

bool CaptureWorkflowController::stopSequence()
{
    return launchSequenceStop(false);
}

void CaptureWorkflowController::newSequence()
{
    if (isActive(sequenceSnapshot_.lifecycle) || sequenceStopFuture_.valid())
        return;
    sequenceService_.reset();
    sequenceSnapshot_ = {};
    sequenceActionError_.clear();
    emit changed();
}

bool CaptureWorkflowController::startDataset(const QString &name,
                                             const QString &experimentType,
                                             const QString &notes,
                                             const QString &duration,
                                             bool saveFullImageSequence)
{
    datasetActionError_.clear();
    if (!captureStartAvailable()) {
        datasetActionError_ = QStringLiteral("Another operation is active.");
        emit changed();
        return false;
    }
    if (!cameraController_.streaming()) {
        datasetActionError_ = QStringLiteral("Start the camera before capturing a Dataset.");
        emit changed();
        return false;
    }
    if (!QDir().mkpath(datasetLocation_) || !QFileInfo(datasetLocation_).isWritable()) {
        datasetActionError_ = QStringLiteral("Choose a writable Dataset save location.");
        emit changed();
        return false;
    }
    QString durationError;
    const auto durationSeconds = parseDuration(duration, &durationError);
    if (!duration.trimmed().isEmpty() && !durationSeconds) {
        datasetActionError_ = durationError;
        emit changed();
        return false;
    }

    datasetService_ = std::make_unique<dataset::DatasetCaptureService>(
        operations_, processor_, monotonicNow_);
    activeCaptureFps_ = stableNominalFps();
    dataset::DatasetCaptureRequest request;
    request.saveRoot = datasetLocation_;
    request.name = name.trimmed().isEmpty() ? defaultName(QStringLiteral("Dataset")) : name;
    request.experimentType = experimentType;
    request.notes = notes;
    request.durationSeconds = durationSeconds;
    request.saveFullImageSequence = saveFullImageSequence;
    request.opendssVersion = opendssVersion_;
    request.cameraSettings = cameraSettingsProvider_ ? cameraSettingsProvider_() : QJsonObject{};
    if (!datasetService_->start(request, &datasetActionError_)) {
        datasetSnapshot_ = datasetService_->snapshot();
        emit changed();
        return false;
    }
    datasetSnapshot_ = datasetService_->snapshot();
    emit changed();
    return true;
}

bool CaptureWorkflowController::pauseOrResumeDataset()
{
    if (!datasetService_)
        return false;
    datasetActionError_.clear();
    const bool ok = datasetSnapshot_.lifecycle == OperationLifecycle::Paused
        ? datasetService_->resume(&datasetActionError_)
        : datasetService_->pause(&datasetActionError_);
    refresh();
    return ok;
}

bool CaptureWorkflowController::stopDataset()
{
    if (!datasetService_)
        return false;
    datasetActionError_.clear();
    const bool ok = datasetService_->stop(&datasetActionError_);
    refresh();
    return ok;
}

void CaptureWorkflowController::newDataset()
{
    if (isActive(datasetSnapshot_.lifecycle))
        return;
    datasetService_.reset();
    datasetSnapshot_ = {};
    datasetActionError_.clear();
    emit changed();
}

void CaptureWorkflowController::acceptFrame(const CameraFrame &frame)
{
    if (previousTimestampNs_ > 0 && frame.monotonicTimestampNs > previousTimestampNs_) {
        const double fps = 1'000'000'000.0
            / static_cast<double>(frame.monotonicTimestampNs - previousTimestampNs_);
        if (std::isfinite(fps) && fps > 0.0)
            estimatedFps_ = fps;
    }
    previousTimestampNs_ = frame.monotonicTimestampNs;

    QString error;
    if (sequenceService_ && !sequenceStopFuture_.valid() &&
        sequenceSnapshot_.lifecycle == OperationLifecycle::Running) {
        if (!sequenceService_->offerFrame(frame, activeCaptureFps_, &error))
            sequenceActionError_ = error;
    } else if (datasetService_ && datasetSnapshot_.lifecycle == OperationLifecycle::Running) {
        QImage image = convertCameraFrame(frame, &error);
        FrameMeta meta;
        meta.width = frame.width;
        meta.height = frame.height;
        meta.bits = 8;
        meta.frameIndex = static_cast<qint64>(frame.deliveryId);
        meta.delivered = static_cast<qint64>(frame.deliveryId);
        meta.internalFps = activeCaptureFps_;
        if (image.isNull()
            || !datasetService_->offerFrame(image, meta, activeCaptureFps_, &error)) {
            datasetActionError_ = error;
        }
    }
}

bool CaptureWorkflowController::launchSequenceStop(bool durationExpired)
{
    if (!sequenceService_ || sequenceStopFuture_.valid() ||
        (sequenceSnapshot_.lifecycle != OperationLifecycle::Running &&
         sequenceSnapshot_.lifecycle != OperationLifecycle::Paused)) {
        return false;
    }
    sequenceActionError_.clear();
    ImageSequenceCaptureService *service = sequenceService_.get();
    try {
        sequenceStopFuture_ = std::async(std::launch::async, [service, durationExpired] {
            SequenceStopResult result;
            result.ok = durationExpired ? service->stopForDuration(&result.error)
                                        : service->stop(&result.error);
            return result;
        });
    } catch (const std::exception &exception) {
        sequenceActionError_ = QStringLiteral("Could not start Image Sequence finalization: %1")
                                   .arg(QString::fromUtf8(exception.what()));
        emit changed();
        return false;
    }
    emit changed();
    return true;
}

void CaptureWorkflowController::collectSequenceStop(bool wait)
{
    if (!sequenceStopFuture_.valid())
        return;
    if (!wait && sequenceStopFuture_.wait_for(std::chrono::seconds(0)) !=
                     std::future_status::ready) {
        return;
    }
    const SequenceStopResult result = sequenceStopFuture_.get();
    if (!result.ok)
        sequenceActionError_ = result.error;
}

void CaptureWorkflowController::refresh()
{
    collectSequenceStop(false);
    if (sequenceService_) {
        sequenceSnapshot_ = sequenceService_->snapshot();
        if (sequenceSnapshot_.lifecycle == OperationLifecycle::Running &&
            !sequenceStopFuture_.valid() && sequenceService_->durationExpired()) {
            launchSequenceStop(true);
            sequenceSnapshot_ = sequenceService_->snapshot();
        }
    }
    if (datasetService_) {
        if (datasetSnapshot_.lifecycle == OperationLifecycle::Running) {
            QString error;
            if (!datasetService_->pollDuration(&error))
                datasetActionError_ = error;
        }
        datasetSnapshot_ = datasetService_->snapshot();
    }
    emit changed();
}

double CaptureWorkflowController::stableNominalFps() const
{
    return std::isfinite(estimatedFps_) && estimatedFps_ > 0.0 ? estimatedFps_ : 100.0;
}

} // namespace desktop_app::v2::sequence
