#include "../v2/settings/settings_controller.h"
#include "../v2/settings/settings_repository.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <iostream>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

class UrlCaptureHandler final : public QObject
{
    Q_OBJECT

public:
    QUrl capturedUrl;

public slots:
    void capture(const QUrl &url)
    {
        capturedUrl = url;
    }
};

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
    if (desktop_app::v2::SettingsController::staticMetaObject.indexOfMethod("openStorageRoot()") < 0)
        return fail(2, "Controller did not expose the storage-root opening API.");
    int textSizeChangedCount = 0;
    int storageRootChangedCount = 0;
    QObject::connect(&controller, &desktop_app::v2::SettingsController::textSizePercentChanged,
                     &controller, [&textSizeChangedCount] { ++textSizeChangedCount; });
    QObject::connect(&controller, &desktop_app::v2::SettingsController::storageRootChanged,
                     &controller, [&storageRootChangedCount] { ++storageRootChangedCount; });

    if (controller.textSizePercent() != 100)
        return fail(3, "Controller did not expose the default text size.");
    if (!controller.storageRoot().isLocalFile()
        || controller.storageRoot().toLocalFile() != temporaryDirectory.path()
        || controller.storageRoot().isRelative()) {
        return fail(3, "Controller did not expose the stored root as an absolute local URL.");
    }

    const QString alternateRoot = temporaryDirectory.filePath(QStringLiteral("alternate-root"));
    if (!QDir().mkpath(alternateRoot)
        || !controller.setStorageRoot(QUrl::fromLocalFile(alternateRoot)).isEmpty()
        || controller.storageRoot().toLocalFile() != alternateRoot || storageRootChangedCount != 1) {
        return fail(4, "Controller did not persist and publish a valid storage root.");
    }
    if (!controller.setStorageRoot(QUrl::fromLocalFile(alternateRoot)).isEmpty()
        || storageRootChangedCount != 1) {
        return fail(4, "Controller emitted for an unchanged storage root.");
    }
    desktop_app::v2::ApplicationStateStore reloadStore;
    desktop_app::v2::SettingsRepository reloadRepository(
        temporaryDirectory.filePath(QStringLiteral("preferences.json")), reloadStore);
    if (!reloadRepository.load() || reloadStore.snapshot().preferences.storageRoot != alternateRoot
        || reloadStore.snapshot().preferences.textSizePercent != 100) {
        return fail(4, "Controller storage-root persistence did not reload.");
    }

    const QString priorStorageRoot = controller.storageRoot().toLocalFile();
    const QUrl missingRoot = QUrl::fromLocalFile(temporaryDirectory.filePath(QStringLiteral("missing-root")));
    if (controller.setStorageRoot(missingRoot).isEmpty()
        || controller.storageRoot().toLocalFile() != priorStorageRoot || storageRootChangedCount != 1) {
        return fail(5, "Controller accepted an invalid storage-root URL.");
    }
    if (controller.setStorageRoot(QUrl(QStringLiteral("https://example.invalid/root"))).isEmpty()
        || controller.storageRoot().toLocalFile() != priorStorageRoot || storageRootChangedCount != 1) {
        return fail(5, "Controller accepted a nonlocal storage-root URL.");
    }

    UrlCaptureHandler urlCaptureHandler;
    QDesktopServices::setUrlHandler(QStringLiteral("file"), &urlCaptureHandler, "capture");
    const QString openStorageRootError = controller.openStorageRoot();
    QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
    if (!openStorageRootError.isEmpty() || urlCaptureHandler.capturedUrl != controller.storageRoot())
        return fail(5, "Controller did not forward the authoritative storage-root URL.");

    desktop_app::v2::ApplicationStateStore invalidRootStore;
    desktop_app::v2::SettingsRepository invalidRootRepository(
        temporaryDirectory.filePath(QStringLiteral("invalid-preferences.json")), invalidRootStore);
    desktop_app::v2::SettingsController invalidRootController(invalidRootRepository, invalidRootStore);
    if (invalidRootController.openStorageRoot().isEmpty())
        return fail(5, "Controller attempted to open an empty storage root.");
    invalidRootStore.publishPreferences(
        {temporaryDirectory.filePath(QStringLiteral("missing-root")), 100});
    if (invalidRootController.openStorageRoot().isEmpty())
        return fail(5, "Controller attempted to open a nonexistent storage root.");

    controller.setTextSizePercent(80);
    if (controller.textSizePercent() != 80 || textSizeChangedCount != 1)
        return fail(6, "Controller did not publish a supported text size.");
    controller.setTextSizePercent(150);
    if (controller.textSizePercent() != 125 || textSizeChangedCount != 2)
        return fail(7, "Controller did not publish normalized legacy text size.");
    controller.setTextSizePercent(90);
    if (controller.textSizePercent() != 100 || textSizeChangedCount != 3)
        return fail(8, "Controller did not normalize the legacy medium value.");
    controller.setTextSizePercent(110);
    if (controller.textSizePercent() != 100 || textSizeChangedCount != 3)
        return fail(9, "Controller published an unsupported text size.");

    const QString failedSaveRoot = temporaryDirectory.filePath(QStringLiteral("failed-save-root"));
    if (!QDir().mkpath(failedSaveRoot))
        return fail(10, "Unable to prepare failed-save storage root.");
    const QString preferencesPath = temporaryDirectory.filePath(QStringLiteral("preferences.json"));
    if (!QFile::remove(preferencesPath) || !QDir().mkdir(preferencesPath)
        || controller.setStorageRoot(QUrl::fromLocalFile(failedSaveRoot)).isEmpty()
        || controller.storageRoot().toLocalFile() != priorStorageRoot || storageRootChangedCount != 1) {
        return fail(10, "Failed storage-root persistence published a candidate state.");
    }

    return 0;
}

#include "settings_controller_test.moc"
