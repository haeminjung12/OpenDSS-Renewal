#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace desktop_app::v2 {

class ApplicationStateStore;
class CameraService;
class OperationCoordinator;
class SingleImageCaptureService;

class SingleImageCaptureController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl outputFolder READ outputFolder WRITE setOutputFolder
                   NOTIFY outputFolderChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(bool canCapture READ canCapture NOTIFY stateChanged)
    Q_PROPERTY(QString presentation READ presentation NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QUrl savedArtifactUrl READ savedArtifactUrl NOTIFY savedArtifactUrlChanged)

public:
    SingleImageCaptureController(SingleImageCaptureService &captureService,
                                 CameraService &cameraService,
                                 ApplicationStateStore &stateStore,
                                 OperationCoordinator &operations,
                                 QObject *parent = nullptr);

    QUrl outputFolder() const;
    void setOutputFolder(const QUrl &folder);
    QString fileName() const;
    void setFileName(const QString &fileName);
    bool canCapture() const;
    QString presentation() const;
    QString error() const;
    QUrl savedArtifactUrl() const;

    Q_INVOKABLE bool capture();

signals:
    void outputFolderChanged();
    void fileNameChanged();
    void stateChanged();
    void errorChanged();
    void savedArtifactUrlChanged();

private:
    void clearOutcome();
    void setError(const QString &error);
    void setSavedArtifactUrl(const QUrl &url);

    SingleImageCaptureService &captureService_;
    CameraService &cameraService_;
    OperationCoordinator &operations_;
    QUrl outputFolder_;
    QString fileName_;
    QString error_;
    QUrl savedArtifactUrl_;
    bool capturing_ = false;
};

} // namespace desktop_app::v2
