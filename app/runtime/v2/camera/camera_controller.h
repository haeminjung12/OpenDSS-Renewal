#pragma once

#include "camera_device.h"

#include <QObject>
#include <QString>

namespace desktop_app::v2 {

class CameraPreviewImageProvider;
class CameraService;

class CameraController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString cameraStatus READ cameraStatus NOTIFY stateChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString previewSource READ previewSource NOTIFY previewSourceChanged)

public:
    CameraController(CameraService &service, CameraPreviewImageProvider &previewProvider,
                     QObject *parent = nullptr);

    QString cameraStatus() const;
    QString deviceId() const;
    QString error() const;
    bool streaming() const;
    bool busy() const;
    QString previewSource() const;
    bool hasFrame() const;
    quint64 latestDeliveryId() const;

    Q_INVOKABLE bool open();
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool recover();
    Q_INVOKABLE bool close();

signals:
    void stateChanged();
    void errorChanged();
    void busyChanged();
    void previewSourceChanged();
    void frameReady(desktop_app::v2::CameraFrame frame);

    void openRequested();
    void startRequested();
    void stopRequested();
    void recoverRequested();
    void closeRequested();

private:
    bool request(void (CameraController::*signal)());
    void updateState(int status, const QString &deviceId, const QString &fault);
    void updateFrame(CameraFrame frame);
    void setError(const QString &error);
    void setBusy(bool busy);

    CameraService &service_;
    CameraPreviewImageProvider &previewProvider_;
    int status_ = 0;
    QString deviceId_;
    QString serviceFault_;
    QString actionError_;
    QString previewSource_;
    quint64 latestDeliveryId_ = 0;
    bool hasFrame_ = false;
    bool busy_ = false;
};

} // namespace desktop_app::v2
