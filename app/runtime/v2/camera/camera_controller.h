#pragma once

#include <QObject>
#include <QString>

namespace desktop_app::v2 {

class CameraService;

class CameraController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString cameraStatus READ cameraStatus NOTIFY stateChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool streaming READ streaming NOTIFY stateChanged)

public:
    explicit CameraController(CameraService &service, QObject *parent = nullptr);

    QString cameraStatus() const;
    QString deviceId() const;
    QString error() const;
    bool streaming() const;

    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool recover();

signals:
    void stateChanged();
    void errorChanged();

private:
    void setError(const QString &error);

    CameraService &service_;
    QString actionError_;
};

} // namespace desktop_app::v2
