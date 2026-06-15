#pragma once
#include <QtCore>
#include <QtGui>
#include <atomic>
#include "frame_types.h"
#include <functional>

class DcamController;

class FrameGrabber : public QObject {
    Q_OBJECT
  public:
    FrameGrabber(DcamController* ctrl, QObject* parent = nullptr);

    void setDisplayEvery(int n);
    void startGrabbing();
    void stopGrabbing();
    void setRecordHook(std::function<void(const QImage&, const FrameMeta&, double)> hook) {
        recordHook = std::move(hook);
    }

  signals:
    void frameReady(const QImage& img, FrameMeta meta, double fps);

  private:
    DcamController* controller;
    bool running;
    int displayEvery;
    std::function<void(const QImage&, const FrameMeta&, double)> recordHook;
};
