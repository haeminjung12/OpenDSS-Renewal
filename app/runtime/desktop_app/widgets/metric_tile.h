#pragma once

#include <QString>
#include <QWidget>

class QLabel;

class MetricTile : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(QString value READ value WRITE setValue)
    Q_PROPERTY(QString sublabel READ sublabel WRITE setSublabel)

  public:
    explicit MetricTile(QWidget* parent = nullptr);
    MetricTile(const QString& label, const QString& value, const QString& sublabel = QString(),
               QWidget* parent = nullptr);

    QString label() const;
    void setLabel(const QString& label);

    QString value() const;
    void setValue(const QString& value);

    QString sublabel() const;
    void setSublabel(const QString& sublabel);

  private:
    void updateAccessibleLabel();

    QLabel* label_ = nullptr;
    QLabel* value_ = nullptr;
    QLabel* sublabel_ = nullptr;
};
