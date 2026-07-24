#pragma once

#include "domain_state.h"

#include <QObject>
#include <QReadWriteLock>

namespace desktop_app::v2 {

class ApplicationStateStore final : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationStateStore(QObject *parent = nullptr);

    ApplicationSnapshot snapshot() const;

    void publishCamera(CameraState state);
    void publishDaq(DaqState state);
    void publishActiveModel(ActiveModelState state);
    void publishDataset(DatasetState state);
    void publishSequence(SequenceState state);
    void publishTraining(TrainingState state);
    void publishRun(RunState state);
    void publishResults(ResultsState state);
    void publishPreferences(PreferencesState state);

signals:
    void changed();

private:
    mutable QReadWriteLock lock_;
    ApplicationSnapshot snapshot_;
};

} // namespace desktop_app::v2
