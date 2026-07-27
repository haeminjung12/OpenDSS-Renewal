#include "sequence_test_image_provider.h"

#include "sequence_test_controller.h"

namespace desktop_app::v2::sequence_test {

SequenceTestImageProvider::SequenceTestImageProvider(
    const SequenceTestController& controller)
    : QQuickImageProvider(Image), controller_(controller) {}

QImage SequenceTestImageProvider::requestImage(const QString& id,
                                               QSize* size,
                                               const QSize& requestedSize) {
    if (id.section(QLatin1Char('?'), 0, 0) != QStringLiteral("frame")) {
        if (size)
            *size = {};
        return {};
    }

    QImage image = controller_.currentPreviewImage();
    if (size)
        *size = image.size();
    if (!image.isNull() && requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image;
}

} // namespace desktop_app::v2::sequence_test
