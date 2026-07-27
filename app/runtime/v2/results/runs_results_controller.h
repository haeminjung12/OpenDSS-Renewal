#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace desktop_app::v2 {

class ApplicationStateStore;

namespace results {

class RunRepository;

class RunsResultsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList runs READ runs NOTIFY runsChanged)
    Q_PROPERTY(QString selectedRunId READ selectedRunId NOTIFY selectedRunIdChanged)
    Q_PROPERTY(QVariantMap loadedRun READ loadedRun NOTIFY loadedRunChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    using ArtifactOpener = std::function<bool(const QUrl &)>;
    using StandardRunsRootProvider = std::function<QString()>;

    RunsResultsController(RunRepository &repository, ApplicationStateStore &stateStore,
                          ArtifactOpener artifactOpener = {},
                          StandardRunsRootProvider standardRunsRootProvider = {},
                          QObject *parent = nullptr);

    QVariantList runs() const;
    QString selectedRunId() const;
    QVariantMap loadedRun() const;
    QString errorMessage() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool refreshRoots(const QString &liveRunRoot,
                                  const QUrl &sequenceRunRoot);
    Q_INVOKABLE bool selectRun(const QString &id);
    Q_INVOKABLE bool loadSelected();
    Q_INVOKABLE bool removeSelected();
    Q_INVOKABLE bool updateLoadedNotes(const QString &notes);
    Q_INVOKABLE bool openDropletLog();
    Q_INVOKABLE bool openRunFolder();
    Q_INVOKABLE bool openDropletCrop(const QUrl &cropUrl);
    Q_INVOKABLE bool openSavedSequence();
    Q_INVOKABLE bool openRunSummary(const QUrl &summaryUrl);

signals:
    void runsChanged();
    void selectedRunIdChanged();
    void loadedRunChanged();
    void errorMessageChanged();
    void savedSequenceRequested(const QString &manifestPath);

private:
    void publishError(const QString &message);
    QString effectiveRoot(const QString &requestedRoot) const;
    bool openExistingPath(const QString &path, bool requireDirectory,
                          const QString &unavailableMessage);

    RunRepository &repository_;
    ApplicationStateStore &stateStore_;
    ArtifactOpener artifactOpener_;
    StandardRunsRootProvider standardRunsRootProvider_;
    QString liveRunRoot_;
    QString sequenceRunRoot_;
};

} // namespace results
} // namespace desktop_app::v2
