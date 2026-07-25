#include "camera_controller.h"

#include "camera_preview_image_provider.h"
#include "camera_service.h"

namespace desktop_app::v2 {
namespace {

QString statusText(int status)
{
    switch (static_cast<CameraStatus>(status)) {
    case CameraStatus::Unavailable:
        return QStringLiteral("Unavailable");
    case CameraStatus::Ready:
        return QStringLiteral("Connected");
    case CameraStatus::Streaming:
        return QStringLiteral("Streaming");
    case CameraStatus::Faulted:
        return QStringLiteral("Unavailable");
    }
    return QStringLiteral("Unavailable");
}

} // namespace

CameraController::CameraController(CameraService &service,
                                   CameraPreviewImageProvider &previewProvider,
                                   QObject *parent)
    : QObject(parent)
    , service_(service)
    , previewProvider_(previewProvider)
    , status_(static_cast<int>(CameraStatus::Unavailable))
{
    connect(this, &CameraController::openRequested,
            &service_, &CameraService::open, Qt::QueuedConnection);
    connect(this, &CameraController::startRequested,
            &service_, &CameraService::start, Qt::QueuedConnection);
    connect(this, &CameraController::stopRequested,
            &service_, &CameraService::stop, Qt::QueuedConnection);
    connect(this, &CameraController::recoverRequested,
            &service_, &CameraService::recover, Qt::QueuedConnection);
    connect(this, &CameraController::closeRequested,
            &service_, &CameraService::close, Qt::QueuedConnection);
    connect(&service_, &CameraService::stateChanged,
            this, &CameraController::updateState, Qt::QueuedConnection);
    connect(&service_, &CameraService::frameReady,
            this, &CameraController::updateFrame, Qt::QueuedConnection);
    connect(&service_, &CameraService::frameError,
            this, &CameraController::setError, Qt::QueuedConnection);
    connect(&service_, &CameraService::commandFinished, this,
            [this](bool, const QString &error) {
                setError(error);
                setBusy(false);
            },
            Qt::QueuedConnection);
}

QString CameraController::cameraStatus() const
{
    return statusText(status_);
}

QString CameraController::deviceId() const
{
    return deviceId_;
}

QString CameraController::error() const
{
    return serviceFault_.isEmpty() ? actionError_ : serviceFault_;
}

bool CameraController::streaming() const
{
    return static_cast<CameraStatus>(status_) == CameraStatus::Streaming;
}

bool CameraController::busy() const
{
    return busy_;
}

QString CameraController::previewSource() const
{
    return previewSource_;
}

bool CameraController::hasFrame() const
{
    return hasFrame_;
}

quint64 CameraController::latestDeliveryId() const
{
    return latestDeliveryId_;
}

bool CameraController::open()
{
    return request(&CameraController::openRequested);
}

bool CameraController::start()
{
    return request(&CameraController::startRequested);
}

bool CameraController::stop()
{
    return request(&CameraController::stopRequested);
}

bool CameraController::recover()
{
    return request(&CameraController::recoverRequested);
}

bool CameraController::close()
{
    return request(&CameraController::closeRequested);
}

bool CameraController::request(void (CameraController::*signal)())
{
    if (busy_)
        return false;
    setError({});
    setBusy(true);
    emit (this->*signal)();
    return true;
}

void CameraController::updateState(int status, const QString &deviceId,
                                   const QString &fault)
{
    const bool unavailable =
        static_cast<CameraStatus>(status) == CameraStatus::Unavailable
        || static_cast<CameraStatus>(status) == CameraStatus::Faulted;
    const bool projectionChanged =
        status_ != status || deviceId_ != deviceId || serviceFault_ != fault;
    status_ = status;
    deviceId_ = deviceId;
    serviceFault_ = fault;
    if (unavailable) {
        hasFrame_ = false;
        latestDeliveryId_ = 0;
        if (!previewSource_.isEmpty()) {
            previewSource_.clear();
            emit previewSourceChanged();
        }
    }
    if (projectionChanged) {
        emit stateChanged();
        emit errorChanged();
    }
}

void CameraController::updateFrame(CameraFrame frame)
{
    setError({});
    latestDeliveryId_ = frame.deliveryId;
    hasFrame_ = true;
    const quint64 revision = previewProvider_.updateFrame(frame);
    previewSource_ =
        QStringLiteral("image://camera-preview/frame?r=%1").arg(revision);
    emit previewSourceChanged();
    emit frameReady(std::move(frame));
}

void CameraController::setError(const QString &error)
{
    if (actionError_ == error) {
        return;
    }
    actionError_ = error;
    emit errorChanged();
}

void CameraController::setBusy(bool busy)
{
    if (busy_ == busy)
        return;
    busy_ = busy;
    emit busyChanged();
    emit stateChanged();
}

} // namespace desktop_app::v2
