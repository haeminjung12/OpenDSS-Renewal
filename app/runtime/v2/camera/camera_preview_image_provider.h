#pragma once

#include "camera_device.h"

#include <QMutex>
#include <QQuickImageProvider>

namespace desktop_app::v2 {

class CameraPreviewImageProvider final : public QQuickImageProvider
{
public:
    CameraPreviewImageProvider();

    quint64 updateFrame(CameraFrame frame);
    quint64 setPreviewLutRange(int blackLevel, int whiteLevel);
    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    QMutex mutex_;
    CameraFrame latestFrame_;
    bool hasFrame_ = false;
    int blackLevel_ = 0;
    int whiteLevel_ = 255;
    quint64 revision_ = 0;
};

} // namespace desktop_app::v2
