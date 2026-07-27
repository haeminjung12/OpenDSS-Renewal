#include "settings_controller.h"

#include "settings_repository.h"
#include "../run/run_manifest_v2.h"
#include "../sequence/sequence_manifest_v2.h"
#include "../state/application_state_store.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>

#include <array>
#include <utility>

namespace desktop_app::v2 {

namespace {

constexpr std::array<OutputRootSelector, 9> kOutputRootSelectors{
    OutputRootSelector::CaptureSingle,
    OutputRootSelector::CaptureSequence,
    OutputRootSelector::CaptureDataset,
    OutputRootSelector::Train,
    OutputRootSelector::ModelTest,
    OutputRootSelector::Live,
    OutputRootSelector::SequenceTest,
    OutputRootSelector::LibraryCreate,
    OutputRootSelector::LibraryExport};

QString cameraAvailabilityText(const CameraState &camera)
{
    const QString deviceId = camera.deviceId.trimmed();
    switch (camera.status) {
    case CameraStatus::Ready:
        return deviceId.isEmpty() ? QStringLiteral("Available — ready")
                                  : QStringLiteral("Available — ready (%1)").arg(deviceId);
    case CameraStatus::Streaming:
        return deviceId.isEmpty() ? QStringLiteral("Available — streaming")
                                  : QStringLiteral("Available — streaming (%1)").arg(deviceId);
    case CameraStatus::Faulted: {
        const QString fault = camera.fault.trimmed();
        return fault.isEmpty() ? QStringLiteral("Unavailable — camera fault")
                               : QStringLiteral("Unavailable — %1").arg(fault);
    }
    case CameraStatus::Unavailable:
        return QStringLiteral("Unavailable");
    }
    return QStringLiteral("Unavailable");
}

QString daqAvailabilityText(const DaqState &daq)
{
    const QString deviceId = daq.deviceId.trimmed();
    switch (daq.status) {
    case DaqStatus::Ready:
        return deviceId.isEmpty() ? QStringLiteral("Available — ready")
                                  : QStringLiteral("Available — ready (%1)").arg(deviceId);
    case DaqStatus::Busy:
        return deviceId.isEmpty() ? QStringLiteral("Available — busy")
                                  : QStringLiteral("Available — busy (%1)").arg(deviceId);
    case DaqStatus::Faulted: {
        const QString fault = daq.fault.trimmed();
        return fault.isEmpty() ? QStringLiteral("Unavailable — DAQ fault")
                               : QStringLiteral("Unavailable — %1").arg(fault);
    }
    case DaqStatus::Disabled:
        return QStringLiteral("Unavailable — disabled");
    }
    return QStringLiteral("Unavailable");
}

QString injectedAvailability(const QString &availability)
{
    const QString trimmed = availability.trimmed();
    return trimmed.isEmpty()
        ? QStringLiteral("Unavailable — diagnostics not initialized") : trimmed;
}

} // namespace

SettingsController::SettingsController(SettingsRepository &repository, ApplicationStateStore &stateStore,
                                       FolderOpener folderOpener, QObject *parent)
    : QObject(parent)
    , repository_(repository)
    , stateStore_(stateStore)
    , folderOpener_(std::move(folderOpener))
    , lastNotifiedTextSizePercent_(textSizePercent())
    , lastNotifiedStorageRoot_(stateStore_.snapshot().preferences.storageRoot)
    , lastNotifiedOutputRootsState_(outputRootsState())
{
    if (!folderOpener_)
        folderOpener_ = [](const QUrl &url) { return QDesktopServices::openUrl(url); };
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
        emit applicationInformationChanged();
    });
}

int SettingsController::textSizePercent() const
{
    return stateStore_.snapshot().preferences.textSizePercent;
}

void SettingsController::setTextSizePercent(int textSizePercent)
{
    if (repository_.setTextSizePercent(textSizePercent))
        notifyOutputRootsIfChanged();
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
    notifyOutputRootsIfChanged();
    return {};
}

QString SettingsController::openStorageRoot() const
{
    return openExistingFolder(storageRoot(), {});
}

QUrl SettingsController::captureSingleOutputRoot() const
{
    return outputRoot(OutputRootSelector::CaptureSingle);
}

QString SettingsController::setCaptureSingleOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::CaptureSingle, outputRoot);
}

QUrl SettingsController::captureSequenceOutputRoot() const
{
    return outputRoot(OutputRootSelector::CaptureSequence);
}

QString SettingsController::setCaptureSequenceOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::CaptureSequence, outputRoot);
}

QUrl SettingsController::captureDatasetOutputRoot() const
{
    return outputRoot(OutputRootSelector::CaptureDataset);
}

QString SettingsController::setCaptureDatasetOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::CaptureDataset, outputRoot);
}

QUrl SettingsController::trainOutputRoot() const
{
    return outputRoot(OutputRootSelector::Train);
}

QString SettingsController::setTrainOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::Train, outputRoot);
}

QUrl SettingsController::modelTestOutputRoot() const
{
    return outputRoot(OutputRootSelector::ModelTest);
}

QString SettingsController::setModelTestOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::ModelTest, outputRoot);
}

QUrl SettingsController::liveOutputRoot() const
{
    return outputRoot(OutputRootSelector::Live);
}

QString SettingsController::setLiveOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::Live, outputRoot);
}

QUrl SettingsController::sequenceTestOutputRoot() const
{
    return outputRoot(OutputRootSelector::SequenceTest);
}

