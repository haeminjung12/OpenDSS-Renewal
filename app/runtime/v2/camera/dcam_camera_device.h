#pragma once

#include "camera_device.h"

#include <memory>

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
    CameraFrameResult latestFrame(CameraFrame &frame, QString *error) override;

private:
    std::unique_ptr<DcamCamera> camera_;
};

} // namespace desktop_app::v2
