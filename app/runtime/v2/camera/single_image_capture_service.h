#pragma once

#include <QString>

namespace desktop_app::v2 {

class CameraService;
class OperationCoordinator;

class SingleImageCaptureService final
{
public:
    SingleImageCaptureService(CameraService &camera, OperationCoordinator &operations);

    bool capture(const QString &saveDirectory,
                 const QString &requestedFileName,
                 QString *savedPath,
                 QString *error);

private:
    CameraService &camera_;
    OperationCoordinator &operations_;
};

} // namespace desktop_app::v2
