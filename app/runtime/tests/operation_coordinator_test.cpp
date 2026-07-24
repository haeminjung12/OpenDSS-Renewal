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

    const QString modelFolderA =
        QDir(datasets.path()).filePath(QStringLiteral("models/a/package"));
    const QString modelFolderB =
        QDir(datasets.path()).filePath(QStringLiteral("models/b/package"));
    if (!QDir().mkpath(modelFolderA) || !QDir().mkpath(modelFolderB))
        return fail(29, "Could not create Model package lock fixtures.");

    auto modelReadA1 = coordinator.acquireModel(modelFolderA, ModelAccess::Read);
    auto modelReadA2 = coordinator.acquireModel(modelFolderA, ModelAccess::Read);
    auto modelWriteAConflict = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto modelWriteB = coordinator.acquireModel(modelFolderB, ModelAccess::Write);
    if (!modelReadA1.acquired() || !modelReadA2.acquired()
        || modelWriteAConflict.acquired() || !modelWriteAConflict.fault
        || !modelWriteB.acquired()) {
        return fail(30, "Model package read/write identity matrix was not enforced.");
    }
    modelWriteB.lease.release();
    modelReadA1.lease.release();
    modelReadA2.lease.release();

    const QString modelAliasA =
        QDir(modelFolderA).filePath(QStringLiteral("nested/.."));
    if (!QDir().mkpath(QDir(modelFolderA).filePath(QStringLiteral("nested"))))
        return fail(31, "Could not create Model package alias fixture.");
    auto modelAliasRead = coordinator.acquireModel(modelAliasA, ModelAccess::Read);
    auto modelAliasWrite = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    if (!modelAliasRead.acquired() || modelAliasWrite.acquired())
        return fail(32, "A lexical Model package alias did not resolve to the same identity.");
    modelAliasRead.lease.release();

    auto modelWriteA1 = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto modelAliasWriteConflict =
        coordinator.acquireModel(modelAliasA, ModelAccess::Write);
    if (!modelWriteA1.acquired() || modelAliasWriteConflict.acquired()
        || !modelAliasWriteConflict.fault) {
        return fail(46, "Equivalent Model package identities allowed concurrent writers.");
    }
    modelWriteA1.lease.release();
    auto modelAliasWriteAfterRelease =
        coordinator.acquireModel(modelAliasA, ModelAccess::Write);
    if (!modelAliasWriteAfterRelease.acquired())
        return fail(47, "Model package write could not reacquire after release.");
    modelAliasWriteAfterRelease.lease.release();

#ifdef Q_OS_WIN
    auto modelCaseRead =
        coordinator.acquireModel(modelFolderA.toUpper(), ModelAccess::Read);
    auto modelCaseWrite = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    if (!modelCaseRead.acquired() || modelCaseWrite.acquired())
        return fail(33, "Windows Model package identity was not case-folded.");
    modelCaseRead.lease.release();
