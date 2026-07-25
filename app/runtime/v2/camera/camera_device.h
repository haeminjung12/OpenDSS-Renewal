#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

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

class ICameraDevice
{
public:
    virtual ~ICameraDevice() = default;

    virtual QString deviceId() const = 0;
    virtual bool open(QString *error) = 0;
    virtual bool start(QString *error) = 0;
    virtual bool stop(QString *error) = 0;
    virtual bool close(QString *error) = 0;
    virtual CameraFrameResult latestFrame(CameraFrame &frame, QString *error) = 0;
};

} // namespace desktop_app::v2
