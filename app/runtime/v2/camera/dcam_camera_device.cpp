#include "dcam_camera_device.h"

#include "../../dcam_camera.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace desktop_app::v2 {
namespace {

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

QString messageFrom(const std::string &message)
{
    return QString::fromStdString(message);
}

qint64 monotonicTimestampNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

DcamCameraDevice::DcamCameraDevice() = default;

DcamCameraDevice::~DcamCameraDevice() = default;

QString DcamCameraDevice::deviceId() const
{
    return QStringLiteral("DCAM:0");
}

bool DcamCameraDevice::open(QString *error)
{
    camera_ = std::make_unique<DcamCamera>();
    const std::string result = camera_->init(0);
    if (!result.empty()) {
        camera_.reset();
        setError(error, messageFrom(result));
        return false;
    }

    setError(error, {});
    return true;
}

bool DcamCameraDevice::start(QString *error)
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return false;
    }

    const std::string result = camera_->start();
    setError(error, messageFrom(result));
    return result.empty();
}

bool DcamCameraDevice::stop(QString *error)
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return false;
    }

    camera_->stop();
    setError(error, {});
    return true;
}

bool DcamCameraDevice::close(QString *error)
{
    if (camera_) {
        camera_->cleanup();
        camera_.reset();
    }
    setError(error, {});
    return true;
}

CameraFrameResult DcamCameraDevice::latestFrame(CameraFrame &frame, QString *error)
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return CameraFrameResult::Error;
    }
    if (!camera_->waitForFrame(1)) {
        setError(error, {});
        return CameraFrameResult::NoFrame;
    }

    FrameData source;
    if (!camera_->getLatestFrame(source) || source.image.empty()) {
        setError(error, QStringLiteral("The camera frame could not be read."));
        return CameraFrameResult::Error;
    }
    if (source.image.type() != CV_8UC1 && source.image.type() != CV_16UC1) {
        setError(error, QStringLiteral("The camera returned an unsupported pixel format."));
        return CameraFrameResult::Error;
    }
    if (source.image.type() == CV_16UC1
        && (source.meta.bits < 9 || source.meta.bits > 16)) {
        setError(error, QStringLiteral("The camera returned an invalid bit depth."));
        return CameraFrameResult::Error;
    }

    const size_t sourceRowBytes = source.image.step;
    if (sourceRowBytes > static_cast<size_t>(std::numeric_limits<int>::max())
        || source.image.rows <= 0
        || sourceRowBytes > static_cast<size_t>(std::numeric_limits<qsizetype>::max())
                / static_cast<size_t>(source.image.rows)) {
        setError(error, QStringLiteral("The camera frame is too large."));
        return CameraFrameResult::Error;
    }

    const qsizetype rowBytes = static_cast<qsizetype>(sourceRowBytes);
    const qsizetype byteCount = rowBytes * source.image.rows;
    frame.pixelFormat =
        source.image.type() == CV_8UC1 ? CameraPixelFormat::Mono8 : CameraPixelFormat::Mono16;
    frame.width = source.image.cols;
    frame.height = source.image.rows;
    frame.rowBytes = static_cast<int>(rowBytes);
    frame.bitDepth =
        frame.pixelFormat == CameraPixelFormat::Mono8 ? 8 : source.meta.bits;
    frame.deliveryId = source.meta.delivered > 0
        ? static_cast<quint64>(source.meta.delivered)
        : static_cast<quint64>(std::max<int64_t>(source.meta.frameIndex, 0));
    frame.monotonicTimestampNs = monotonicTimestampNs();
    frame.bytes = QByteArray(reinterpret_cast<const char *>(source.image.data), byteCount);
    setError(error, {});
    return CameraFrameResult::Frame;
}

} // namespace desktop_app::v2
