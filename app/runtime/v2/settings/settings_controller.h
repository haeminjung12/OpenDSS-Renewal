#pragma once

#include "settings_repository.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

namespace desktop_app::v2 {

class ApplicationStateStore;

struct SettingsDiagnostics {
    QString runtimeAvailability;
    QString gpuEnvironmentAvailability;
    QString diagnosticFolder;
};

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int textSizePercent READ textSizePercent NOTIFY textSizePercentChanged)
    Q_PROPERTY(QUrl storageRoot READ storageRoot NOTIFY storageRootChanged)
    Q_PROPERTY(QString applicationVersion READ applicationVersion NOTIFY applicationInformationChanged)
    Q_PROPERTY(QString schemaVersions READ schemaVersions CONSTANT)
    Q_PROPERTY(QString runtimeAvailability READ runtimeAvailability NOTIFY applicationInformationChanged)
    Q_PROPERTY(QString cameraDriverAvailability READ cameraDriverAvailability NOTIFY applicationInformationChanged)
    Q_PROPERTY(QString daqDriverAvailability READ daqDriverAvailability NOTIFY applicationInformationChanged)
    Q_PROPERTY(QString gpuEnvironmentAvailability READ gpuEnvironmentAvailability NOTIFY applicationInformationChanged)
    Q_PROPERTY(QUrl diagnosticFolder READ diagnosticFolder NOTIFY applicationInformationChanged)
    Q_PROPERTY(bool canOpenDiagnosticFolder READ canOpenDiagnosticFolder NOTIFY applicationInformationChanged)
    Q_PROPERTY(QString diagnosticFolderUnavailableReason READ diagnosticFolderUnavailableReason NOTIFY applicationInformationChanged)

public:
    using FolderOpener = std::function<bool(const QUrl &)>;

    SettingsController(SettingsRepository &repository, ApplicationStateStore &stateStore,
                       FolderOpener folderOpener = {}, QObject *parent = nullptr);

    int textSizePercent() const;
    Q_INVOKABLE void setTextSizePercent(int textSizePercent);
    QUrl storageRoot() const;
    Q_INVOKABLE QString setStorageRoot(const QUrl &storageRoot);
    Q_INVOKABLE QString openStorageRoot() const;
    QUrl outputRoot(OutputRootSelector selector) const;
    bool outputRootFellBack(OutputRootSelector selector) const;
    QString outputRootFallbackReason(OutputRootSelector selector) const;
    QString setOutputRoot(OutputRootSelector selector, const QUrl &outputRoot);
    QString applicationVersion() const;
    QString schemaVersions() const;
    QString runtimeAvailability() const;
    QString cameraDriverAvailability() const;
    QString daqDriverAvailability() const;
    QString gpuEnvironmentAvailability() const;
    QUrl diagnosticFolder() const;
    bool canOpenDiagnosticFolder() const;
    QString diagnosticFolderUnavailableReason() const;
    Q_INVOKABLE QString openDiagnosticFolder() const;

    void setDiagnostics(SettingsDiagnostics diagnostics);

signals:
    void textSizePercentChanged();
    void storageRootChanged();
    void outputRootsChanged();
    void applicationInformationChanged();

private:
    QString openExistingFolder(const QUrl &folder, const QString &unavailableReason) const;
    QString outputRootsState() const;
    void notifyOutputRootsIfChanged();

    SettingsRepository &repository_;
    ApplicationStateStore &stateStore_;
    FolderOpener folderOpener_;
    SettingsDiagnostics diagnostics_;
    int lastNotifiedTextSizePercent_;
    QString lastNotifiedStorageRoot_;
    QString lastNotifiedOutputRootsState_;
};

} // namespace desktop_app::v2
