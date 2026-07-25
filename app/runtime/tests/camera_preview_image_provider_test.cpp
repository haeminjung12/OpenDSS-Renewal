#include "v2/camera/camera_preview_image_provider.h"

#include <QCoreApplication>
#include <QDebug>

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    CameraPreviewImageProvider provider;
    CameraFrame frame;
    frame.pixelFormat = CameraPixelFormat::Mono8;
    frame.width = 2;
    frame.height = 1;
    frame.rowBytes = 2;
    frame.bitDepth = 8;
    frame.deliveryId = 1;
    frame.monotonicTimestampNs = 100;
    frame.bytes = QByteArray::fromHex("1020");

    ok &= check(provider.updateFrame(frame) == 1,
                "The first immutable preview frame must receive revision one.");
    frame.bytes[0] = static_cast<char>(0x7f);
    QSize sourceSize;
    const QImage image = provider.requestImage(
        QStringLiteral("frame?r=1"), &sourceSize, QSize(4, 4));
    ok &= check(sourceSize == QSize(2, 1) && image.size() == QSize(4, 2)
                    && image.constScanLine(0)[0] == static_cast<uchar>(0x10),
                "Preview requests must use the deep-owned frame and requested size.");
    ok &= check(provider.requestImage(QStringLiteral("unknown"), nullptr, {}).isNull(),
                "Unknown provider paths must not return stale preview content.");

    frame.deliveryId = 2;
    ok &= check(provider.updateFrame(frame) == 2,
                "Each accepted frame must advance the preview revision exactly once.");
    return ok ? 0 : 1;
}
