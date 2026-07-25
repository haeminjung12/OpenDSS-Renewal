#include "sequence_viewer_image_provider.h"

#include "sequence_viewer_controller.h"

namespace desktop_app::v2::sequence {

SequenceViewerImageProvider::SequenceViewerImageProvider(const SequenceViewerController& controller)
    : QQuickImageProvider(Image)
    , controller_(controller) {}

QImage SequenceViewerImageProvider::requestImage(const QString& id,
                                                  QSize* size,
                                                  const QSize& requestedSize) {
    Q_UNUSED(id)
    QImage image = controller_.currentImage();
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.isValid())
        image = image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return image;
}

} // namespace desktop_app::v2::sequence
