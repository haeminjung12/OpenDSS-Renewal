#pragma once

#include <QString>
#include <QWidget>

class QBoxLayout;
class QLabel;
class QToolButton;

class CollapsibleSection : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(bool collapsed READ isCollapsed WRITE setCollapsed NOTIFY collapsedChanged)

  public:
    explicit CollapsibleSection(QWidget* parent = nullptr);
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& title);

    bool isCollapsed() const;
    void setCollapsed(bool collapsed);

    QWidget* contentWidget() const;
    void setContentWidget(QWidget* widget);

    QBoxLayout* contentLayout() const;
    void addWidget(QWidget* widget);

  signals:
    void collapsedChanged(bool collapsed);

  public slots:
    void toggleCollapsed();

  private:
    void updateHeader();
    void updateAccessibleLabel();

    QToolButton* toggleButton_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QWidget* content_ = nullptr;
    QBoxLayout* contentLayout_ = nullptr;
    bool collapsed_ = false;
};
