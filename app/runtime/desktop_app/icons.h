#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

QIcon makeBrandIcon(const QString& key,
                    const QColor& fg = QColor("#FFFFFF"),
                    const QColor& accent = QColor("#7DD3FC"),
                    const QColor& fill = QColor(0, 0, 0, 0));
