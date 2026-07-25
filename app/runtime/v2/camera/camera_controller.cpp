#include "camera_controller.h"

#include "camera_service.h"

namespace desktop_app::v2 {
namespace {

QString statusText(CameraStatus status)
{
    switch (status) {
    case CameraStatus::Unavailable:
        return QStringLiteral("Unavailable");
    case CameraStatus::Ready:
        return QStringLiteral("Ready");
    case CameraStatus::Streaming:
        return QStringLiteral("Streaming");
    case CameraStatus::Faulted:
        return QStringLiteral("Faulted");
    }
    return QStringLiteral("Unavailable");
}

} // namespace

CameraController::CameraController(CameraService &service, QObject *parent)
    : QObject(parent)
    , service_(service)
{
    QString openError;
    if (!service_.open(&openError)) {
        setError(openError);
    }
}

QString CameraController::cameraStatus() const
{
    return statusText(service_.state().status);
}

QString CameraController::deviceId() const
{
    return service_.state().deviceId;
}

QString CameraController::error() const
{
    return service_.state().fault.isEmpty() ? actionError_ : service_.state().fault;
}

bool CameraController::streaming() const
{
    return service_.state().status == CameraStatus::Streaming;
}

bool CameraController::start()
{
    QString serviceError;
    const bool started = service_.start(&serviceError);
    setError(serviceError);
    emit stateChanged();
    return started;
}

bool CameraController::stop()
{
    QString serviceError;
    const bool stopped = service_.stop(&serviceError);
    setError(serviceError);
    emit stateChanged();
    return stopped;
}

bool CameraController::recover()
{
    QString serviceError;
    const bool recovered = service_.recover(&serviceError);
    setError(serviceError);
    emit stateChanged();
    return recovered;
}

void CameraController::setError(const QString &error)
{
    if (actionError_ == error) {
        return;
    }
    actionError_ = error;
    emit errorChanged();
}

} // namespace desktop_app::v2
