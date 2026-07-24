#include "../v2/operation/operation_coordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <array>
#include <iostream>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

using namespace desktop_app::v2;

int fail(int code, const char *message)
{
    std::cerr << message << '\n';
    return code;
}

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("{}") == 2;
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
        coordinator.acquire(OperationKind::DatasetCapture, ResourceLock::Storage);
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

    QTemporaryDir datasets;
    const QString folderA = QDir(datasets.path()).filePath(QStringLiteral("a"));
    const QString folderB = QDir(datasets.path()).filePath(QStringLiteral("b"));
    if (!QDir().mkpath(folderA) || !QDir().mkpath(folderB))
        return fail(15, "Could not create Dataset lock fixtures.");
    const QString datasetA = QDir(folderA).filePath(QStringLiteral("dataset.json"));
    const QString datasetB = QDir(folderB).filePath(QStringLiteral("dataset.json"));
    if (!writeFile(datasetA) || !writeFile(datasetB))
        return fail(16, "Could not write Dataset lock fixtures.");

    auto readA1 = coordinator.acquireDataset(datasetA, DatasetAccess::Read);
    auto readA2 = coordinator.acquireDataset(datasetA, DatasetAccess::Read);
    auto writeAConflict = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    auto writeB = coordinator.acquireDataset(datasetB, DatasetAccess::Write);
    if (!readA1.acquired() || !readA2.acquired() || writeAConflict.acquired()
        || !writeAConflict.fault || !writeB.acquired()) {
        return fail(17, "Dataset read/write identity matrix was not enforced.");
    }
    writeB.lease.release();
    readA1.lease.release();
    readA2.lease.release();

    const QString aliasA =
        QDir(folderA).filePath(QStringLiteral("nested/../dataset.json"));
    if (!QDir().mkpath(QDir(folderA).filePath(QStringLiteral("nested"))))
        return fail(18, "Could not create Dataset alias fixture.");
    auto aliasRead = coordinator.acquireDataset(aliasA, DatasetAccess::Read);
    auto aliasWrite = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    if (!aliasRead.acquired() || aliasWrite.acquired())
        return fail(18, "A lexical Dataset alias did not resolve to the same identity.");
    aliasRead.lease.release();

#ifdef Q_OS_WIN
    auto caseRead =
        coordinator.acquireDataset(datasetA.toUpper(), DatasetAccess::Read);
    auto caseWrite = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    if (!caseRead.acquired() || caseWrite.acquired())
        return fail(19, "Windows Dataset identity was not case-folded.");
    caseRead.lease.release();
#endif

    const QString linkA = QDir(datasets.path()).filePath(QStringLiteral("dataset-link.json"));
    bool linkCreated = false;
#ifdef Q_OS_WIN
    const QString nativeLink = QDir::toNativeSeparators(linkA);
    const QString nativeTarget = QDir::toNativeSeparators(datasetA);
    linkCreated = CreateSymbolicLinkW(
                      reinterpret_cast<LPCWSTR>(nativeLink.utf16()),
                      reinterpret_cast<LPCWSTR>(nativeTarget.utf16()), 0x2)
        != FALSE;
#else
    linkCreated = QFile::link(datasetA, linkA);
#endif
    if (linkCreated) {
        auto originalRead = coordinator.acquireDataset(datasetA, DatasetAccess::Read);
        auto linkedWrite = coordinator.acquireDataset(linkA, DatasetAccess::Write);
        if (!originalRead.acquired() || linkedWrite.acquired())
            return fail(20, "A symbolic Dataset alias did not resolve to the same identity.");
        originalRead.lease.release();
    }

    auto trainingRead = coordinator.acquireWithDataset(
        OperationKind::Training, ResourceLock::Training | ResourceLock::Storage,
        datasetA, DatasetAccess::Read);
    auto labelWriteA = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    auto labelWriteB = coordinator.acquireDataset(datasetB, DatasetAccess::Write);
    if (!trainingRead.acquired() || labelWriteA.acquired() || !labelWriteA.fault
        || labelWriteA.fault->currentKind != OperationKind::Training
        || !labelWriteB.acquired()) {
        return fail(21, "A combined operation did not retain only its keyed Dataset hold.");
    }
    labelWriteB.lease.release();
    trainingRead.lease.release();
    auto labelWriteAfter = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    if (!labelWriteAfter.acquired())
        return fail(22, "Combined operation release did not release its Dataset hold.");
    labelWriteAfter.lease.release();

    auto active = coordinator.acquire(OperationKind::ModelTest, ResourceLock::Model);
    const bool activeAcquired = active.acquired();
    auto failedCombined = coordinator.acquireWithDataset(
        OperationKind::Training, ResourceLock::Training, datasetA, DatasetAccess::Read);
    active.lease.release();
    auto writeAfterFailedCombined =
        coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    if (!activeAcquired || failedCombined.acquired() || !writeAfterFailedCombined.acquired())
        return fail(23, "Failed combined acquisition retained partial ownership.");
    writeAfterFailedCombined.lease.release();

    auto movedSource = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    DatasetLease movedDataset = std::move(movedSource.lease);
    if (movedSource.lease.isValid() || !movedDataset.isValid())
        return fail(24, "Moving a Dataset lease did not transfer ownership.");
    movedDataset.release();
    auto replacementDataset = coordinator.acquireDataset(datasetA, DatasetAccess::Write);
    DatasetLease staleDataset = std::move(movedDataset);
    movedDataset.release();
    staleDataset.release();
    auto replacementConflict = coordinator.acquireDataset(datasetA, DatasetAccess::Read);
    if (!replacementDataset.acquired() || replacementConflict.acquired())
        return fail(25, "A stale Dataset lease affected a newer reservation.");
    replacementDataset.lease.release();

    DatasetLease expiredDataset;
    {
        OperationCoordinator scopedCoordinator;
        auto scoped =
            scopedCoordinator.acquireDataset(datasetA, DatasetAccess::Read);
        expiredDataset = std::move(scoped.lease);
    }
    if (expiredDataset.isValid())
        return fail(26, "Dataset lease did not expire with its coordinator.");
    expiredDataset.release();

    const QString captureFolder =
        QDir(datasets.path()).filePath(QStringLiteral("capture"));
    if (!QDir().mkpath(captureFolder))
        return fail(27, "Could not create capture target parent.");
    const QString futureDataset =
        QDir(captureFolder).filePath(QStringLiteral("dataset.json"));
    auto missingRead = coordinator.acquireDataset(futureDataset, DatasetAccess::Read);
    auto wrongMissing = coordinator.acquireWithDataset(
        OperationKind::Training, ResourceLock::Training, futureDataset,
        DatasetAccess::Read);
    auto captureWrite = coordinator.acquireWithDataset(
        OperationKind::DatasetCapture, ResourceLock::Camera | ResourceLock::Storage,
        futureDataset, DatasetAccess::Write);
    auto captureConflict = coordinator.acquireDataset(futureDataset, DatasetAccess::Read);
    if (missingRead.acquired() || wrongMissing.acquired() || !captureWrite.acquired()
        || captureConflict.acquired()) {
        return fail(28, "Missing Dataset targets were not restricted to Dataset Capture writes.");
    }
    captureWrite.lease.release();

    return 0;
}
