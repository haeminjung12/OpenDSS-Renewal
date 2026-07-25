#include "settings_controller.h"

#include "settings_repository.h"
#include "../state/application_state_store.h"

namespace desktop_app::v2 {

SettingsController::SettingsController(SettingsRepository &repository, ApplicationStateStore &stateStore,
                                       QObject *parent)
    : QObject(parent)
    , repository_(repository)
    , stateStore_(stateStore)
    , lastNotifiedTextSizePercent_(textSizePercent())
    , lastNotifiedStorageRoot_(stateStore_.snapshot().preferences.storageRoot)
{
    connect(&stateStore_, &ApplicationStateStore::changed, this, [this] {
        const int currentTextSizePercent = textSizePercent();
        if (currentTextSizePercent != lastNotifiedTextSizePercent_) {
            lastNotifiedTextSizePercent_ = currentTextSizePercent;
            emit textSizePercentChanged();
        }

        const QString currentStorageRoot = stateStore_.snapshot().preferences.storageRoot;
        if (currentStorageRoot != lastNotifiedStorageRoot_) {
            lastNotifiedStorageRoot_ = currentStorageRoot;
            emit storageRootChanged();
        }
    });
}

int SettingsController::textSizePercent() const
{
    return stateStore_.snapshot().preferences.textSizePercent;
}

void SettingsController::setTextSizePercent(int textSizePercent)
{
    repository_.setTextSizePercent(textSizePercent);
}

QUrl SettingsController::storageRoot() const
{
    return QUrl::fromLocalFile(stateStore_.snapshot().preferences.storageRoot);
}

QString SettingsController::setStorageRoot(const QUrl &storageRoot)
{
    if (!storageRoot.isLocalFile() || storageRoot.hasQuery() || storageRoot.hasFragment())
        return QStringLiteral("Storage root must be a local folder URL.");

    const QString localStorageRoot = storageRoot.toLocalFile();
    if (localStorageRoot.isEmpty())
        return QStringLiteral("Storage root must be a local folder URL.");

    QString error;
    if (!repository_.setStorageRoot(localStorageRoot, &error))
        return error;
    return {};
}

} // namespace desktop_app::v2
