#include "frame_conversion.h"

#include <cstring>
#include <limits>

namespace desktop_app::v2 {
namespace {

QImage fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return {};
}

} // namespace

QImage convertCameraFrame(const CameraFrame &frame, QString *error)
{
    if (frame.width <= 0 || frame.height <= 0) {
        return fail(error, QStringLiteral("The camera frame dimensions are invalid."));
    }

    const int bytesPerPixel = frame.pixelFormat == CameraPixelFormat::Mono8 ? 1 : 2;
    if ((frame.pixelFormat == CameraPixelFormat::Mono8 && frame.bitDepth != 8)
        || (frame.pixelFormat == CameraPixelFormat::Mono16
            && (frame.bitDepth < 9 || frame.bitDepth > 16))) {
        return fail(error, QStringLiteral("The camera frame bit depth does not match its pixel format."));
    }

    if (frame.width > std::numeric_limits<int>::max() / bytesPerPixel) {
        return fail(error, QStringLiteral("The camera frame row is too large."));
    }
    const int activeRowBytes = frame.width * bytesPerPixel;
    if (frame.rowBytes < activeRowBytes) {
        return fail(error, QStringLiteral("The camera frame row stride is too small."));
    }
    if (frame.rowBytes > 0
        && frame.height > std::numeric_limits<qsizetype>::max() / frame.rowBytes) {
        return fail(error, QStringLiteral("The camera frame byte count is too large."));
    }
    const qsizetype requiredBytes =
        static_cast<qsizetype>(frame.rowBytes) * static_cast<qsizetype>(frame.height);
    if (frame.bytes.size() < requiredBytes) {
        return fail(error, QStringLiteral("The camera frame data is incomplete."));
    }

    const QImage::Format format = frame.pixelFormat == CameraPixelFormat::Mono8
        ? QImage::Format_Grayscale8
        : QImage::Format_Grayscale16;
    QImage owned(frame.width, frame.height, format);
    if (owned.isNull()) {
        return fail(error, QStringLiteral("The camera frame image could not be allocated."));
    }

    for (int row = 0; row < frame.height; ++row) {
        const char *source = frame.bytes.constData() + static_cast<qsizetype>(row) * frame.rowBytes;
        std::memcpy(owned.scanLine(row), source, static_cast<size_t>(activeRowBytes));
    }

    if (error) {
        error->clear();
    }
    if (frame.pixelFormat == CameraPixelFormat::Mono8) {
        return owned;
    }
    return owned.convertToFormat(QImage::Format_Grayscale8);
}

} // namespace desktop_app::v2
