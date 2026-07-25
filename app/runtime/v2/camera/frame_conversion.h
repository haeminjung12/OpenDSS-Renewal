#pragma once

#include "camera_device.h"

#include <QImage>

namespace desktop_app::v2 {

QImage convertCameraFrame(const CameraFrame &frame, QString *error = nullptr);

} // namespace desktop_app::v2
