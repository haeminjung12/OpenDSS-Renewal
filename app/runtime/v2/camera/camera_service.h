#pragma once

#include "camera_device.h"
#include "../state/domain_state.h"

#include <QMutex>
#include <QObject>
#include <QTimer>

#include <memory>
#include <optional>

namespace desktop_app::v2 {

class ApplicationStateStore;

class CameraService final : public QObject
{
    Q_OBJECT

public:
    CameraService(std::unique_ptr<ICameraDevice> device, ApplicationStateStore &stateStore);
    ~CameraService() override;

    CameraState state() const;

public slots:
    void open();
    void start();
    void stop();
    void close();
    void recover();
    void applyConfiguration(desktop_app::v2::CameraAppliedSettings requested);

signals:
    void stateChanged(int status, const QString &deviceId, const QString &fault);
    void frameReady(desktop_app::v2::CameraFrame frame);
    void frameError(const QString &error);
    void commandFinished(bool success, const QString &error);
    void configurationChanged(bool available,
                              desktop_app::v2::CameraAppliedSettings appliedSettings);

private:
    bool openDevice(QString *error);
    bool closeDevice(QString *error);
    void drainFrames();
    void publish(CameraStatus status, const QString &fault = {});

    std::unique_ptr<ICameraDevice> device_;
    ApplicationStateStore &stateStore_;
    QTimer *pollTimer_ = nullptr;
    mutable QMutex stateMutex_;
    CameraState state_;
    std::optional<quint64> lastDeliveryId_;
    std::optional<qint64> lastTimestampNs_;
};

} // namespace desktop_app::v2

