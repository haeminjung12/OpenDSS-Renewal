#pragma once

#include "camera_device.h"
#include "../state/domain_state.h"

#include <memory>
#include <optional>

namespace desktop_app::v2 {

class ApplicationStateStore;

class CameraService final
{
public:
    CameraService(std::unique_ptr<ICameraDevice> device, ApplicationStateStore &stateStore);

    bool open(QString *error = nullptr);
    bool start(QString *error = nullptr);
    bool stop(QString *error = nullptr);
    std::optional<CameraFrame> latestOwnedFrame(QString *error = nullptr);

    CameraState state() const;

private:
    void publish(CameraStatus status, const QString &fault = {});

    std::unique_ptr<ICameraDevice> device_;
    ApplicationStateStore &stateStore_;
    CameraState state_;
    std::optional<quint64> lastDeliveryId_;
    std::optional<qint64> lastTimestampNs_;
};

} // namespace desktop_app::v2
