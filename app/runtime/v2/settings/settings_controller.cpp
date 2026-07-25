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
{
    connect(&stateStore_, &ApplicationStateStore::changed, this, [this] {
        const int currentTextSizePercent = textSizePercent();
        if (currentTextSizePercent == lastNotifiedTextSizePercent_)
            return;
        lastNotifiedTextSizePercent_ = currentTextSizePercent;
        emit textSizePercentChanged();
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

} // namespace desktop_app::v2
