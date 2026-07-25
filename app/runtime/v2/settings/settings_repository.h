#pragma once

#include "../state/domain_state.h"

#include <QString>

namespace desktop_app::v2 {

class ApplicationStateStore;

class SettingsRepository final
{
public:
    SettingsRepository(QString preferencesFilePath, ApplicationStateStore &stateStore);

    bool load(QString *error = nullptr);
    bool setStorageRoot(const QString &storageRoot, QString *error = nullptr);
    bool setTextSizePercent(int textSizePercent, QString *error = nullptr);

private:
    bool save(const PreferencesState &preferences, QString *error) const;

    QString preferencesFilePath_;
    ApplicationStateStore &stateStore_;
};

} // namespace desktop_app::v2