QString SettingsController::setSequenceTestOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::SequenceTest, outputRoot);
}

QUrl SettingsController::libraryCreateOutputRoot() const
{
    return outputRoot(OutputRootSelector::LibraryCreate);
}

QString SettingsController::setLibraryCreateOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::LibraryCreate, outputRoot);
}

QUrl SettingsController::libraryExportOutputRoot() const
{
    return outputRoot(OutputRootSelector::LibraryExport);
}

QString SettingsController::setLibraryExportOutputRoot(const QUrl &outputRoot)
{
    return setOutputRoot(OutputRootSelector::LibraryExport, outputRoot);
}

QUrl SettingsController::outputRoot(OutputRootSelector selector) const
{
    return QUrl::fromLocalFile(repository_.outputRoot(selector));
}

bool SettingsController::outputRootFellBack(OutputRootSelector selector) const
{
    return repository_.outputRootFellBack(selector);
}

QString SettingsController::outputRootFallbackReason(OutputRootSelector selector) const
{
    return repository_.outputRootFallbackReason(selector);
}

QString SettingsController::setOutputRoot(OutputRootSelector selector,
                                          const QUrl &outputRoot)
{
    if (!outputRoot.isLocalFile() || outputRoot.hasQuery() || outputRoot.hasFragment())
        return QStringLiteral("Output location must be a local folder URL.");

    const QString localOutputRoot = outputRoot.toLocalFile();
    if (localOutputRoot.isEmpty())
        return QStringLiteral("Output location must be a local folder URL.");

    QString error;
    if (!repository_.setOutputRoot(selector, localOutputRoot, &error))
        return error;
    notifyOutputRootsIfChanged();
    return {};
}

QString SettingsController::applicationVersion() const
{
    const QString version = QCoreApplication::applicationVersion().trimmed();
    return version.isEmpty() ? QStringLiteral("Unavailable") : version;
}

QString SettingsController::schemaVersions() const
{
    return QStringLiteral("%1; %2").arg(
        QString::fromLatin1(sequence::SequenceManifestV2::SchemaVersion),
        QString::fromLatin1(run::RunManifestV2::SchemaVersion));
}

QString SettingsController::runtimeAvailability() const
{
    return injectedAvailability(diagnostics_.runtimeAvailability);
}

QString SettingsController::cameraDriverAvailability() const
{
    return cameraAvailabilityText(stateStore_.snapshot().camera);
}

QString SettingsController::daqDriverAvailability() const
{
    return daqAvailabilityText(stateStore_.snapshot().daq);
}

QString SettingsController::gpuEnvironmentAvailability() const
{
    return injectedAvailability(diagnostics_.gpuEnvironmentAvailability);
}

QUrl SettingsController::diagnosticFolder() const
{
    return diagnostics_.diagnosticFolder.trimmed().isEmpty()
        ? QUrl{} : QUrl::fromLocalFile(diagnostics_.diagnosticFolder);
}

bool SettingsController::canOpenDiagnosticFolder() const
{
    return diagnosticFolderUnavailableReason().isEmpty();
}

QString SettingsController::diagnosticFolderUnavailableReason() const
{
    const QUrl folder = diagnosticFolder();
    if (folder.isEmpty() || !folder.isValid() || !folder.isLocalFile())
        return QStringLiteral("Diagnostic folder is unavailable.");
    const QFileInfo info(folder.toLocalFile());
    if (!info.exists() || !info.isDir())
        return QStringLiteral("Diagnostic folder does not exist.");
    return {};
}

QString SettingsController::openDiagnosticFolder() const
{
    return openExistingFolder(diagnosticFolder(),
                              diagnosticFolderUnavailableReason());
}

void SettingsController::setDiagnostics(SettingsDiagnostics diagnostics)
{
    if (diagnostics_.runtimeAvailability == diagnostics.runtimeAvailability
        && diagnostics_.gpuEnvironmentAvailability == diagnostics.gpuEnvironmentAvailability
        && diagnostics_.diagnosticFolder == diagnostics.diagnosticFolder) {
        return;
    }
    diagnostics_ = std::move(diagnostics);
    emit applicationInformationChanged();
}

QString SettingsController::openExistingFolder(
    const QUrl &folder, const QString &unavailableReason) const
{
    if (!unavailableReason.isEmpty())
        return unavailableReason;
    if (folder.isEmpty() || !folder.isValid() || !folder.isLocalFile())
        return QStringLiteral("Folder is unavailable.");
    const QFileInfo info(folder.toLocalFile());
    if (!info.exists() || !info.isDir())
        return QStringLiteral("Folder is not an existing directory.");
    if (!folderOpener_(folder))
        return QStringLiteral("Unable to request opening the folder.");
    return {};
}

QString SettingsController::outputRootsState() const
{
    QString state;
    for (const OutputRootSelector selector : kOutputRootSelectors) {
        state += repository_.outputRoot(selector);
        state += QChar(0x1f);
        state += repository_.outputRootFellBack(selector)
            ? QLatin1Char('1') : QLatin1Char('0');
        state += QChar(0x1f);
        state += repository_.outputRootFallbackReason(selector);
        state += QChar(0x1e);
    }
    return state;
}

void SettingsController::notifyOutputRootsIfChanged()
{
    const QString currentState = outputRootsState();
    if (currentState == lastNotifiedOutputRootsState_)
        return;
    lastNotifiedOutputRootsState_ = currentState;
    emit outputRootsChanged();
}

} // namespace desktop_app::v2
