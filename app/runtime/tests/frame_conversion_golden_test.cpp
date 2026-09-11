#include "v2/camera/frame_conversion.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstring>
#include <limits>

using namespace desktop_app::v2;

namespace {

bool check(bool condition, const char *message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;

    CameraFrame mono8;
    mono8.pixelFormat = CameraPixelFormat::Mono8;
    mono8.width = 3;
    mono8.height = 2;
    mono8.rowBytes = 5;
    mono8.bitDepth = 8;
    mono8.bytes = QByteArray::fromHex("001122aabb334455ccdd");
    const QImage converted8 = convertCameraFrame(mono8, &error);
    ok &= check(converted8.format() == QImage::Format_Grayscale8
                    && converted8.width() == 3 && converted8.height() == 2,
                "Mono8 must produce an owned Grayscale8 image.");
    ok &= check(std::memcmp(converted8.constScanLine(0), "\x00\x11\x22", 3) == 0
                    && std::memcmp(converted8.constScanLine(1), "\x33\x44\x55", 3) == 0,
                "Mono8 conversion must copy active pixels and skip row padding.");
    const QImage lutImage = applyLinearContrast(converted8, 17, 68);
    ok &= check(lutImage.constScanLine(0)[0] == 0
                    && lutImage.constScanLine(0)[1] == 0
                    && lutImage.constScanLine(0)[2] == 85
                    && lutImage.constScanLine(1)[0] == 170
                    && lutImage.constScanLine(1)[1] == 255,
                "Contrast must linearly clamp LOW and HIGH levels.");
    ok &= check(converted8.constScanLine(0)[2] == 0x22,
                "Contrast conversion must not modify the source image.");
    QImage allValues(256, 1, QImage::Format_Grayscale8);
    for (int value = 0; value < 256; ++value)
        allValues.scanLine(0)[value] = static_cast<uchar>(value);
    const QImage allValuesAdjusted = applyLinearContrast(allValues, 17, 68);
    bool exactLinearContrast = true;
    for (int value = 0; value < 256; ++value) {
        const int expected = value <= 17 ? 0
            : value >= 68 ? 255
            : (value - 17) * 255 / (68 - 17);
        exactLinearContrast &= allValuesAdjusted.constScanLine(0)[value] == expected;
    }
    ok &= check(exactLinearContrast,
                "Contrast lookup must preserve exact linear-clamp output for every byte value.");
    ok &= check(autoContrastRange(QImage()) == QPair<int, int>(0, 255),
                "Auto Contrast must retain the default range for an empty frame.");
    QImage flat(2, 2, QImage::Format_Grayscale8);
    flat.fill(127);
    ok &= check(autoContrastRange(flat) == QPair<int, int>(126, 128),
                "Auto Contrast must produce a valid deterministic range for a flat frame.");

    CameraFrame mono16;
    mono16.pixelFormat = CameraPixelFormat::Mono16;
    mono16.width = 2;
    mono16.height = 2;
    mono16.rowBytes = 6;
    mono16.bitDepth = 16;
    mono16.bytes.resize(12);
    const quint16 values[4] = {0, 0x1234, 0x8000, 0xffff};
    std::memcpy(mono16.bytes.data(), values, 2 * sizeof(quint16));
    mono16.bytes[4] = static_cast<char>(0xaa);
    mono16.bytes[5] = static_cast<char>(0xbb);
    std::memcpy(mono16.bytes.data() + 6, values + 2, 2 * sizeof(quint16));
    mono16.bytes[10] = static_cast<char>(0xcc);
    mono16.bytes[11] = static_cast<char>(0xdd);

    QImage reference16(2, 2, QImage::Format_Grayscale16);
    std::memcpy(reference16.scanLine(0), values, 2 * sizeof(quint16));
    std::memcpy(reference16.scanLine(1), values + 2, 2 * sizeof(quint16));
    const QImage expected8 = reference16.convertToFormat(QImage::Format_Grayscale8);
    const QImage converted16 = convertCameraFrame(mono16, &error);
    ok &= check(converted16 == expected8,
                "Mono16 conversion must exactly match Qt Grayscale16-to-Grayscale8 semantics.");

    CameraFrame invalid = mono8;
    invalid.rowBytes = 2;
    ok &= check(convertCameraFrame(invalid, &error).isNull() && error.contains("stride"),
                "A short row stride must be rejected.");
    invalid = mono8;
    invalid.bytes.chop(1);
    ok &= check(convertCameraFrame(invalid, &error).isNull() && error.contains("incomplete"),
                "A truncated frame must be rejected.");
    invalid = mono8;
    invalid.bitDepth = 7;
    ok &= check(convertCameraFrame(invalid, &error).isNull() && error.contains("bit depth"),
                "A mismatched bit depth must be rejected.");
    invalid = mono8;
    invalid.width = 0;
    ok &= check(convertCameraFrame(invalid, &error).isNull() && error.contains("dimensions"),
                "Invalid dimensions must be rejected.");
    invalid = mono8;
    invalid.height = std::numeric_limits<int>::max();
    invalid.rowBytes = std::numeric_limits<int>::max();
    invalid.bytes.clear();
    ok &= check(convertCameraFrame(invalid, &error).isNull()
                    && (error.contains("byte count") || error.contains("incomplete")),
                "An overflowing or unavailable declared frame byte count must be rejected.");

    return ok ? 0 : 1;
}
