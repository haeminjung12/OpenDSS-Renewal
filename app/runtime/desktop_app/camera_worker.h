#pragma once

#include <QtCore>
#include <QtGui/QImage>
#include <functional>
#include <memory>

#include "dcam_controller.h"
#include "frame_types.h"

class CameraWorker : public QObject {
    Q_OBJECT
public:
    explicit CameraWorker(QObject* parent = nullptr);
    ~CameraWorker() override;

    void setRecordHook(std::function<void(const QImage&, const FrameMeta&, double)> hook);

public slots:
    void initAndOpen(int bits, int pixelType);
    void reconnect(int bits, int pixelType);
    void startCapture(int bits, int pixelType);
    void stopCapture();
    void applySettings(const ApplySettings& settings);
    void setDisplayEvery(int n);
    void shutdown();

signals:
    void initCompleted(const QString& error);
    void reconnectCompleted(const QString& error);
    void startCompleted(const QString& error);
    void stopCompleted();
    void applyCompleted(const QString& error);
    void exposureLimitsReady(double minimumMs, double maximumMs, double currentMs);
    void formatOptionsReady(const QVariantMap& options);
    void readbackReady(const QString& text);
    void frameReady(const QImage& img, FrameMeta meta, double fps);
    void shutdownDone();

private slots:
    void grabOnce();

private:
    DcamController* controller();
    void emitExposureLimits();
    void emitFormatOptions();
    void emitReadback();
    void applyDefaultCameraFormat(int bits, int pixelType);
    void scheduleGrab(int delayMs = 0);

    std::unique_ptr<DcamController> controller_;
    std::function<void(const QImage&, const FrameMeta&, double)> recordHook_;
    bool running_ = false;
    bool shuttingDown_ = false;
    int displayEvery_ = 1;
    int displayCounter_ = 0;
    int framesThisSecond_ = 0;
    double currentFps_ = 0.0;
    QElapsedTimer secondTimer_;
    QElapsedTimer emitTimer_;
    qint64 lastEmitMs_ = 0;
};
