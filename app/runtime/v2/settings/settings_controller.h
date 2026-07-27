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
    Q_PROPERTY(QUrl captureSingleOutputRoot READ captureSingleOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl captureSequenceOutputRoot READ captureSequenceOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl captureDatasetOutputRoot READ captureDatasetOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl trainOutputRoot READ trainOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl modelTestOutputRoot READ modelTestOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl liveOutputRoot READ liveOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl sequenceTestOutputRoot READ sequenceTestOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl libraryCreateOutputRoot READ libraryCreateOutputRoot NOTIFY outputRootsChanged)
    Q_PROPERTY(QUrl libraryExportOutputRoot READ libraryExportOutputRoot NOTIFY outputRootsChanged)
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
    QUrl captureSingleOutputRoot() const;
    Q_INVOKABLE QString setCaptureSingleOutputRoot(const QUrl &outputRoot);
    QUrl captureSequenceOutputRoot() const;
    Q_INVOKABLE QString setCaptureSequenceOutputRoot(const QUrl &outputRoot);
    QUrl captureDatasetOutputRoot() const;
    Q_INVOKABLE QString setCaptureDatasetOutputRoot(const QUrl &outputRoot);
    QUrl trainOutputRoot() const;
    Q_INVOKABLE QString setTrainOutputRoot(const QUrl &outputRoot);
    QUrl modelTestOutputRoot() const;
    Q_INVOKABLE QString setModelTestOutputRoot(const QUrl &outputRoot);
    QUrl liveOutputRoot() const;
    Q_INVOKABLE QString setLiveOutputRoot(const QUrl &outputRoot);
    QUrl sequenceTestOutputRoot() const;
    Q_INVOKABLE QString setSequenceTestOutputRoot(const QUrl &outputRoot);
    QUrl libraryCreateOutputRoot() const;
    Q_INVOKABLE QString setLibraryCreateOutputRoot(const QUrl &outputRoot);
    QUrl libraryExportOutputRoot() const;
    Q_INVOKABLE QString setLibraryExportOutputRoot(const QUrl &outputRoot);
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
