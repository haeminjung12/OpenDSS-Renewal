#include "../v2/operation/operation_coordinator.h"

#include <QCoreApplication>

#include <array>
#include <iostream>
#include <utility>

namespace {

using namespace desktop_app::v2;

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

ResourceLocks locks(ResourceLock first, ResourceLock second)
{
    ResourceLocks result(first);
    result |= second;
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    OperationCoordinator coordinator;

    const std::array allKinds{
        OperationKind::ImageSequence,
        OperationKind::DatasetCapture,
        OperationKind::Training,
        OperationKind::ModelTest,
        OperationKind::SequenceTest,
        OperationKind::LiveSorting,
    };
    for (const OperationKind kind : allKinds) {
        auto acquired = coordinator.acquire(kind, ResourceLock::Storage);
        if (!acquired.acquired() || acquired.fault
            || coordinator.snapshot().kind != kind
            || coordinator.snapshot().lifecycle != OperationLifecycle::Starting
            || !acquired.lease.transition(OperationLifecycle::Running)
            || !acquired.lease.transition(OperationLifecycle::Completed)
            || coordinator.snapshot().lifecycle != OperationLifecycle::Completed) {
            return fail(1, "An operation kind did not follow acquire/run/complete lifecycle.");
        }
        acquired.lease.release();
        if (coordinator.snapshot().kind
            || coordinator.snapshot().lifecycle != OperationLifecycle::Idle) {
            return fail(2, "Explicit release did not return the coordinator to Idle.");
        }
    }

    const ResourceLocks trainingLocks =
        locks(ResourceLock::Training, ResourceLock::Storage);
    auto training = coordinator.acquire(OperationKind::Training, trainingLocks);
    auto disjointLong =
        coordinator.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    if (!training.acquired() || disjointLong.acquired() || !disjointLong.fault
        || disjointLong.fault->currentKind != OperationKind::Training
        || disjointLong.fault->currentLifecycle != OperationLifecycle::Starting
        || disjointLong.fault->currentLocks != trainingLocks
        || disjointLong.fault->reason.isEmpty() || disjointLong.fault->recovery.isEmpty()) {
        return fail(3, "The global long-operation slot did not reject disjoint locks contextually.");
    }

    auto cameraMomentary = coordinator.acquireMomentary(ResourceLock::Camera);
    auto storageMomentary = coordinator.acquireMomentary(ResourceLock::Storage);
    if (!cameraMomentary.acquired() || storageMomentary.acquired() || !storageMomentary.fault
        || storageMomentary.fault->currentKind != OperationKind::Training) {
        return fail(4, "Momentary resource overlap rules were not enforced against a long operation.");
    }
    cameraMomentary.lease.release();
    training.lease.release();

    auto firstMomentary = coordinator.acquireMomentary(ResourceLock::Camera);
    auto secondMomentary = coordinator.acquireMomentary(ResourceLock::Daq);
    auto overlappingMomentary = coordinator.acquireMomentary(ResourceLock::Camera);
    if (!firstMomentary.acquired() || !secondMomentary.acquired()
        || overlappingMomentary.acquired() || !overlappingMomentary.fault
        || overlappingMomentary.fault->currentKind
        || overlappingMomentary.fault->currentLocks != ResourceLock::Camera) {
        return fail(5, "Momentary leases did not allow disjoint and reject overlapping resources.");
    }
    firstMomentary.lease.release();
    secondMomentary.lease.release();

    {
        auto original =
            coordinator.acquire(OperationKind::ModelTest, ResourceLock::Model);
        OperationLease moved = std::move(original.lease);
        if (original.lease.isValid() || !moved.isValid()
            || !moved.transition(OperationLifecycle::Running)) {
            return fail(6, "Moving an operation lease did not transfer ownership.");
        }
    }
    if (coordinator.snapshot().kind)
        return fail(7, "Operation lease destruction did not release the long-operation slot.");

    auto stale = coordinator.acquire(OperationKind::SequenceTest, ResourceLock::Sequence);
    OperationLease staleHolder = std::move(stale.lease);
    staleHolder.release();
    auto replacement = coordinator.acquire(OperationKind::LiveSorting, ResourceLock::Daq);
    OperationLease releasedMove = std::move(staleHolder);
    stale.lease.release();
    staleHolder.release();
    releasedMove.release();
    if (!replacement.acquired()
        || coordinator.snapshot().kind != OperationKind::LiveSorting) {
        return fail(8, "A stale lease release affected a newer operation generation.");
    }
    replacement.lease.release();

    OperationLease assignedOperation;
    auto assignmentSource =
        coordinator.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    assignedOperation = std::move(assignmentSource.lease);
    if (assignmentSource.lease.isValid() || !assignedOperation.isValid())
        return fail(9, "Operation move assignment did not transfer the lease.");
    assignedOperation.release();
    assignedOperation.release();
    if (coordinator.snapshot().kind)
        return fail(10, "Double operation release did not remain harmless.");

    auto firstAssignedMomentary = coordinator.acquireMomentary(ResourceLock::Camera);
    auto secondAssignedMomentary = coordinator.acquireMomentary(ResourceLock::Daq);
    MomentaryLease assignedMomentary = std::move(firstAssignedMomentary.lease);
    assignedMomentary = std::move(secondAssignedMomentary.lease);
    auto cameraAfterAssignment = coordinator.acquireMomentary(ResourceLock::Camera);
    auto daqAfterAssignment = coordinator.acquireMomentary(ResourceLock::Daq);
    if (firstAssignedMomentary.lease.isValid() || secondAssignedMomentary.lease.isValid()
        || !assignedMomentary.isValid() || !cameraAfterAssignment.acquired()
        || daqAfterAssignment.acquired()) {
        return fail(11, "Momentary move construction or assignment did not transfer exact locks.");
    }
    assignedMomentary.release();
    assignedMomentary.release();
    cameraAfterAssignment.lease.release();

    auto staleMomentarySource = coordinator.acquireMomentary(ResourceLock::Storage);
    MomentaryLease staleMomentary = std::move(staleMomentarySource.lease);
    staleMomentary.release();
    auto replacementMomentary = coordinator.acquireMomentary(ResourceLock::Storage);
    MomentaryLease releasedMomentaryMove = std::move(staleMomentary);
    staleMomentarySource.lease.release();
    staleMomentary.release();
    releasedMomentaryMove.release();
    auto replacementOverlap = coordinator.acquireMomentary(ResourceLock::Storage);
    if (!replacementMomentary.acquired() || replacementOverlap.acquired())
        return fail(12, "A stale momentary holder affected a newer resource generation.");
    replacementMomentary.lease.release();

    auto lifecycle =
        coordinator.acquire(OperationKind::DatasetCapture, ResourceLock::Dataset);
    OperationFault invalidTransition;
    if (!lifecycle.acquired()
        || lifecycle.lease.transition(OperationLifecycle::Paused, &invalidTransition)
        || invalidTransition.currentKind != OperationKind::DatasetCapture
        || invalidTransition.currentLifecycle != OperationLifecycle::Starting
        || invalidTransition.reason.isEmpty() || invalidTransition.recovery.isEmpty()
        || !lifecycle.lease.transition(OperationLifecycle::Running)
        || !lifecycle.lease.transition(OperationLifecycle::Paused)
        || !lifecycle.lease.transition(OperationLifecycle::Stopping)
        || !lifecycle.lease.transition(OperationLifecycle::Interrupted)) {
        return fail(13, "Lifecycle validation or contextual transition fault was incorrect.");
    }
    lifecycle.lease.release();

    OperationLease expiredOperation;
    MomentaryLease expiredMomentary;
    {
        OperationCoordinator scopedCoordinator;
        auto operation =
            scopedCoordinator.acquire(OperationKind::ModelTest, ResourceLock::Model);
        auto momentary =
            scopedCoordinator.acquireMomentary(ResourceLock::Camera);
        expiredOperation = std::move(operation.lease);
        expiredMomentary = std::move(momentary.lease);
    }
    OperationFault expiredFault;
    if (expiredOperation.isValid()
        || expiredOperation.transition(OperationLifecycle::Running, &expiredFault)
        || expiredFault.reason.isEmpty() || expiredFault.recovery.isEmpty()
        || expiredMomentary.isValid()) {
        return fail(14, "Leases surviving coordinator destruction did not expire contextually.");
    }
    expiredOperation.release();
    expiredOperation.release();
    expiredMomentary.release();
    expiredMomentary.release();

    return 0;
}
