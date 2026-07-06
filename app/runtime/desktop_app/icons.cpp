#include "icons.h"

#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

#include <cmath>

QIcon makeBrandIcon(const QString& key, const QColor& fg, const QColor& accent, const QColor& fill) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (fill.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(3, 3, 26, 26), 6, 6);
    }
    QPen mainPen(fg, 2.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen accentPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(mainPen);
    painter.setBrush(Qt::NoBrush);

    if (key == "menu") {
        painter.drawLine(QPointF(9, 11), QPointF(23, 11));
        painter.drawLine(QPointF(9, 16), QPointF(23, 16));
        painter.drawLine(QPointF(9, 21), QPointF(19, 21));
    } else if (key == "info") {
        painter.drawEllipse(QPointF(16, 16), 9, 9);
        painter.drawLine(QPointF(16, 15), QPointF(16, 21));
        painter.setBrush(fg);
        painter.drawEllipse(QPointF(16, 11), 1.2, 1.2);
    } else if (key == "play") {
        painter.setBrush(accent);
        painter.setPen(Qt::NoPen);
        QPolygonF tri;
        tri << QPointF(12, 9) << QPointF(23, 16) << QPointF(12, 23);
        painter.drawPolygon(tri);
    } else if (key == "camera") {
        painter.drawRoundedRect(QRectF(8, 11, 16, 12), 3, 3);
        painter.drawLine(QPointF(12, 11), QPointF(14, 8));
        painter.drawLine(QPointF(14, 8), QPointF(19, 8));
        painter.drawLine(QPointF(19, 8), QPointF(21, 11));
        painter.setPen(accentPen);
        painter.drawEllipse(QPointF(16, 17), 3.4, 3.4);
    } else if (key == "model") {
        painter.drawRoundedRect(QRectF(9, 9, 14, 14), 3, 3);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(13, 9), QPointF(13, 6));
        painter.drawLine(QPointF(19, 9), QPointF(19, 6));
        painter.drawLine(QPointF(13, 26), QPointF(13, 23));
        painter.drawLine(QPointF(19, 26), QPointF(19, 23));
        painter.drawLine(QPointF(6, 13), QPointF(9, 13));
        painter.drawLine(QPointF(6, 19), QPointF(9, 19));
        painter.drawLine(QPointF(23, 13), QPointF(26, 13));
        painter.drawLine(QPointF(23, 19), QPointF(26, 19));
    } else if (key == "dataset") {
        painter.drawRoundedRect(QRectF(8, 10, 16, 13), 2.5, 2.5);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(10, 10), QPointF(13, 7));
        painter.drawLine(QPointF(13, 7), QPointF(22, 7));
        painter.drawLine(QPointF(22, 7), QPointF(24, 10));
    } else if (key == "trainer") {
        painter.drawLine(QPointF(9, 22), QPointF(9, 10));
        painter.drawLine(QPointF(9, 22), QPointF(24, 22));
        painter.setPen(accentPen);
        painter.drawPolyline(QPolygonF() << QPointF(10, 20) << QPointF(14, 15) << QPointF(18, 17) << QPointF(23, 10));
    } else if (key == "validator") {
        painter.drawRoundedRect(QRectF(8, 8, 16, 16), 4, 4);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(12, 16), QPointF(15, 19));
        painter.drawLine(QPointF(15, 19), QPointF(21, 12));
    } else if (key == "reports") {
        painter.drawRoundedRect(QRectF(10, 7, 13, 18), 2, 2);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(13, 13), QPointF(20, 13));
        painter.drawLine(QPointF(13, 17), QPointF(20, 17));
        painter.drawLine(QPointF(13, 21), QPointF(18, 21));
    } else if (key == "settings") {
        constexpr double kPi = 3.14159265358979323846;
        painter.drawEllipse(QPointF(16, 16), 6.5, 6.5);
        painter.setPen(accentPen);
        for (int i = 0; i < 8; ++i) {
            const double angle = (kPi * 2.0 * i) / 8.0;
            const QPointF inner(16 + std::cos(angle) * 9.0, 16 + std::sin(angle) * 9.0);
            const QPointF outer(16 + std::cos(angle) * 11.5, 16 + std::sin(angle) * 11.5);
            painter.drawLine(inner, outer);
        }
    } else if (key == "fit") {
        painter.drawRect(QRectF(9, 9, 14, 14));
        painter.setPen(accentPen);
        painter.drawLine(QPointF(11, 15), QPointF(15, 11));
        painter.drawLine(QPointF(17, 21), QPointF(21, 17));
    } else if (key == "crosshair") {
        painter.drawEllipse(QPointF(16, 16), 7, 7);
        painter.setPen(accentPen);
        painter.drawLine(QPointF(16, 7), QPointF(16, 25));
        painter.drawLine(QPointF(7, 16), QPointF(25, 16));
    } else if (key == "snapshot") {
        painter.drawRoundedRect(QRectF(8, 10, 16, 13), 2.5, 2.5);
        painter.setPen(accentPen);
        painter.drawEllipse(QPointF(16, 16.5), 3.5, 3.5);
    } else {
        painter.drawEllipse(QPointF(16, 16), 7, 7);
    }

    return QIcon(pixmap);
}
