#include "camera_service.h"

#include "../state/application_state_store.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

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
    : QObject(nullptr)
    , device_(std::move(device))
    , stateStore_(stateStore)
    , pollTimer_(new QTimer(this))
{
    qRegisterMetaType<CameraFrame>();
    pollTimer_->setInterval(16);
    pollTimer_->setTimerType(Qt::PreciseTimer);
    connect(pollTimer_, &QTimer::timeout, this, &CameraService::pollFrame);
    publish(CameraStatus::Unavailable);
}

CameraService::~CameraService()
{
    pollTimer_->stop();
    QString ignored;
    closeDevice(&ignored);
}

void CameraService::open()
{
    QString error;
    const bool succeeded = openDevice(&error);
    emit commandFinished(succeeded, error);
}

bool CameraService::openDevice(QString *error)
{
    if (state().status != CameraStatus::Unavailable) {
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
        QString ignored;
        device_->close(&ignored);
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

void CameraService::start()
{
    QString error;
    bool succeeded = false;
    if (state().status != CameraStatus::Ready) {
        const QString message = QStringLiteral("The camera must be ready before streaming can start.");
        setError(&error, message);
    } else {
        QString deviceError;
        if (!device_->start(&deviceError)) {
            const QString message = factualError(QStringLiteral("start streaming"), deviceError);
            QString ignored;
            device_->close(&ignored);
            publish(CameraStatus::Faulted, message);
            setError(&error, message);
        } else {
            lastDeliveryId_.reset();
            lastTimestampNs_.reset();
            publish(CameraStatus::Streaming);
            pollTimer_->start();
            succeeded = true;
        }
    }
    emit commandFinished(succeeded, error);
}

void CameraService::stop()
{
    QString error;
    bool succeeded = false;
    if (state().status != CameraStatus::Streaming) {
        const QString message = QStringLiteral("The camera is not streaming.");
        setError(&error, message);
    } else {
        pollTimer_->stop();
        QString deviceError;
        if (!device_->stop(&deviceError)) {
            const QString message = factualError(QStringLiteral("stop streaming"), deviceError);
            QString ignored;
            device_->close(&ignored);
            publish(CameraStatus::Faulted, message);
            setError(&error, message);
        } else {
            publish(CameraStatus::Ready);
            succeeded = true;
        }
    }
    emit commandFinished(succeeded, error);
}

void CameraService::close()
{
    QString error;
    const bool succeeded = closeDevice(&error);
    emit commandFinished(succeeded, error);
}

bool CameraService::closeDevice(QString *error)
{
    pollTimer_->stop();
    if (!device_) {
        lastDeliveryId_.reset();
        lastTimestampNs_.reset();
        publish(CameraStatus::Unavailable);
        setError(error, {});
        return true;
    }

    QString firstError;
    if (state().status == CameraStatus::Streaming) {
        QString stopError;
        if (!device_->stop(&stopError)) {
            firstError = factualError(QStringLiteral("stop streaming"), stopError);
        }
    }

    QString closeError;
    if (!device_->close(&closeError) && firstError.isEmpty()) {
        firstError = factualError(QStringLiteral("close"), closeError);
    }

    lastDeliveryId_.reset();
    lastTimestampNs_.reset();
    if (!firstError.isEmpty()) {
        publish(CameraStatus::Faulted, firstError);
        setError(error, firstError);
        return false;
    }

    publish(CameraStatus::Unavailable);
    setError(error, {});
    return true;
}

void CameraService::recover()
{
    QString error;
    QString closeError;
    bool succeeded = closeDevice(&closeError);
    if (!succeeded) {
        error = closeError;
    } else {
        succeeded = openDevice(&error);
    }
    emit commandFinished(succeeded, error);
}

void CameraService::pollFrame()
{
    CameraFrame frame;
    QString deviceError;
    const CameraFrameResult result = device_->latestFrame(frame, &deviceError);
    if (result == CameraFrameResult::NoFrame)
        return;
    if (result == CameraFrameResult::Error) {
        pollTimer_->stop();
        const QString message =
            factualError(QStringLiteral("provide a current frame"), deviceError);
        QString ignored;
        device_->stop(&ignored);
        device_->close(&ignored);
        lastDeliveryId_.reset();
        lastTimestampNs_.reset();
        publish(CameraStatus::Faulted, message);
        emit frameError(message);
        return;
    }

    if (lastDeliveryId_ && frame.deliveryId < *lastDeliveryId_) {
        emit frameError(QStringLiteral("The camera returned an older frame delivery identifier."));
        return;
    }
    if (lastTimestampNs_ && frame.monotonicTimestampNs < *lastTimestampNs_) {
        emit frameError(QStringLiteral("The camera returned an older frame timestamp."));
        return;
    }

    frame.bytes = QByteArray(frame.bytes.constData(), frame.bytes.size());
    lastDeliveryId_ = frame.deliveryId;
    lastTimestampNs_ = frame.monotonicTimestampNs;
    emit frameReady(std::move(frame));
}

CameraState CameraService::state() const
{
    QMutexLocker locker(&stateMutex_);
    return state_;
}

void CameraService::publish(CameraStatus status, const QString &fault)
{
    CameraState published;
    {
        QMutexLocker locker(&stateMutex_);
        state_.status = status;
        state_.deviceId = device_ ? device_->deviceId() : QString();
        state_.fault = fault;
        published = state_;
    }

    if (stateStore_.thread() == QThread::currentThread()) {
        stateStore_.publishCamera(published);
    } else {
        QMetaObject::invokeMethod(
            &stateStore_,
            [store = &stateStore_, published]() { store->publishCamera(published); },
            Qt::QueuedConnection);
    }
    emit stateChanged(static_cast<int>(published.status), published.deviceId, published.fault);
}

} // namespace desktop_app::v2
