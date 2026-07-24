#include "operation_coordinator.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <utility>

namespace desktop_app::v2 {

class OperationControl final
{
public:
    struct ActiveOperation {
        OperationKind kind;
        OperationLifecycle lifecycle = OperationLifecycle::Starting;
        ResourceLocks locks;
        quint64 generation = 0;
    };

    QMutex mutex;
    std::optional<ActiveOperation> active;
    QHash<quint64, ResourceLocks> momentary;
    quint64 nextGeneration = 1;
};

namespace {

bool overlaps(ResourceLocks left, ResourceLocks right)
{
    return (left & right) != ResourceLocks{};
}

QString operationName(OperationKind kind)
{
    switch (kind) {
    case OperationKind::ImageSequence:
        return QStringLiteral("Image Sequence");
    case OperationKind::DatasetCapture:
        return QStringLiteral("Droplet Dataset Capture");
    case OperationKind::Training:
        return QStringLiteral("Training");
    case OperationKind::ModelTest:
        return QStringLiteral("Model Test");
    case OperationKind::SequenceTest:
        return QStringLiteral("Sequence Test");
    case OperationKind::LiveSorting:
        return QStringLiteral("Live Sorting");
    }
    return QStringLiteral("operation");
}

bool validTransition(OperationLifecycle current, OperationLifecycle next)
{
    switch (current) {
    case OperationLifecycle::Starting:
        return next == OperationLifecycle::Running || next == OperationLifecycle::Stopping
            || next == OperationLifecycle::Interrupted || next == OperationLifecycle::Failed;
    case OperationLifecycle::Running:
        return next == OperationLifecycle::Paused || next == OperationLifecycle::Stopping
            || next == OperationLifecycle::Completed || next == OperationLifecycle::Interrupted
            || next == OperationLifecycle::Failed;
    case OperationLifecycle::Paused:
        return next == OperationLifecycle::Running || next == OperationLifecycle::Stopping
            || next == OperationLifecycle::Interrupted || next == OperationLifecycle::Failed;
    case OperationLifecycle::Stopping:
        return next == OperationLifecycle::Completed || next == OperationLifecycle::Interrupted
            || next == OperationLifecycle::Failed;
    case OperationLifecycle::Idle:
    case OperationLifecycle::Completed:
    case OperationLifecycle::Interrupted:
    case OperationLifecycle::Failed:
        return false;
    }
    return false;
}

ResourceLocks momentaryLocks(const OperationControl &control)
{
    ResourceLocks locks;
    for (auto it = control.momentary.cbegin(); it != control.momentary.cend(); ++it)
        locks |= it.value();
    return locks;
}

OperationFault conflictFault(const OperationControl &control, ResourceLocks conflictingLocks)
{
    OperationFault fault;
    fault.currentLocks = conflictingLocks;
    if (control.active) {
        fault.currentKind = control.active->kind;
        fault.currentLifecycle = control.active->lifecycle;
        fault.currentLocks = control.active->locks;
        fault.reason = QStringLiteral("Another long-running operation is active: %1.")
                           .arg(operationName(control.active->kind));
        fault.recovery =
            QStringLiteral("Stop or wait for %1 to finish.").arg(operationName(control.active->kind));
    } else {
        fault.reason = QStringLiteral("A required resource is in use.");
        fault.recovery = QStringLiteral("Wait for the current resource action to finish.");
    }
    return fault;
}

OperationFault expiredCoordinatorFault()
{
    OperationFault fault;
    fault.reason = QStringLiteral("The operation coordinator is no longer available.");
    fault.recovery = QStringLiteral("Discard the expired operation and request a new operation.");
    return fault;
}

} // namespace

OperationLease::OperationLease(std::weak_ptr<OperationControl> control, quint64 generation)
    : control_(std::move(control))
    , generation_(generation)
{
}

OperationLease::~OperationLease()
{
    release();
}

OperationLease::OperationLease(OperationLease &&other) noexcept
    : control_(std::move(other.control_))
    , generation_(std::exchange(other.generation_, 0))
{
    other.control_.reset();
}

OperationLease &OperationLease::operator=(OperationLease &&other) noexcept
{
    if (this != &other) {
        release();
        control_ = std::move(other.control_);
        generation_ = std::exchange(other.generation_, 0);
        other.control_.reset();
    }
    return *this;
}

bool OperationLease::isValid() const
{
    return generation_ != 0 && !control_.expired();
}

bool OperationLease::transition(OperationLifecycle next, OperationFault *fault)
{
    const auto control = control_.lock();
    if (!control) {
        if (fault)
            *fault = expiredCoordinatorFault();
        return false;
    }

    QMutexLocker locker(&control->mutex);
    if (!control->active || control->active->generation != generation_
        || !validTransition(control->active->lifecycle, next)) {
        if (fault) {
            *fault = conflictFault(*control, control->active ? control->active->locks : ResourceLocks{});
            fault->reason = QStringLiteral("The requested operation state transition is not valid.");
            fault->recovery =
                QStringLiteral("Use the lifecycle actions available for the current operation state.");
        }
        return false;
    }
    control->active->lifecycle = next;
    return true;
}

void OperationLease::release()
{
    if (generation_ == 0)
        return;
    const quint64 generation = std::exchange(generation_, 0);
    const auto control = control_.lock();
    control_.reset();
    if (!control)
        return;
    QMutexLocker locker(&control->mutex);
    if (control->active && control->active->generation == generation)
        control->active.reset();
}

MomentaryLease::MomentaryLease(std::weak_ptr<OperationControl> control, quint64 generation)
    : control_(std::move(control))
    , generation_(generation)
{
}

MomentaryLease::~MomentaryLease()
{
    release();
}

MomentaryLease::MomentaryLease(MomentaryLease &&other) noexcept
    : control_(std::move(other.control_))
    , generation_(std::exchange(other.generation_, 0))
{
    other.control_.reset();
}

MomentaryLease &MomentaryLease::operator=(MomentaryLease &&other) noexcept
{
    if (this != &other) {
        release();
        control_ = std::move(other.control_);
        generation_ = std::exchange(other.generation_, 0);
        other.control_.reset();
    }
    return *this;
}

bool MomentaryLease::isValid() const
{
    return generation_ != 0 && !control_.expired();
}

void MomentaryLease::release()
{
    if (generation_ == 0)
        return;
    const quint64 generation = std::exchange(generation_, 0);
    const auto control = control_.lock();
    control_.reset();
    if (!control)
        return;
    QMutexLocker locker(&control->mutex);
    control->momentary.remove(generation);
}

bool OperationAcquireResult::acquired() const
{
    return lease.isValid();
}

bool MomentaryAcquireResult::acquired() const
{
    return lease.isValid();
}

OperationCoordinator::OperationCoordinator()
    : control_(std::make_shared<OperationControl>())
{
}

OperationCoordinator::~OperationCoordinator() = default;

OperationAcquireResult OperationCoordinator::acquire(OperationKind kind, ResourceLocks locks)
{
    QMutexLocker locker(&control_->mutex);
    if (control_->active) {
        return {{}, conflictFault(*control_, control_->active->locks)};
    }
    const ResourceLocks heldMomentaryLocks = momentaryLocks(*control_);
    if (overlaps(locks, heldMomentaryLocks))
        return {{}, conflictFault(*control_, locks & heldMomentaryLocks)};

    const quint64 generation = control_->nextGeneration++;
    control_->active =
        OperationControl::ActiveOperation{kind, OperationLifecycle::Starting, locks, generation};
    return {OperationLease(control_, generation), std::nullopt};
}

MomentaryAcquireResult OperationCoordinator::acquireMomentary(ResourceLocks locks)
{
    QMutexLocker locker(&control_->mutex);
    if (control_->active && overlaps(locks, control_->active->locks))
        return {{}, conflictFault(*control_, locks & control_->active->locks)};
    const ResourceLocks heldMomentaryLocks = momentaryLocks(*control_);
    if (overlaps(locks, heldMomentaryLocks))
        return {{}, conflictFault(*control_, locks & heldMomentaryLocks)};

    const quint64 generation = control_->nextGeneration++;
    control_->momentary.insert(generation, locks);
    return {MomentaryLease(control_, generation), std::nullopt};
}

OperationSnapshot OperationCoordinator::snapshot() const
{
    QMutexLocker locker(&control_->mutex);
    if (!control_->active)
        return {};
    return {control_->active->kind, control_->active->lifecycle, control_->active->locks};
}

} // namespace desktop_app::v2
