#pragma once

#include "camera_device.h"

#include <memory>
#include <optional>

class DcamCamera;

namespace desktop_app::v2 {

class DcamCameraDevice final : public ICameraDevice
{
public:
    DcamCameraDevice();
    ~DcamCameraDevice() override;

    QString deviceId() const override;
    bool open(QString *error) override;
    bool start(QString *error) override;
    bool stop(QString *error) override;
    bool close(QString *error) override;
    CameraFrameResult drainFrames(std::vector<CameraFrame> &frames,
                                  QString *error) override;
    CameraConfigurationSupport configurationSupport(QString *error) const override;
    bool readConfiguration(CameraAppliedSettings &settings, QString *error) override;
    CameraConfigurationResult applyConfiguration(
        const CameraAppliedSettings &requested,
        CameraAppliedSettings &applied,
        QString *error) override;

private:
    std::unique_ptr<DcamCamera> camera_;
    std::optional<quint64> lastTimestampDeliveryId_;
    std::optional<qint64> lastAcquisitionTimestampNs_;
};

} // namespace desktop_app::v2

