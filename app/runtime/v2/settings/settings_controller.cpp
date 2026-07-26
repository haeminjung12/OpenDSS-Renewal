#include "settings_controller.h"

#include "settings_repository.h"
#include "../run/run_manifest_v2.h"
#include "../sequence/sequence_manifest_v2.h"
#include "../state/application_state_store.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>

#include <utility>

namespace desktop_app::v2 {

namespace {

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

QString SettingsController::openStorageRoot() const
{
    return openExistingFolder(storageRoot(), {});
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

} // namespace desktop_app::v2
