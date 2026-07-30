#pragma once

#include "camera_device.h"

#include <QString>

namespace desktop_app::v2 {

class SingleImageCaptureService final
{
public:
    bool save(const CameraFrame &frame,
              const QString &saveDirectory,
              const QString &requestedFileName,
              QString *savedPath,
              QString *error) const;
};

} // namespace desktop_app::v2
