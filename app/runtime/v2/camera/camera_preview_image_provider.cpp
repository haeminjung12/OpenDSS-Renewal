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

quint64 CameraPreviewImageProvider::setPreviewLutRange(int blackLevel,
                                                       int whiteLevel)
{
    QMutexLocker locker(&mutex_);
    if (blackLevel_ == blackLevel && whiteLevel_ == whiteLevel)
        return revision_;
    blackLevel_ = blackLevel;
    whiteLevel_ = whiteLevel;
    return ++revision_;
}

std::optional<CameraFrame> CameraPreviewImageProvider::latestFrame() const
{
    QMutexLocker locker(&mutex_);
    if (!hasFrame_)
        return std::nullopt;
    return latestFrame_;
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
    int blackLevel = 0;
    int whiteLevel = 255;
    {
        QMutexLocker locker(&mutex_);
        if (!hasFrame_) {
            if (size)
                *size = {};
            return {};
        }
        frame = latestFrame_;
        blackLevel = blackLevel_;
        whiteLevel = whiteLevel_;
    }

    QImage image = applyLinearPreviewLut(
        convertCameraFrame(frame), blackLevel, whiteLevel);
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.width() > 0 && requestedSize.height() > 0)
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace desktop_app::v2
