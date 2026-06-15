#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QScrollArea>

#include <atomic>
#include <functional>

class QLabel;
class QWheelEvent;

class ZoomImageView : public QScrollArea {
  public:
    explicit ZoomImageView(QWidget* parent = nullptr);

    void setZoomChanged(const std::function<void(double)>& cb);
    void setImageLabelObjectName(const char* name);
    void setImage(const QImage& img);
    void resetScale();
    void fitToView();
    void zoomBySteps(int steps);

  protected:
    void wheelEvent(QWheelEvent* ev) override;

  private:
    double computeMaxScale() const;
    void updatePixmap();

    QLabel* label_;
    QImage lastImage_;
    QPixmap basePixmap_;
    double scale_;
    double effectiveScale_;
    bool hasImage_;
    int zoomSteps_;
    QElapsedTimer zoomReadyTimer_;
    std::atomic_flag updatingPixmap_ = ATOMIC_FLAG_INIT;
    std::function<void(double)> onZoomChanged_;
};
