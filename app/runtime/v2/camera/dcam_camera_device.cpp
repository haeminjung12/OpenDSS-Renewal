#include "dcam_camera_device.h"

#include "../../dcam_camera.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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

bool validateConfiguration(const CameraAppliedSettings &settings, QString *error)
{
    if (settings.width <= 0 || settings.height <= 0) {
        setError(error, QStringLiteral("Camera width and height must be positive."));
        return false;
    }
    if (settings.bitDepth != 8 && settings.bitDepth != 12 && settings.bitDepth != 16) {
        setError(error, QStringLiteral("Camera bit depth must be 8, 12, or 16."));
        return false;
    }
    if ((settings.bitDepth == 8 && settings.pixelType != CameraPixelType::Mono8)
        || (settings.bitDepth > 8 && settings.pixelType != CameraPixelType::Mono16)) {
        setError(error, QStringLiteral("Camera pixel type does not match the selected bit depth."));
        return false;
    }
    if (!std::isfinite(settings.exposureMs) || settings.exposureMs <= 0.0) {
        setError(error, QStringLiteral("Camera exposure must be greater than 0 ms."));
        return false;
    }
    return true;
}

bool fromDcam(const CameraSettings &source, CameraAppliedSettings &settings, QString *error)
{
    settings.width = source.width;
    settings.height = source.height;
    settings.bitDepth = source.bits;
    settings.exposureMs = source.exposureMs;
    if (source.pixelType == DCAM_PIXELTYPE_MONO8)
        settings.pixelType = CameraPixelType::Mono8;
    else if (source.pixelType == DCAM_PIXELTYPE_MONO16)
        settings.pixelType = CameraPixelType::Mono16;
    else {
        setError(error, QStringLiteral("The camera reported an unsupported pixel type."));
        return false;
    }
    if (source.readoutSpeed <= DCAMPROP_READOUTSPEED__SLOWEST)
        settings.readoutMode = CameraReadoutMode::Slow;
    else
        settings.readoutMode = CameraReadoutMode::Fast;
    return validateConfiguration(settings, error);
}

CameraSettings toDcam(const CameraAppliedSettings &source)
{
    CameraSettings settings;
    settings.width = source.width;
    settings.height = source.height;
    settings.bits = source.bitDepth;
    settings.pixelType =
        source.pixelType == CameraPixelType::Mono8 ? DCAM_PIXELTYPE_MONO8
                                                   : DCAM_PIXELTYPE_MONO16;
    settings.exposureMs = source.exposureMs;
    settings.readoutSpeed =
        source.readoutMode == CameraReadoutMode::Fast ? DCAMPROP_READOUTSPEED__FASTEST
                                                      : DCAMPROP_READOUTSPEED__SLOWEST;
    return settings;
}

QString configurationMismatch(const CameraAppliedSettings &requested,
                              const CameraAppliedSettings &readback)
{
    if (requested.width != readback.width) {
        return QStringLiteral("width requested %1, read back %2")
            .arg(requested.width)
            .arg(readback.width);
    }
    if (requested.height != readback.height) {
        return QStringLiteral("height requested %1, read back %2")
            .arg(requested.height)
            .arg(readback.height);
    }
    if (requested.bitDepth != readback.bitDepth) {
        return QStringLiteral("bit depth requested %1, read back %2")
            .arg(requested.bitDepth)
            .arg(readback.bitDepth);
    }
    if (requested.pixelType != readback.pixelType) {
        return QStringLiteral("pixel type requested %1, read back %2")
            .arg(static_cast<int>(requested.pixelType))
            .arg(static_cast<int>(readback.pixelType));
    }
    if (std::abs(requested.exposureMs - readback.exposureMs) > 0.01) {
        return QStringLiteral(
                   "exposure requested %1 ms, read back %2 ms (tolerance 0.01 ms)")
            .arg(requested.exposureMs, 0, 'g', 12)
            .arg(readback.exposureMs, 0, 'g', 12);
    }
    if (requested.readoutMode != readback.readoutMode) {
        return QStringLiteral("readout mode requested %1, read back %2")
            .arg(static_cast<int>(requested.readoutMode))
            .arg(static_cast<int>(readback.readoutMode));
    }
    return {};
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

CameraConfigurationSupport DcamCameraDevice::configurationSupport(QString *error) const
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return CameraConfigurationSupport::Error;
    }
    std::string probeError;
    switch (camera_->approvedSettingsSupport(probeError)) {
    case CameraSettingsSupport::Supported:
        setError(error, {});
        return CameraConfigurationSupport::Supported;
    case CameraSettingsSupport::Unsupported:
        setError(error, {});
        return CameraConfigurationSupport::Unsupported;
    case CameraSettingsSupport::Error:
        setError(error, messageFrom(probeError));
        return CameraConfigurationSupport::Error;
    }
    setError(error, QStringLiteral("The DCAM camera configuration probe failed."));
    return CameraConfigurationSupport::Error;
}

