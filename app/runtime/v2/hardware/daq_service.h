#pragma once

#include "../run/run_manifest_v2.h"
#include "../operation/operation_coordinator.h"
#include "../state/domain_state.h"

#include <QJsonObject>

#include <memory>
#include <mutex>

namespace desktop_app::v2 {

class ApplicationStateStore;
class IDaqOutput;
class OperationCoordinator;

class DaqService final
{
public:
    DaqService(OperationCoordinator &operations, ApplicationStateStore &stateStore);
    DaqService(OperationCoordinator &operations, ApplicationStateStore &stateStore,
               std::unique_ptr<IDaqOutput> output);
    ~DaqService();

    DaqService(const DaqService &) = delete;
    DaqService &operator=(const DaqService &) = delete;

    static QString settingsValidationError(const DaqAppliedSettings &settings);

    bool applySettings(const DaqAppliedSettings &settings, QString *error = nullptr);
    void markUnavailable(const QString &reason);
    bool ready() const;
    QJsonObject settingsSnapshot() const;
    run::DaqPulseStatus issueLiveHit(bool outputEnabled, QString *error = nullptr);
    bool sendTestSine(QString *error = nullptr);
    bool startContinuous(QString *error = nullptr);
    bool stopContinuous(QString *error = nullptr);
    bool continuousActive() const;
    void shutdown();

private:
    DaqState updateStateLocked(DaqStatus status, const QString &fault = {});

    OperationCoordinator &operations_;
    ApplicationStateStore &stateStore_;
    std::mutex operationOrderMutex_;
    mutable std::mutex stateMutex_;
    std::unique_ptr<IDaqOutput> output_;
    MomentaryLease continuousLease_;
    DaqState state_;
};

} // namespace desktop_app::v2
