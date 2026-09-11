#include "camera_preview_image_provider.h"

#include "frame_conversion.h"

#include <QMutexLocker>

namespace desktop_app::v2 {

CameraPreviewImageProvider::CameraPreviewImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

quint64 CameraPreviewImageProvider::updateFrame(CameraFrame frame)
{
    frame.bytes = QByteArray(frame.bytes.constData(), frame.bytes.size());
    QMutexLocker locker(&mutex_);
    latestFrame_ = std::move(frame);
    hasFrame_ = true;
    return ++revision_;
}

QImage CameraPreviewImageProvider::requestImage(const QString &id, QSize *size,
                                                const QSize &requestedSize)
{
    if (id.section(QLatin1Char('?'), 0, 0) != QStringLiteral("frame")) {
        if (size)
            *size = {};
        return {};
    }

    CameraFrame frame;
    {
        QMutexLocker locker(&mutex_);
        if (!hasFrame_) {
            if (size)
                *size = {};
            return {};
        }
        frame = latestFrame_;
    }

    QImage image = convertCameraFrame(frame);
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.width() > 0 && requestedSize.height() > 0)
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace desktop_app::v2
