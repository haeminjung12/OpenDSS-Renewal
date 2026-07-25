#include "camera_service.h"

#include "../state/application_state_store.h"

namespace desktop_app::v2 {
namespace {

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString factualError(const QString &action, const QString &deviceError)
{
    if (!deviceError.trimmed().isEmpty()) {
        return deviceError.trimmed();
    }
    return QStringLiteral("The camera could not %1.").arg(action);
}

} // namespace

CameraService::CameraService(std::unique_ptr<ICameraDevice> device,
                             ApplicationStateStore &stateStore)
    : device_(std::move(device))
    , stateStore_(stateStore)
{
    publish(CameraStatus::Unavailable);
}

bool CameraService::open(QString *error)
{
    if (state_.status != CameraStatus::Unavailable) {
        setError(error, QStringLiteral("The camera can only be opened while it is unavailable."));
        return false;
    }

    if (!device_) {
        const QString message = QStringLiteral("No camera device is available.");
        publish(CameraStatus::Faulted, message);
        setError(error, message);
        return false;
    }

    QString deviceError;
    if (!device_->open(&deviceError)) {
        const QString message = factualError(QStringLiteral("open"), deviceError);
        publish(CameraStatus::Faulted, message);
        setError(error, message);
        return false;
    }

    lastDeliveryId_.reset();
    lastTimestampNs_.reset();
    publish(CameraStatus::Ready);
    setError(error, {});
    return true;
}

bool CameraService::start(QString *error)
{
    if (state_.status != CameraStatus::Ready) {
        const QString message = QStringLiteral("The camera must be ready before streaming can start.");
        setError(error, message);
        return false;
    }

    QString deviceError;
    if (!device_->start(&deviceError)) {
        const QString message = factualError(QStringLiteral("start streaming"), deviceError);
        publish(CameraStatus::Faulted, message);
        setError(error, message);
        return false;
    }

    lastDeliveryId_.reset();
    lastTimestampNs_.reset();
    publish(CameraStatus::Streaming);
    setError(error, {});
    return true;
}

bool CameraService::stop(QString *error)
{
    if (state_.status != CameraStatus::Streaming) {
        const QString message = QStringLiteral("The camera is not streaming.");
        setError(error, message);
        return false;
    }

    QString deviceError;
    if (!device_->stop(&deviceError)) {
        const QString message = factualError(QStringLiteral("stop streaming"), deviceError);
        publish(CameraStatus::Faulted, message);
        setError(error, message);
        return false;
    }

    publish(CameraStatus::Ready);
    setError(error, {});
    return true;
}

std::optional<CameraFrame> CameraService::latestOwnedFrame(QString *error)
{
    if (state_.status != CameraStatus::Streaming) {
        setError(error, QStringLiteral("The camera is not streaming."));
        return std::nullopt;
    }

    CameraFrame frame;
    QString deviceError;
    if (!device_->latestFrame(frame, &deviceError)) {
        setError(error, factualError(QStringLiteral("provide a current frame"), deviceError));
        return std::nullopt;
    }

    if (lastDeliveryId_ && frame.deliveryId < *lastDeliveryId_) {
        setError(error, QStringLiteral("The camera returned an older frame delivery identifier."));
        return std::nullopt;
    }
    if (lastTimestampNs_ && frame.monotonicTimestampNs < *lastTimestampNs_) {
        setError(error, QStringLiteral("The camera returned an older frame timestamp."));
        return std::nullopt;
    }

    frame.bytes = QByteArray(frame.bytes.constData(), frame.bytes.size());
    lastDeliveryId_ = frame.deliveryId;
    lastTimestampNs_ = frame.monotonicTimestampNs;
    setError(error, {});
    return frame;
}

CameraState CameraService::state() const
{
    return state_;
}

void CameraService::publish(CameraStatus status, const QString &fault)
{
    state_.status = status;
    state_.deviceId = device_ ? device_->deviceId() : QString();
    state_.fault = fault;
    stateStore_.publishCamera(state_);
}

} // namespace desktop_app::v2
