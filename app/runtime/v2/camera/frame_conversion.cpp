#include "frame_conversion.h"

#include <algorithm>
#include <array>
#include <cmath>
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

QImage applyLinearContrast(const QImage &image, int low, int high)
{
    if (image.isNull())
        return image;

    low = std::clamp(low, 0, 255);
    high = std::clamp(high, 0, 255);
    if ((low == 0 && high == 255) || high <= low)
        return image;

    QImage output = image.format() == QImage::Format_Grayscale8
        ? image.copy()
        : image.convertToFormat(QImage::Format_Grayscale8);
    std::array<uchar, 256> lookup{};
    for (int value = 0; value < 256; ++value) {
        lookup[value] = static_cast<uchar>(
            value <= low ? 0
            : value >= high ? 255
            : (value - low) * 255 / (high - low));
    }
    for (int y = 0; y < output.height(); ++y) {
        uchar *row = output.scanLine(y);
        for (int x = 0; x < output.width(); ++x)
            row[x] = lookup[row[x]];
    }
    return output;
}

QPair<int, int> autoContrastRange(const QImage &image)
{
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
        return {0, 255};

    const QImage grayscale = image.format() == QImage::Format_Grayscale8
        ? image : image.convertToFormat(QImage::Format_Grayscale8);
    std::array<qsizetype, 256> histogram{};
    qsizetype count = 0;
    for (int y = 0; y < grayscale.height(); ++y) {
        const uchar *row = grayscale.constScanLine(y);
        for (int x = 0; x < grayscale.width(); ++x) {
            ++histogram[row[x]];
            ++count;
        }
    }
    if (count == 0)
        return {0, 255};

    const auto percentile = [&histogram, count](double fraction) {
        const qsizetype target = std::max<qsizetype>(1, static_cast<qsizetype>(std::ceil(count * fraction)));
        qsizetype cumulative = 0;
        for (int value = 0; value < 256; ++value) {
            cumulative += histogram[value];
            if (cumulative >= target)
                return value;
        }
        return 255;
    };
    int low = percentile(0.01);
    int high = percentile(0.995);
    if (low < high)
        return {low, high};
    if (low == 0)
        return {0, 1};
    if (low == 255)
        return {254, 255};
    return {low - 1, low + 1};
}

} // namespace desktop_app::v2
