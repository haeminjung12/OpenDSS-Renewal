#pragma once

#include "camera_device.h"

#include <QImage>

namespace desktop_app::v2 {

QImage convertCameraFrame(const CameraFrame &frame, QString *error = nullptr);
QImage applyLinearPreviewLut(const QImage &image, int blackLevel, int whiteLevel);

} // namespace desktop_app::v2
