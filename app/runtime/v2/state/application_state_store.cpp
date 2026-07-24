#include "application_state_store.h"

#include <QReadLocker>
#include <QWriteLocker>

#include <utility>

namespace desktop_app::v2 {

ApplicationStateStore::ApplicationStateStore(QObject *parent)
    : QObject(parent)
{
}

ApplicationSnapshot ApplicationStateStore::snapshot() const
{
    QReadLocker locker(&lock_);
    return snapshot_;
}

void ApplicationStateStore::publishCamera(CameraState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.camera = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishDaq(DaqState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.daq = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishActiveModel(ActiveModelState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.activeModel = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishDataset(DatasetState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.dataset = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishSequence(SequenceState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.sequence = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishTraining(TrainingState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.training = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishRun(RunState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.run = std::move(state);
    }
    emit changed();
}

void ApplicationStateStore::publishPreferences(PreferencesState state)
{
    {
        QWriteLocker locker(&lock_);
        snapshot_.preferences = std::move(state);
    }
    emit changed();
}

} // namespace desktop_app::v2
