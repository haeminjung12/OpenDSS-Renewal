#pragma once

#include "../run/run_manifest_v2.h"
#include "../state/domain_state.h"

#include <QJsonObject>

#include <memory>
#include <mutex>

class DaqTrigger;

namespace desktop_app::v2 {

class ApplicationStateStore;
class OperationCoordinator;

class DaqService final
{
public:
    DaqService(OperationCoordinator &operations, ApplicationStateStore &stateStore);
    ~DaqService();

    DaqService(const DaqService &) = delete;
    DaqService &operator=(const DaqService &) = delete;

    bool applySettings(const DaqAppliedSettings &settings, QString *error = nullptr);
    bool ready() const;
    QJsonObject settingsSnapshot() const;
    run::DaqPulseStatus issueLiveHit(bool outputEnabled, QString *error = nullptr);
    void shutdown();

private:
    DaqState updateStateLocked(DaqStatus status, const QString &fault = {});

    OperationCoordinator &operations_;
    ApplicationStateStore &stateStore_;
    std::mutex operationOrderMutex_;
    mutable std::mutex stateMutex_;
    std::unique_ptr<DaqTrigger> trigger_;
    DaqState state_;
};

} // namespace desktop_app::v2
