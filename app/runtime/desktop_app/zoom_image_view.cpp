#include "zoom_image_view.h"

#include <QLabel>
#include <QPalette>
#include <QPointF>
#include <QScrollBar>
#include <QSizePolicy>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <exception>

#include "object_names.h"

ZoomImageView::ZoomImageView(QWidget* parent)
    : QScrollArea(parent), label_(new QLabel), scale_(1.0), effectiveScale_(1.0), hasImage_(false), zoomSteps_(0) {
    label_->setBackgroundRole(QPalette::Base);
    label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    label_->setScaledContents(true); // paint-time scaling instead of allocating huge pixmaps
    setWidget(label_);
    setAlignment(Qt::AlignCenter);
    setWidgetResizable(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setMouseTracking(true);
}

void ZoomImageView::setZoomChanged(const std::function<void(double)>& cb) {
    onZoomChanged_ = cb;
}

void ZoomImageView::setImageLabelObjectName(const char* name) {
    nameWidget(label_, name);
}

void ZoomImageView::setImage(const QImage& img) {
    if (img.isNull()) {
        return;
    }
    if (!hasImage_) {
        scale_ = 1.0;
        effectiveScale_ = 1.0;
        hasImage_ = true;
        zoomSteps_ = 0;
        zoomReadyTimer_.restart();
        if (onZoomChanged_) {
            onZoomChanged_(effectiveScale_);
        }
        if (horizontalScrollBar()) {
            horizontalScrollBar()->setValue(0);
        }
        if (verticalScrollBar()) {
            verticalScrollBar()->setValue(0);
        }
    }
    // Make a deep copy so the buffer is stable while frames keep streaming.
    lastImage_ = img.copy();
    basePixmap_ = QPixmap::fromImage(lastImage_);
    updatePixmap();
}

void ZoomImageView::resetScale() {
    scale_ = 1.0;
    effectiveScale_ = 1.0;
    zoomSteps_ = 0;
    if (onZoomChanged_) {
        onZoomChanged_(effectiveScale_);
    }
    hasImage_ = !lastImage_.isNull();
    updatePixmap();
}

void ZoomImageView::fitToView() {
    if (lastImage_.isNull()) {
        resetScale();
        return;
    }
    const QSize available = viewport() ? viewport()->contentsRect().size() : QSize();
    if (available.width() <= 0 || available.height() <= 0) {
        resetScale();
        return;
    }
    const double sx = static_cast<double>(available.width()) / static_cast<double>(lastImage_.width());
    const double sy = static_cast<double>(available.height()) / static_cast<double>(lastImage_.height());
    scale_ = std::clamp(std::min(sx, sy), 0.05, computeMaxScale());
    zoomSteps_ = static_cast<int>(std::lround(std::log(scale_) / std::log(1.25)));
    updatePixmap();
    if (horizontalScrollBar()) {
        horizontalScrollBar()->setValue(0);
    }
    if (verticalScrollBar()) {
        verticalScrollBar()->setValue(0);
    }
}

void ZoomImageView::zoomBySteps(int steps) {
    if (lastImage_.isNull()) {
        return;
    }
    zoomSteps_ = std::clamp(zoomSteps_ + steps, -50, 50);
    const double desiredScale = std::pow(1.25, zoomSteps_);
    scale_ = std::clamp(desiredScale, 0.05, computeMaxScale());
    zoomSteps_ = static_cast<int>(std::lround(std::log(scale_) / std::log(1.25)));
    updatePixmap();
}

void ZoomImageView::wheelEvent(QWheelEvent* ev) {
    try {
        if (lastImage_.isNull()) {
            QScrollArea::wheelEvent(ev);
            return;
        }
        if (zoomReadyTimer_.isValid() && zoomReadyTimer_.elapsed() < 1000) {
            ev->accept();
            return;
        }
        if (!horizontalScrollBar() || !verticalScrollBar()) {
            QScrollArea::wheelEvent(ev);
            return;
        }
        // Normalize to wheel ticks (120 per detent)
        const double ticks = ev->angleDelta().y() / 120.0;
        const double oldScale = scale_;
        const int newSteps = std::clamp(zoomSteps_ + static_cast<int>(std::round(ticks)), -50, 50);
        const double desiredScale = std::pow(1.25, newSteps); // ~1.25x per tick
        const double maxScale = computeMaxScale();
        const double newScale = std::clamp(desiredScale, 0.05, maxScale); // avoid zero/negative and clamp max
        if (qFuzzyCompare(newScale, scale_)) {
            ev->accept();
            return;
        }
        // Keep steps consistent with the clamped scale to avoid runaway values.
        zoomSteps_ = static_cast<int>(std::lround(std::log(newScale) / std::log(1.25)));

        const QPointF vpPos = ev->position();
        const QPointF contentPos =
            (vpPos + QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value())) / oldScale;

        scale_ = newScale;
        updatePixmap();

        horizontalScrollBar()->setValue(int(contentPos.x() * scale_ - vpPos.x()));
        verticalScrollBar()->setValue(int(contentPos.y() * scale_ - vpPos.y()));
        ev->accept();
    } catch (const std::exception& e) {
        Q_UNUSED(e);
    } catch (...) {
    }
}

double ZoomImageView::computeMaxScale() const {
    if (basePixmap_.isNull()) {
        return 1.56;
    }
    const int w = basePixmap_.width();
    const int h = basePixmap_.height();
    const int maxDim = (std::min(w, h) <= 256) ? 8192 : 4096;
    const double dimCap = static_cast<double>(maxDim) / static_cast<double>(std::max(w, h));
    // Allow more zoom for small dimensions but cap to a sane upper bound.
    return std::clamp(std::max(1.56, dimCap * 2.0), 0.1, 8.0);
}

void ZoomImageView::updatePixmap() {
    try {
        if (basePixmap_.isNull() || scale_ <= 0.0) {
            return;
        }
        if (updatingPixmap_.test_and_set()) {
            // Skip re-entrant calls that can happen when zooming rapidly during streaming.
            return;
        }
        const int baseW = basePixmap_.width();
        const int baseH = basePixmap_.height();
        QSize targetSize = (scale_ == 1.0) ? basePixmap_.size()
                                           : QSize(std::max(1, int(std::lround(baseW * scale_))),
                                                   std::max(1, int(std::lround(baseH * scale_))));

        const int maxDim = (std::min(baseW, baseH) <= 256) ? 8192 : 4096;
        if (targetSize.width() > maxDim || targetSize.height() > maxDim) {
            const double factor =
                static_cast<double>(maxDim) / static_cast<double>(std::max(targetSize.width(), targetSize.height()));
            targetSize.setWidth(std::max(1, int(std::lround(targetSize.width() * factor))));
            targetSize.setHeight(std::max(1, int(std::lround(targetSize.height() * factor))));
        }

        label_->setPixmap(basePixmap_);
        label_->resize(targetSize);
        label_->setAlignment(Qt::AlignCenter);
        effectiveScale_ = static_cast<double>(targetSize.width()) / static_cast<double>(baseW);
        if (onZoomChanged_) {
            onZoomChanged_(effectiveScale_);
        }
        updatingPixmap_.clear();
    } catch (const std::exception& e) {
        Q_UNUSED(e);
        updatingPixmap_.clear();
    } catch (...) {
        updatingPixmap_.clear();
    }
}