#endif

    auto modelTestRead = coordinator.acquireWithModel(
        OperationKind::ModelTest, ResourceLock::Model, modelFolderA, ModelAccess::Read);
    auto modelLibraryWriteA = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto modelLibraryWriteB = coordinator.acquireModel(modelFolderB, ModelAccess::Write);
    if (!modelTestRead.acquired() || modelLibraryWriteA.acquired()
        || !modelLibraryWriteA.fault
        || modelLibraryWriteA.fault->currentKind != OperationKind::ModelTest
        || !modelLibraryWriteB.acquired()) {
        return fail(34, "A combined operation did not retain only its keyed Model package hold.");
    }
    modelLibraryWriteB.lease.release();
    modelTestRead.lease.release();
    auto modelWriteAfter = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    if (!modelWriteAfter.acquired())
        return fail(35, "Combined operation release did not release its Model package hold.");
    modelWriteAfter.lease.release();

    auto heldModelWrite = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto failedModelCombined = coordinator.acquireWithModel(
        OperationKind::SequenceTest, ResourceLock::Model, modelFolderA, ModelAccess::Read);
    auto operationAfterFailedModelCombined =
        coordinator.acquire(OperationKind::ImageSequence, ResourceLock::Camera);
    if (!heldModelWrite.acquired() || failedModelCombined.acquired()
        || !failedModelCombined.fault || !operationAfterFailedModelCombined.acquired()) {
        return fail(36, "Failed combined Model acquisition retained partial operation ownership.");
    }
    operationAfterFailedModelCombined.lease.release();
    heldModelWrite.lease.release();

    auto activeModelOperation =
        coordinator.acquire(OperationKind::ModelTest, ResourceLock::Model);
    const bool activeModelOperationAcquired = activeModelOperation.acquired();
    auto blockedModelCombined = coordinator.acquireWithModel(
        OperationKind::SequenceTest, ResourceLock::Sequence, modelFolderA, ModelAccess::Read);
    activeModelOperation.lease.release();
    auto modelAfterBlockedCombined =
        coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    if (!activeModelOperationAcquired || blockedModelCombined.acquired()
        || !modelAfterBlockedCombined.acquired()) {
        return fail(37, "Operation conflict during combined Model acquisition retained a lease.");
    }
    modelAfterBlockedCombined.lease.release();

    auto movedModelSource = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    ModelLease movedModel = std::move(movedModelSource.lease);
    if (movedModelSource.lease.isValid() || !movedModel.isValid())
        return fail(38, "Moving a Model lease did not transfer ownership.");
    movedModel.release();
    auto replacementModel = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    ModelLease staleModel = std::move(movedModel);
    movedModel.release();
    staleModel.release();
    auto replacementModelConflict =
        coordinator.acquireModel(modelFolderA, ModelAccess::Read);
    if (!replacementModel.acquired() || replacementModelConflict.acquired())
        return fail(39, "A stale Model lease affected a newer reservation.");
    replacementModel.lease.release();

    auto assignedModelA = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto assignedModelB = coordinator.acquireModel(modelFolderB, ModelAccess::Write);
    ModelLease assignedModel = std::move(assignedModelA.lease);
    assignedModel = std::move(assignedModelB.lease);
    auto modelAAfterAssignment = coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    auto modelBWhileAssigned = coordinator.acquireModel(modelFolderB, ModelAccess::Read);
    if (assignedModelA.lease.isValid() || assignedModelB.lease.isValid()
        || !assignedModel.isValid() || !modelAAfterAssignment.acquired()
        || modelBWhileAssigned.acquired()) {
        return fail(40, "Model move assignment did not transfer the exact reservation.");
    }
    assignedModel.release();
    assignedModel.release();
    modelAAfterAssignment.lease.release();

    {
        auto scopedModel =
            coordinator.acquireModel(modelFolderA, ModelAccess::Write);
        if (!scopedModel.acquired())
            return fail(41, "Could not acquire the scoped Model lease.");
    }
    auto modelAfterDestruction =
        coordinator.acquireModel(modelFolderA, ModelAccess::Write);
    if (!modelAfterDestruction.acquired())
        return fail(42, "Model lease destruction did not release its reservation.");
    modelAfterDestruction.lease.release();

    ModelLease expiredModel;
    {
        OperationCoordinator scopedCoordinator;
        auto scoped =
            scopedCoordinator.acquireModel(modelFolderA, ModelAccess::Read);
        expiredModel = std::move(scoped.lease);
    }
    if (expiredModel.isValid())
        return fail(43, "Model lease did not expire with its coordinator.");
    expiredModel.release();

    const QString regularFile =
        QDir(datasets.path()).filePath(QStringLiteral("not-a-model-package"));
    if (!writeFile(regularFile))
        return fail(44, "Could not create invalid Model package fixture.");
    auto emptyModel = coordinator.acquireModel(QString(), ModelAccess::Read);
    auto missingModel = coordinator.acquireModel(
        QDir(datasets.path()).filePath(QStringLiteral("missing-model")),
        ModelAccess::Read);
    auto fileModel = coordinator.acquireModel(regularFile, ModelAccess::Read);
    if (emptyModel.acquired() || missingModel.acquired() || fileModel.acquired()
        || !emptyModel.fault || !missingModel.fault || !fileModel.fault) {
        return fail(45, "Invalid Model package identities were accepted.");
    }

    return 0;
}
