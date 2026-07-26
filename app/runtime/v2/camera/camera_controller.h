#pragma once

#include "camera_device.h"

#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <optional>

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
    Q_PROPERTY(bool configurationAvailable READ configurationAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY stateChanged)
    Q_PROPERTY(QString customWidth READ customWidth NOTIFY stateChanged)
    Q_PROPERTY(QString customHeight READ customHeight NOTIFY stateChanged)
    Q_PROPERTY(QString bitDepth READ bitDepth NOTIFY stateChanged)
    Q_PROPERTY(QString exposureMs READ exposureMs NOTIFY stateChanged)
    Q_PROPERTY(QString readoutMode READ readoutMode NOTIFY stateChanged)
    Q_PROPERTY(QStringList resolutionPresets READ resolutionPresets CONSTANT)
    Q_PROPERTY(int resolutionPresetIndex READ resolutionPresetIndex NOTIFY stateChanged)
    Q_PROPERTY(int previewLutMinimum READ previewLutMinimum NOTIFY previewLutChanged)
    Q_PROPERTY(int previewLutMaximum READ previewLutMaximum NOTIFY previewLutChanged)

public:
    CameraController(CameraService &service, CameraPreviewImageProvider &previewProvider,
                     QObject *parent = nullptr);

    QString cameraStatus() const;
    QString deviceId() const;
    QString error() const;
    bool streaming() const;
    bool busy() const;
    QString previewSource() const;
    bool configurationAvailable() const;
    QString resolution() const;
    QString customWidth() const;
    QString customHeight() const;
    QString bitDepth() const;
    QString exposureMs() const;
    QString readoutMode() const;
    QStringList resolutionPresets() const;
    int resolutionPresetIndex() const;
    int previewLutMinimum() const;
    int previewLutMaximum() const;
    bool hasFrame() const;
    quint64 latestDeliveryId() const;

    Q_INVOKABLE bool open();
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();
    Q_INVOKABLE bool recover();
    Q_INVOKABLE bool close();
    Q_INVOKABLE bool applyResolution(int width, int height);
    Q_INVOKABLE bool selectCustomResolution();
    Q_INVOKABLE bool selectResolutionPreset(int index);
    Q_INVOKABLE bool applyBitDepth(int bitDepth);
    Q_INVOKABLE bool applyExposureMs(double exposureMs);
    Q_INVOKABLE bool applyReadoutMode(const QString &readoutMode);
    Q_INVOKABLE void setPreviewLutRange(int blackLevel, int whiteLevel);
    bool applyProfileSettings(const CameraAppliedSettings &settings,
                              int lutMinimum, int lutMaximum,
                              int timeoutMs = 5000);

signals:
    void stateChanged();
    void errorChanged();
    void busyChanged();
    void previewSourceChanged();
    void previewLutChanged();
    void frameReady(desktop_app::v2::CameraFrame frame);

    void openRequested();
    void startRequested();
    void stopRequested();
    void recoverRequested();
    void closeRequested();
    void configurationRequested(desktop_app::v2::CameraAppliedSettings requested);

private:
    bool request(void (CameraController::*signal)());
    void updateState(int status, const QString &deviceId, const QString &fault);
    void acceptFrame(CameraFrame frame);
    void updateFrame();
    void updateConfiguration(bool available, CameraAppliedSettings appliedSettings);
    bool requestConfiguration(CameraAppliedSettings requested);
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
    bool configurationAvailable_ = false;
    bool customResolutionSelected_ = false;
    std::optional<bool> pendingCustomResolutionSelected_;
    bool profileApplyTimedOut_ = false;
    CameraAppliedSettings appliedSettings_;
    int previewLutMinimum_ = 0;
    int previewLutMaximum_ = 255;
    QTimer previewPublishTimer_;
    QMutex pendingPreviewFrameMutex_;
    std::optional<CameraFrame> pendingPreviewFrame_;
    bool previewDeliveryQueued_ = false;
};

} // namespace desktop_app::v2
