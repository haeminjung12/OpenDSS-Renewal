#include "../v2/settings/settings_controller.h"
#include "../v2/settings/settings_repository.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <iostream>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid())
        return fail(1, "Unable to create temporary directory.");

    desktop_app::v2::ApplicationStateStore store;
    desktop_app::v2::SettingsRepository repository(
        temporaryDirectory.filePath(QStringLiteral("preferences.json")), store);
    if (!repository.load() || !repository.setStorageRoot(temporaryDirectory.path()))
        return fail(2, "Unable to establish test preferences.");

    desktop_app::v2::SettingsController controller(repository, store);
    int changedCount = 0;
    QObject::connect(&controller, &desktop_app::v2::SettingsController::textSizePercentChanged,
                     &controller, [&changedCount] { ++changedCount; });

    if (controller.textSizePercent() != 100)
        return fail(3, "Controller did not expose the default text size.");
    controller.setTextSizePercent(80);
    if (controller.textSizePercent() != 80 || changedCount != 1)
        return fail(4, "Controller did not publish a supported text size.");
    controller.setTextSizePercent(150);
    if (controller.textSizePercent() != 125 || changedCount != 2)
        return fail(5, "Controller did not publish normalized legacy text size.");
    controller.setTextSizePercent(90);
    if (controller.textSizePercent() != 100 || changedCount != 3)
        return fail(6, "Controller did not normalize the legacy medium value.");
    controller.setTextSizePercent(110);
    if (controller.textSizePercent() != 100 || changedCount != 3)
        return fail(7, "Controller published an unsupported text size.");

    return 0;
}
