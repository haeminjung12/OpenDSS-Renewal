#pragma once

#include <QObject>

namespace desktop_app::v2 {

class ApplicationStateStore;
class SettingsRepository;

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int textSizePercent READ textSizePercent NOTIFY textSizePercentChanged)

public:
    SettingsController(SettingsRepository &repository, ApplicationStateStore &stateStore,
                       QObject *parent = nullptr);

    int textSizePercent() const;
    Q_INVOKABLE void setTextSizePercent(int textSizePercent);

signals:
    void textSizePercentChanged();

private:
    SettingsRepository &repository_;
    ApplicationStateStore &stateStore_;
    int lastNotifiedTextSizePercent_;
};

} // namespace desktop_app::v2
