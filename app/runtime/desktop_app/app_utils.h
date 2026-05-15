#pragma once

#include <QColor>
#include <QComboBox>
#include <QImage>
#include <QJsonObject>
#include <QSize>
#include <QString>
#include <QVector>

QString findProjectRootFromApp();
QString runOutputBaseForSettings(const QString& outputDir);
void setComboTextIfPresent(QComboBox* combo, const QString& text);
QJsonObject comboSnapshot(QComboBox* combo);
QString formatTimeSeconds(double seconds);
QImage renderPieChart(const QString& title,
                      const QVector<QString>& labels,
                      const QVector<double>& values,
                      const QVector<QColor>& colors,
                      const QSize& size = QSize(520, 420));
