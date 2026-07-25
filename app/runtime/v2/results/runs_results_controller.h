#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

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
    RunsResultsController(RunRepository &repository, ApplicationStateStore &stateStore,
                          QObject *parent = nullptr);

    QVariantList runs() const;
    QString selectedRunId() const;
    QVariantMap loadedRun() const;
    QString errorMessage() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool selectRun(const QString &id);
    Q_INVOKABLE bool loadSelected();
    Q_INVOKABLE bool updateLoadedNotes(const QString &notes);

signals:
    void runsChanged();
    void selectedRunIdChanged();
    void loadedRunChanged();
    void errorMessageChanged();

private:
    void publishError(const QString &message);

    RunRepository &repository_;
    ApplicationStateStore &stateStore_;
};

} // namespace results
} // namespace desktop_app::v2
