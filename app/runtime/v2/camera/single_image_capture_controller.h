#pragma once

#include "camera_device.h"
#include "../operation/operation_coordinator.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

namespace desktop_app::v2 {

class CameraController;
class SingleImageCaptureService;

class SingleImageCaptureController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl outputFolder READ outputFolder WRITE setOutputFolder
                   NOTIFY outputFolderChanged)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
    Q_PROPERTY(bool canCapture READ canCapture NOTIFY stateChanged)
    Q_PROPERTY(QString disabledReason READ disabledReason NOTIFY stateChanged)
    Q_PROPERTY(QString presentation READ presentation NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QUrl savedArtifactUrl READ savedArtifactUrl NOTIFY savedArtifactUrlChanged)

public:
    SingleImageCaptureController(SingleImageCaptureService &captureService,
                                 CameraController &cameraController,
                                 OperationCoordinator &operations,
                                 QObject *parent = nullptr);
    ~SingleImageCaptureController() override;

    bool initializeDefaultOutputFolder(const QString &documentsFolder);
    QUrl outputFolder() const;
    void setOutputFolder(const QUrl &folder);
    QString fileName() const;
    void setFileName(const QString &fileName);
    bool canCapture() const;
    QString disabledReason() const;
    QString presentation() const;
    QString error() const;
    QUrl savedArtifactUrl() const;

    Q_INVOKABLE void setOutputFolderPath(const QString &path);
    Q_INVOKABLE bool capture();

signals:
    void outputFolderChanged();
    void fileNameChanged();
    void stateChanged();
    void errorChanged();
    void savedArtifactUrlChanged();

private:
    struct CaptureResult {
        bool saved = false;
        QString path;
        QString error;
    };

    void acceptFrame(CameraFrame frame);
    void finishCapture();
    void timeoutWaitingForFrame();
    void clearOutcome();
    void setError(const QString &error);
    void setSavedArtifactUrl(const QUrl &url);

    SingleImageCaptureService &captureService_;
    CameraController &cameraController_;
    OperationCoordinator &operations_;
    QFutureWatcher<CaptureResult> saveWatcher_;
    QTimer frameTimeout_;
    MomentaryLease lease_;
    QUrl outputFolder_;
    QString fileName_;
    QString acceptedOutputFolder_;
    QString acceptedFileName_;
    QString error_;
    QUrl savedArtifactUrl_;
    quint64 baselineDeliveryId_ = 0;
    bool baselineHadFrame_ = false;
    bool capturing_ = false;
    bool writing_ = false;
};

} // namespace desktop_app::v2
