#pragma once

#include <QFrame>
#include <QString>

class QBoxLayout;

class PanelFrame : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)

public:
    explicit PanelFrame(QWidget* parent = nullptr);
    explicit PanelFrame(const QString& title, QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& title);

    QWidget* bodyWidget() const;
    QBoxLayout* bodyLayout() const;
    void addWidget(QWidget* widget);

private:
    void updateTitleVisibility();

    class QLabel* titleLabel_ = nullptr;
    QWidget* body_ = nullptr;
    QBoxLayout* bodyLayout_ = nullptr;
};