bool DcamCameraDevice::readConfiguration(CameraAppliedSettings &settings, QString *error)
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return false;
    }

    CameraSettings source;
    const std::string result = camera_->readApprovedSettings(source);
    if (!result.empty()) {
        setError(error, messageFrom(result));
        return false;
    }
    if (!fromDcam(source, settings, error))
        return false;
    setError(error, {});
    return true;
}

CameraConfigurationResult DcamCameraDevice::applyConfiguration(
    const CameraAppliedSettings &requested,
    CameraAppliedSettings &applied,
    QString *error)
{
    if (!camera_ || !camera_->isOpened()) {
        setError(error, QStringLiteral("The DCAM camera is not open."));
        return CameraConfigurationResult::Rejected;
    }
    if (!validateConfiguration(requested, error))
        return CameraConfigurationResult::Rejected;

    CameraAppliedSettings previous;
    if (!readConfiguration(previous, error))
        return CameraConfigurationResult::Rejected;

    auto rollback = [&](const QString &applyFailure) {
        const std::string rollbackResult =
            camera_->applyApprovedSettings(toDcam(previous));
        if (!rollbackResult.empty()) {
            setError(error,
                     QStringLiteral("%1 Rollback also failed: %2")
                         .arg(applyFailure, messageFrom(rollbackResult)));
            return CameraConfigurationResult::StateUnknown;
        }
        CameraAppliedSettings rollbackReadback;
        QString rollbackReadError;
        const bool rollbackRead = readConfiguration(
            rollbackReadback, &rollbackReadError);
        const QString rollbackMismatch = rollbackRead
            ? configurationMismatch(previous, rollbackReadback) : QString();
        if (!rollbackRead || !rollbackMismatch.isEmpty()) {
            const QString rollbackFailure = !rollbackReadError.isEmpty()
                ? rollbackReadError
                : QStringLiteral("rollback readback mismatch: %1")
                      .arg(rollbackMismatch);
            setError(error,
                     QStringLiteral("%1 Rollback verification failed: %2")
                         .arg(applyFailure, rollbackFailure));
            return CameraConfigurationResult::StateUnknown;
        }
        setError(error, applyFailure);
        return CameraConfigurationResult::Rejected;
    };

    const std::string applyResult = camera_->applyApprovedSettings(toDcam(requested));
    if (!applyResult.empty()) {
        return rollback(messageFrom(applyResult));
    }

    CameraAppliedSettings readback;
    const bool readbackAvailable = readConfiguration(readback, error);
    const QString mismatch = readbackAvailable
        ? configurationMismatch(requested, readback) : QString();
    if (!readbackAvailable || !mismatch.isEmpty()) {
        const QString applyFailure = error && !error->isEmpty()
            ? *error
            : QStringLiteral("Camera configuration readback mismatch: %1")
                  .arg(mismatch);
        return rollback(applyFailure);
    }

    applied = readback;
    setError(error, {});
    return CameraConfigurationResult::Applied;
}

} // namespace desktop_app::v2
