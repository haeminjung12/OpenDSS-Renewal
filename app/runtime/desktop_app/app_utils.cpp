#include "app_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>

#include <cmath>

QString findProjectRootFromApp() {
    QStringList starts;
    starts << QCoreApplication::applicationDirPath() << QDir::currentPath();
    for (const auto& start : starts) {
        QDir dir(start);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists() &&
                (QFileInfo(dir.filePath("app/runtime/models")).isDir() ||
                 QFileInfo(dir.filePath("internal-release/app/runtime/models")).isDir())) {
                return dir.absolutePath();
            }
            if (!dir.cdUp()) break;
        }
    }
    return QString();
}

QString runOutputBaseForSettings(const QString& outputDir) {
    QString trimmed = outputDir.trimmed();
    if (trimmed.isEmpty()) return trimmed;
    QDir dir(trimmed);
    QString leaf = dir.dirName();
    if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
        dir.cdUp();
        return dir.absolutePath();
    }
    return trimmed;
}

void setComboTextIfPresent(QComboBox* combo, const QString& text) {
    if (!combo || text.isEmpty()) return;
    int index = combo->findText(text);
    if (index >= 0) combo->setCurrentIndex(index);
}

QJsonObject comboSnapshot(QComboBox* combo) {
    QJsonObject obj;
    if (!combo) return obj;
    obj["index"] = combo->currentIndex();
    obj["text"] = combo->currentText();
    return obj;
}

QString formatTimeSeconds(double seconds) {
    if (seconds < 0) seconds = 0;
    int totalMs = static_cast<int>(std::lround(seconds * 1000.0));
    int ms = totalMs % 1000;
    int totalSec = totalMs / 1000;
    int s = totalSec % 60;
    int totalMin = totalSec / 60;
    int m = totalMin % 60;
    int h = totalMin / 60;
    if (h > 0) {
        return QString("%1:%2:%3.%4")
            .arg(h, 2, 10, QChar('0'))
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
    }
    return QString("%1:%2.%3")
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

QImage renderPieChart(const QString& title,
                      const QVector<QString>& labels,
                      const QVector<double>& values,
                      const QVector<QColor>& colors,
                      const QSize& size) {
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont titleFont = p.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(Qt::black);
    p.drawText(QRect(0, 0, size.width(), 30), Qt::AlignCenter, title);

    double total = 0.0;
    for (double v : values) total += v;
    QRect pieRect(20, 40, size.height() - 60, size.height() - 60);
    if (total <= 0.0) {
        p.setFont(QFont(p.font().family(), 10));
        p.drawText(pieRect, Qt::AlignCenter, "No data");
        return img;
    }

    int startAngle = 0;
    for (int i = 0; i < values.size(); ++i) {
        double fraction = values[i] / total;
        int span = static_cast<int>(std::round(fraction * 360.0 * 16.0));
        p.setBrush(colors.value(i, Qt::gray));
        p.setPen(Qt::NoPen);
        p.drawPie(pieRect, startAngle, span);
        startAngle += span;
    }

    QRect legendRect(pieRect.right() + 20, pieRect.top(), size.width() - pieRect.right() - 30, pieRect.height());
    p.setPen(Qt::black);
    QFont legendFont = p.font();
    legendFont.setPointSize(9);
    legendFont.setBold(false);
    p.setFont(legendFont);
    int y = legendRect.top();
    for (int i = 0; i < labels.size(); ++i) {
        double percent = values[i] / total * 100.0;
        QRect colorBox(legendRect.left(), y + 4, 12, 12);
        p.fillRect(colorBox, colors.value(i, Qt::gray));
        p.drawRect(colorBox);
        QString text = QString("%1  %2% (%3)")
            .arg(labels[i])
            .arg(percent, 0, 'f', 1)
            .arg(static_cast<int>(values[i]));
        p.drawText(legendRect.left() + 18, y + 14, text);
        y += 20;
    }

    return img;
}
