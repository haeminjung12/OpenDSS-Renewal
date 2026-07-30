#include "../v2/settings/settings_controller.h"
#include "../v2/settings/settings_repository.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

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

    QVector<QUrl> openedUrls;
    desktop_app::v2::SettingsController controller(
        repository, store,
        [&openedUrls](const QUrl &url) {
            openedUrls.append(url);
            return true;
        });
    if (desktop_app::v2::SettingsController::staticMetaObject.indexOfMethod("openStorageRoot()") < 0)
        return fail(2, "Controller did not expose the storage-root opening API.");
    int textSizeChangedCount = 0;
    int storageRootChangedCount = 0;
    int outputRootsChangedCount = 0;
    QObject::connect(&controller, &desktop_app::v2::SettingsController::textSizePercentChanged,
                     &controller, [&textSizeChangedCount] { ++textSizeChangedCount; });
    QObject::connect(&controller, &desktop_app::v2::SettingsController::storageRootChanged,
                     &controller, [&storageRootChangedCount] { ++storageRootChangedCount; });
    QObject::connect(&controller, &desktop_app::v2::SettingsController::outputRootsChanged,
                     &controller, [&outputRootsChangedCount] { ++outputRootsChangedCount; });

    if (controller.textSizePercent() != 100)
        return fail(3, "Controller did not expose the default text size.");
    if (!controller.storageRoot().isLocalFile()
        || controller.storageRoot().toLocalFile() != temporaryDirectory.path()
        || controller.storageRoot().isRelative()) {
        return fail(3, "Controller did not expose the stored root as an absolute local URL.");
    }
    const QString expectedDatasetsRoot =
        QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                 .filePath(QStringLiteral("OpenDropletSortingSuite")))
            .filePath(QStringLiteral("datasets"));
    const QString expectedModelsRoot =
        QDir(QFileInfo(expectedDatasetsRoot).absolutePath())
            .filePath(QStringLiteral("models"));
    const QString expectedReportsRoot =
        QDir(QFileInfo(expectedDatasetsRoot).absolutePath())
            .filePath(QStringLiteral("reports"));
    const QString expectedRunsRoot =
        QDir(QFileInfo(expectedDatasetsRoot).absolutePath())
            .filePath(QStringLiteral("runs"));
    if (controller.captureSingleOutputRoot() != QUrl::fromLocalFile(expectedDatasetsRoot)
        || controller.captureSequenceOutputRoot() != QUrl::fromLocalFile(expectedDatasetsRoot)
        || controller.captureDatasetOutputRoot() != QUrl::fromLocalFile(expectedDatasetsRoot)
        || controller.trainOutputRoot() != QUrl::fromLocalFile(expectedModelsRoot)
        || controller.modelTestOutputRoot() != QUrl::fromLocalFile(expectedReportsRoot)
        || controller.liveOutputRoot() != QUrl::fromLocalFile(expectedRunsRoot)
        || controller.sequenceTestOutputRoot() != QUrl::fromLocalFile(expectedRunsRoot)
        || controller.libraryCreateOutputRoot() != QUrl::fromLocalFile(expectedModelsRoot)
        || controller.libraryExportOutputRoot() != QUrl::fromLocalFile(expectedModelsRoot)
        || controller.outputRootFellBack(
            desktop_app::v2::OutputRootSelector::CaptureSingle)
        || !controller.outputRootFallbackReason(
                desktop_app::v2::OutputRootSelector::CaptureSingle).isEmpty()) {
        return fail(3, "Controller did not expose the canonical output default.");
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
    const QString captureOutputRoot =
        temporaryDirectory.filePath(QStringLiteral("capture-output"));
    if (!QDir().mkpath(captureOutputRoot)
        || !controller.setCaptureSingleOutputRoot(
                QUrl::fromLocalFile(captureOutputRoot)).isEmpty()
        || controller.captureSingleOutputRoot().toLocalFile() != captureOutputRoot
        || controller.outputRootFellBack(
            desktop_app::v2::OutputRootSelector::CaptureSingle)
        || outputRootsChangedCount != 1) {
        return fail(4, "Controller did not persist a selected output root.");
    }
    if (!controller.setCaptureSingleOutputRoot(
            QUrl::fromLocalFile(captureOutputRoot)).isEmpty()
        || outputRootsChangedCount != 1) {
        return fail(4, "Controller emitted for an unchanged output root.");
    }
    const QString captureSequenceRoot =
        temporaryDirectory.filePath(QStringLiteral("capture-sequence"));
    const QString captureDatasetRoot =
        temporaryDirectory.filePath(QStringLiteral("capture-dataset"));
    const QString trainRoot =
        temporaryDirectory.filePath(QStringLiteral("train"));
    const QString modelTestRoot =
        temporaryDirectory.filePath(QStringLiteral("model-test"));
    const QString liveRoot =
        temporaryDirectory.filePath(QStringLiteral("live"));
    const QString sequenceTestRoot =
        temporaryDirectory.filePath(QStringLiteral("sequence-test"));
    const QString libraryCreateRoot =
        temporaryDirectory.filePath(QStringLiteral("library-create"));
    const QString libraryExportRoot =
        temporaryDirectory.filePath(QStringLiteral("library-export"));
    if (!QDir().mkpath(captureSequenceRoot)
        || !QDir().mkpath(captureDatasetRoot)
        || !QDir().mkpath(trainRoot)
        || !QDir().mkpath(modelTestRoot)
        || !QDir().mkpath(liveRoot)
        || !QDir().mkpath(sequenceTestRoot)
        || !QDir().mkpath(libraryCreateRoot)
        || !QDir().mkpath(libraryExportRoot)
        || !controller.setCaptureSequenceOutputRoot(
                QUrl::fromLocalFile(captureSequenceRoot)).isEmpty()
        || !controller.setCaptureDatasetOutputRoot(
                QUrl::fromLocalFile(captureDatasetRoot)).isEmpty()
        || !controller.setTrainOutputRoot(QUrl::fromLocalFile(trainRoot)).isEmpty()
        || !controller.setModelTestOutputRoot(
                QUrl::fromLocalFile(modelTestRoot)).isEmpty()
        || !controller.setLiveOutputRoot(QUrl::fromLocalFile(liveRoot)).isEmpty()
        || !controller.setSequenceTestOutputRoot(
                QUrl::fromLocalFile(sequenceTestRoot)).isEmpty()
        || !controller.setLibraryCreateOutputRoot(
                QUrl::fromLocalFile(libraryCreateRoot)).isEmpty()
        || !controller.setLibraryExportOutputRoot(
                QUrl::fromLocalFile(libraryExportRoot)).isEmpty()
        || controller.captureSequenceOutputRoot().toLocalFile() != captureSequenceRoot
        || controller.captureDatasetOutputRoot().toLocalFile() != captureDatasetRoot
        || controller.trainOutputRoot().toLocalFile() != trainRoot
        || controller.modelTestOutputRoot().toLocalFile() != modelTestRoot
        || controller.liveOutputRoot().toLocalFile() != liveRoot
        || controller.sequenceTestOutputRoot().toLocalFile() != sequenceTestRoot
        || controller.libraryCreateOutputRoot().toLocalFile() != libraryCreateRoot
        || controller.libraryExportOutputRoot().toLocalFile() != libraryExportRoot
        || outputRootsChangedCount != 9) {
        return fail(4, "Controller did not map each named output root independently.");
    }
    desktop_app::v2::ApplicationStateStore reloadStore;
    desktop_app::v2::SettingsRepository reloadRepository(
        temporaryDirectory.filePath(QStringLiteral("preferences.json")), reloadStore);
    if (!reloadRepository.load() || reloadStore.snapshot().preferences.storageRoot != alternateRoot
        || reloadStore.snapshot().preferences.textSizePercent != 100) {
        return fail(4, "Controller storage-root persistence did not reload.");
    }
    if (reloadRepository.outputRoot(
            desktop_app::v2::OutputRootSelector::CaptureSingle) != captureOutputRoot
        || reloadRepository.outputRoot(
               desktop_app::v2::OutputRootSelector::LibraryExport) != libraryExportRoot) {
        return fail(4, "Controller output-root persistence did not reload.");
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
    if (controller.setCaptureSingleOutputRoot(
            QUrl::fromLocalFile(temporaryDirectory.filePath(
                QStringLiteral("missing-output")))).isEmpty()
        || controller.setCaptureSingleOutputRoot(
            QUrl(QStringLiteral("https://example.invalid/output"))).isEmpty()
        || controller.captureSingleOutputRoot().toLocalFile() != captureOutputRoot
        || outputRootsChangedCount != 9) {
        return fail(5, "Controller accepted an invalid output root.");
    }
    if (!QDir().rmdir(captureOutputRoot)
        || !controller.outputRootFellBack(
            desktop_app::v2::OutputRootSelector::CaptureSingle)
        || controller.outputRootFallbackReason(
               desktop_app::v2::OutputRootSelector::CaptureSingle).isEmpty()
        || controller.outputRoot(
               desktop_app::v2::OutputRootSelector::CaptureSingle).toLocalFile()
            != expectedDatasetsRoot) {
        return fail(5, "Controller did not expose a factual unavailable-root fallback.");
    }
    controller.setTextSizePercent(80);
    if (controller.textSizePercent() != 80 || textSizeChangedCount != 1
        || outputRootsChangedCount != 10
        || !QDir().mkpath(captureOutputRoot)
        || controller.outputRoot(
               desktop_app::v2::OutputRootSelector::CaptureSingle).toLocalFile()
            != expectedDatasetsRoot
        || !controller.outputRootFellBack(
            desktop_app::v2::OutputRootSelector::CaptureSingle)
        || controller.outputRootFallbackReason(
               desktop_app::v2::OutputRootSelector::CaptureSingle).isEmpty()
        || outputRootsChangedCount != 10) {
        return fail(5, "Unrelated save did not reconcile one unavailable-root fallback.");
    }

    const QString openStorageRootError = controller.openStorageRoot();
    if (!openStorageRootError.isEmpty() || openedUrls.size() != 1 ||
        openedUrls.last() != controller.storageRoot())
        return fail(5, "Controller did not forward the authoritative storage-root URL.");

    const QString diagnosticFolder =
        temporaryDirectory.filePath(QStringLiteral("diagnostics"));
    if (!QDir().mkpath(diagnosticFolder))
        return fail(5, "Unable to create diagnostic folder fixture.");
    controller.setDiagnostics(
        {QStringLiteral("Available — Python runtime found"),
         QStringLiteral("Unavailable — CUDA provider not found"),
         diagnosticFolder});
    if (controller.runtimeAvailability() !=
            QStringLiteral("Available — Python runtime found") ||
        controller.gpuEnvironmentAvailability() !=
            QStringLiteral("Unavailable — CUDA provider not found") ||
        !controller.diagnosticFolder().isLocalFile() ||
        !controller.canOpenDiagnosticFolder() ||
        !controller.diagnosticFolderUnavailableReason().isEmpty() ||
        !controller.openDiagnosticFolder().isEmpty() ||
        openedUrls.size() != 2 ||
        openedUrls.last().toLocalFile() != diagnosticFolder) {
        return fail(5, "Controller did not expose or open injected diagnostics.");
    }

    store.publishCamera(
        {desktop_app::v2::CameraStatus::Ready, QStringLiteral("camera-001"), {},
         true, {}});
    desktop_app::v2::DaqState daq;
    daq.status = desktop_app::v2::DaqStatus::Faulted;
    daq.fault = QStringLiteral("NI driver missing");
    store.publishDaq(daq);
    if (!controller.cameraDriverAvailability().contains(
            QStringLiteral("Available — ready")) ||
        controller.daqDriverAvailability() !=
            QStringLiteral("Unavailable — NI driver missing")) {
        return fail(5, "Controller did not project factual Camera and DAQ readiness.");
    }

    controller.setDiagnostics(
        {QStringLiteral("Unavailable — Python runtime not found"),
         QStringLiteral("Unavailable — CUDA provider not found"),
         temporaryDirectory.filePath(QStringLiteral("missing-diagnostics"))});
    if (controller.canOpenDiagnosticFolder() ||
        controller.diagnosticFolderUnavailableReason().isEmpty() ||
        controller.openDiagnosticFolder().isEmpty() || openedUrls.size() != 2) {
        return fail(5, "Controller attempted to open an unavailable diagnostic folder.");
    }

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

