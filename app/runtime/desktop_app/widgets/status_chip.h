#pragma once

#include <QString>
#include <QWidget>

class QLabel;

class StatusChip : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QString state READ stateName WRITE setStateName)
    Q_PROPERTY(bool dotVisible READ isDotVisible WRITE setDotVisible)

public:
    enum class State {
        Neutral,
        Info,
        Success,
        Warning,
        Error,
        Running,
        Armed,
        Disabled
    };
    Q_ENUM(State)

    explicit StatusChip(QWidget* parent = nullptr);
    explicit StatusChip(const QString& text, State state = State::Neutral, QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);

    State state() const;
    void setState(State state);

    QString stateName() const;
    void setStateName(const QString& state);

    bool isDotVisible() const;
    void setDotVisible(bool visible);

private:
    void updateStatePresentation();
    void updateAccessibleLabel();

    QWidget* dot_ = nullptr;
    QLabel* label_ = nullptr;
    State state_ = State::Neutral;
    bool dotVisible_ = true;
};
