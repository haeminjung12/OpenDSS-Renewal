#pragma once

#include "camera_device.h"

#include <QImage>
#include <QPair>

namespace desktop_app::v2 {

QImage convertCameraFrame(const CameraFrame &frame, QString *error = nullptr);
QImage applyLinearContrast(const QImage &image, int low, int high);
QPair<int, int> autoContrastRange(const QImage &image);

} // namespace desktop_app::v2
