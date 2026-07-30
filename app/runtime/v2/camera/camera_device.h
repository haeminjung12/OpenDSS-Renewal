#pragma once

#include "../state/domain_state.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <utility>
#include <vector>

namespace desktop_app::v2 {

enum class CameraPixelFormat {
    Mono8,
    Mono16,
};

struct CameraFrame {
    CameraPixelFormat pixelFormat = CameraPixelFormat::Mono8;
    int width = 0;
    int height = 0;
    int rowBytes = 0;
    int bitDepth = 0;
    quint64 deliveryId = 0;
    qint64 monotonicTimestampNs = 0;
    QByteArray bytes;
};

enum class CameraFrameResult {
    Frame,
    NoFrame,
    Error,
};

enum class CameraConfigurationResult {
    Applied,
    Rejected,
    StateUnknown,
};

enum class CameraConfigurationSupport {
    Supported,
    Unsupported,
    Error,
};

class ICameraDevice
{
public:
    virtual ~ICameraDevice() = default;

    virtual QString deviceId() const = 0;
    virtual bool open(QString *error) = 0;
    virtual bool start(QString *error) = 0;
    virtual bool stop(QString *error) = 0;
    virtual bool close(QString *error) = 0;
    virtual CameraFrameResult latestFrame(CameraFrame &frame, QString *error)
    {
        Q_UNUSED(frame);
        if (error)
            *error = QStringLiteral("Single-frame camera reads are not supported.");
        return CameraFrameResult::Error;
    }
    virtual CameraFrameResult drainFrames(std::vector<CameraFrame> &frames,
                                          QString *error)
    {
        CameraFrame frame;
        const CameraFrameResult result = latestFrame(frame, error);
        if (result == CameraFrameResult::Frame)
            frames.push_back(std::move(frame));
        return result;
    }
    virtual CameraConfigurationSupport configurationSupport(QString *error) const
    {
        if (error)
            error->clear();
        return CameraConfigurationSupport::Unsupported;
    }
    virtual bool readConfiguration(CameraAppliedSettings &settings, QString *error)
    {
        Q_UNUSED(settings);
        if (error)
            *error = QStringLiteral("Camera configuration is not supported.");
        return false;
    }
    virtual CameraConfigurationResult applyConfiguration(
        const CameraAppliedSettings &requested,
        CameraAppliedSettings &applied,
        QString *error)
    {
        Q_UNUSED(requested);
        Q_UNUSED(applied);
        if (error)
            *error = QStringLiteral("Camera configuration is not supported.");
        return CameraConfigurationResult::Rejected;
    }
};

} // namespace desktop_app::v2

Q_DECLARE_METATYPE(desktop_app::v2::CameraFrame)

