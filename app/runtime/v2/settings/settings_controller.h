#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace desktop_app::v2 {

class ApplicationStateStore;
class SettingsRepository;

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int textSizePercent READ textSizePercent NOTIFY textSizePercentChanged)
    Q_PROPERTY(QUrl storageRoot READ storageRoot NOTIFY storageRootChanged)

public:
    SettingsController(SettingsRepository &repository, ApplicationStateStore &stateStore,
                       QObject *parent = nullptr);

    int textSizePercent() const;
    Q_INVOKABLE void setTextSizePercent(int textSizePercent);
    QUrl storageRoot() const;
    Q_INVOKABLE QString setStorageRoot(const QUrl &storageRoot);

signals:
    void textSizePercentChanged();
    void storageRootChanged();

private:
    SettingsRepository &repository_;
    ApplicationStateStore &stateStore_;
    int lastNotifiedTextSizePercent_;
    QString lastNotifiedStorageRoot_;
};

} // namespace desktop_app::v2
