#include "operation_coordinator.h"

#include <QHash>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRecursiveMutex>

#include <functional>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace desktop_app::v2 {

class OperationControl final
{
public:
    struct ModelReservation {
        QString key;
        ModelAccess access = ModelAccess::Read;
    };

    struct DatasetReservation {
        QString key;
        DatasetAccess access = DatasetAccess::Read;
    };

    struct ActiveOperation {
        OperationKind kind;
        OperationLifecycle lifecycle = OperationLifecycle::Starting;
        ResourceLocks locks;
        quint64 generation = 0;
        bool hasDataset = false;
        bool hasModel = false;
    };

    QMutex mutex;
    std::optional<ActiveOperation> active;
    QHash<quint64, ResourceLocks> momentary;
    QHash<quint64, DatasetReservation> datasets;
    QHash<quint64, ModelReservation> models;
    quint64 nextGeneration = 1;
    QRecursiveMutex resourcesChangedMutex;
    std::function<void()> resourcesChanged;
};

namespace {

bool overlaps(ResourceLocks left, ResourceLocks right)
{
    return (left & right) != ResourceLocks{};
}

bool isLinkOrReparse(const QFileInfo &entry)
{
    if (entry.isSymLink())
        return true;
#ifdef Q_OS_WIN
    const QString nativePath = QDir::toNativeSeparators(entry.absoluteFilePath());
    const DWORD attributes =
        GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return false;
#endif
}

bool pathContainsLinkOrReparse(QString path)
{
    path = QFileInfo(path).absoluteFilePath();
    for (;;) {
        const QFileInfo entry(path);
        if (isLinkOrReparse(entry))
            return true;
        const QString parent = entry.absolutePath();
        if (parent == path)
            return false;
        path = parent;
    }
}

QString normalizedPathKey(QString path)
{
    path = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    return path.toCaseFolded();
#else
    return path;
#endif
}

std::optional<QString> datasetKey(const QString &path, bool allowMissingCaptureTarget,
                                  QString *error)
{
    const QFileInfo requested(path);
    if (requested.exists()) {
        const QString canonical = requested.canonicalFilePath();
        const QFileInfo resolved(canonical);
        if (canonical.isEmpty() || !resolved.isFile()) {
            if (error)
                *error = QStringLiteral("Dataset path must identify an existing regular file.");
            return std::nullopt;
        }
        return normalizedPathKey(canonical);
    }

    if (!allowMissingCaptureTarget) {
        if (error)
            *error = QStringLiteral("Dataset path must identify an existing regular file.");
        return std::nullopt;
    }
    if (isLinkOrReparse(requested)) {
        if (error)
            *error = QStringLiteral("Dataset Capture target must not be a link or reparse point.");
        return std::nullopt;
    }
    const QFileInfo parent(requested.absolutePath());
    if (!parent.exists() || !parent.isDir()
        || pathContainsLinkOrReparse(parent.absoluteFilePath())) {
        if (error)
            *error = QStringLiteral("Dataset Capture target parent must be an existing real directory.");
        return std::nullopt;
    }
    const QString canonicalParent = parent.canonicalFilePath();
    if (canonicalParent.isEmpty()) {
        if (error)
            *error = QStringLiteral("Could not resolve the Dataset Capture target parent.");
        return std::nullopt;
    }
    return normalizedPathKey(QDir(canonicalParent).filePath(requested.fileName()));
}

bool datasetConflicts(const OperationControl::DatasetReservation &held,
                      const QString &key, DatasetAccess requested)
{
    return held.key == key
        && (held.access == DatasetAccess::Write || requested == DatasetAccess::Write);
}

std::optional<QString> modelKey(const QString &path, QString *error)
{
    const QFileInfo requested(path);
    const QString canonical = requested.canonicalFilePath();
    const QFileInfo resolved(canonical);
    if (path.trimmed().isEmpty() || canonical.isEmpty() || !resolved.isDir()) {
        if (error)
            *error = QStringLiteral("Model package path must identify an existing directory.");
        return std::nullopt;
    }
    return normalizedPathKey(canonical);
}

bool modelConflicts(const OperationControl::ModelReservation &held,
                    const QString &key, ModelAccess requested)
{
    return held.key == key
        && (held.access == ModelAccess::Write || requested == ModelAccess::Write);
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

std::optional<OperationFault> momentaryConflict(const OperationControl &control,
                                                ResourceLocks locks)
{
    if (control.active && overlaps(locks, control.active->locks))
        return conflictFault(control, locks & control.active->locks);
    const ResourceLocks heldMomentaryLocks = momentaryLocks(control);
    if (overlaps(locks, heldMomentaryLocks))
        return conflictFault(control, locks & heldMomentaryLocks);
    return std::nullopt;
}

OperationFault datasetConflictFault(const OperationControl &control, quint64 generation)
{
    if (control.active && control.active->generation == generation)
        return conflictFault(control, control.active->locks);
    OperationFault fault;
    fault.reason = QStringLiteral("This Dataset is in use.");
    fault.recovery = QStringLiteral("Wait for the current Dataset action to finish.");
    return fault;
}

OperationFault modelConflictFault(const OperationControl &control, quint64 generation)
{
    if (control.active && control.active->generation == generation)
        return conflictFault(control, control.active->locks);
    OperationFault fault;
    fault.reason = QStringLiteral("This Model package is in use.");
    fault.recovery = QStringLiteral("Wait for the current Model package action to finish.");
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

ModelLease::ModelLease(std::weak_ptr<OperationControl> control, quint64 generation)
    : control_(std::move(control))
    , generation_(generation)
{
}

ModelLease::~ModelLease()
{
    release();
}

ModelLease::ModelLease(ModelLease &&other) noexcept
    : control_(std::move(other.control_))
    , generation_(std::exchange(other.generation_, 0))
{
    other.control_.reset();
}

ModelLease &ModelLease::operator=(ModelLease &&other) noexcept
{
    if (this != &other) {
        release();
        control_ = std::move(other.control_);
        generation_ = std::exchange(other.generation_, 0);
        other.control_.reset();
    }
    return *this;
}

bool ModelLease::isValid() const
{
    return generation_ != 0 && !control_.expired();
}

void ModelLease::release()
{
    if (generation_ == 0)
        return;
    const quint64 generation = std::exchange(generation_, 0);
    const auto control = control_.lock();
    control_.reset();
    if (!control)
        return;
    QMutexLocker locker(&control->mutex);
    control->models.remove(generation);
}

DatasetLease::DatasetLease(std::weak_ptr<OperationControl> control, quint64 generation)
    : control_(std::move(control))
    , generation_(generation)
{
}

DatasetLease::~DatasetLease()
{
    release();
}

DatasetLease::DatasetLease(DatasetLease &&other) noexcept
    : control_(std::move(other.control_))
    , generation_(std::exchange(other.generation_, 0))
{
    other.control_.reset();
}

DatasetLease &DatasetLease::operator=(DatasetLease &&other) noexcept
{
    if (this != &other) {
        release();
        control_ = std::move(other.control_);
        generation_ = std::exchange(other.generation_, 0);
        other.control_.reset();
    }
    return *this;
}

bool DatasetLease::isValid() const
{
    return generation_ != 0 && !control_.expired();
}

void DatasetLease::release()
{
    if (generation_ == 0)
        return;
    const quint64 generation = std::exchange(generation_, 0);
    const auto control = control_.lock();
    control_.reset();
    if (!control)
        return;
    QMutexLocker locker(&control->mutex);
    control->datasets.remove(generation);
}

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
    bool released = false;
    {
        QMutexLocker locker(&control->mutex);
        if (control->active && control->active->generation == generation) {
            if (control->active->hasDataset)
                control->datasets.remove(generation);
            if (control->active->hasModel)
                control->models.remove(generation);
            control->active.reset();
            released = true;
        }
    }
    if (released) {
        QMutexLocker callbackLocker(&control->resourcesChangedMutex);
        if (control->resourcesChanged)
            control->resourcesChanged();
    }
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
    bool released = false;
    {
        QMutexLocker locker(&control->mutex);
        released = control->momentary.remove(generation);
    }
    if (released) {
        QMutexLocker callbackLocker(&control->resourcesChangedMutex);
        if (control->resourcesChanged)
            control->resourcesChanged();
    }
}

bool OperationAcquireResult::acquired() const
{
    return lease.isValid();
}

bool MomentaryAcquireResult::acquired() const
{
    return lease.isValid();
}

bool DatasetAcquireResult::acquired() const
{
    return lease.isValid();
}

bool ModelAcquireResult::acquired() const
{
    return lease.isValid();
}

OperationCoordinator::OperationCoordinator(QObject *parent)
    : QObject(parent)
    , control_(std::make_shared<OperationControl>())
{
    control_->resourcesChanged = [this]() { emit resourcesChanged(); };
}

OperationCoordinator::~OperationCoordinator()
{
    QMutexLocker callbackLocker(&control_->resourcesChangedMutex);
    control_->resourcesChanged = {};
}

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
        OperationControl::ActiveOperation{kind, OperationLifecycle::Starting, locks, generation,
                                          false, false};
    OperationAcquireResult result{OperationLease(control_, generation), std::nullopt};
    locker.unlock();
    emit resourcesChanged();
    return result;
}

OperationAcquireResult OperationCoordinator::acquireWithDataset(
    OperationKind kind, ResourceLocks locks, const QString &datasetJsonPath,
    DatasetAccess access)
{
    QString pathError;
    const bool allowMissing =
        kind == OperationKind::DatasetCapture && access == DatasetAccess::Write;
    const auto key = datasetKey(datasetJsonPath, allowMissing, &pathError);
    if (!key) {
        OperationFault fault;
        fault.reason = pathError;
        fault.recovery = QStringLiteral("Choose a valid Dataset manifest path.");
        return {{}, fault};
    }

    QMutexLocker locker(&control_->mutex);
    if (control_->active)
        return {{}, conflictFault(*control_, control_->active->locks)};
    const ResourceLocks heldMomentaryLocks = momentaryLocks(*control_);
    if (overlaps(locks, heldMomentaryLocks))
        return {{}, conflictFault(*control_, locks & heldMomentaryLocks)};
    for (auto it = control_->datasets.cbegin(); it != control_->datasets.cend(); ++it) {
        if (datasetConflicts(it.value(), *key, access))
            return {{}, datasetConflictFault(*control_, it.key())};
    }

    const quint64 generation = control_->nextGeneration++;
    control_->active =
        OperationControl::ActiveOperation{kind, OperationLifecycle::Starting, locks, generation,
                                          true, false};
    control_->datasets.insert(generation, {*key, access});
    OperationAcquireResult result{OperationLease(control_, generation), std::nullopt};
    locker.unlock();
    emit resourcesChanged();
    return result;
}

OperationAcquireResult OperationCoordinator::acquireWithModel(
    OperationKind kind, ResourceLocks locks, const QString &modelPackagePath,
    ModelAccess access)
{
    QString pathError;
    const auto key = modelKey(modelPackagePath, &pathError);
    if (!key) {
        OperationFault fault;
        fault.reason = pathError;
        fault.recovery = QStringLiteral("Choose an existing Model package.");
        return {{}, fault};
    }

    QMutexLocker locker(&control_->mutex);
    if (control_->active)
        return {{}, conflictFault(*control_, control_->active->locks)};
    const ResourceLocks heldMomentaryLocks = momentaryLocks(*control_);
    if (overlaps(locks, heldMomentaryLocks))
        return {{}, conflictFault(*control_, locks & heldMomentaryLocks)};
    for (auto it = control_->models.cbegin(); it != control_->models.cend(); ++it) {
        if (modelConflicts(it.value(), *key, access))
            return {{}, modelConflictFault(*control_, it.key())};
    }

    const quint64 generation = control_->nextGeneration++;
    control_->active =
        OperationControl::ActiveOperation{kind, OperationLifecycle::Starting, locks, generation,
                                          false, true};
    control_->models.insert(generation, {*key, access});
    OperationAcquireResult result{OperationLease(control_, generation), std::nullopt};
    locker.unlock();
    emit resourcesChanged();
    return result;
}

MomentaryAcquireResult OperationCoordinator::acquireMomentary(ResourceLocks locks)
{
    QMutexLocker locker(&control_->mutex);
    if (const auto fault = momentaryConflict(*control_, locks))
        return {{}, fault};

    const quint64 generation = control_->nextGeneration++;
    control_->momentary.insert(generation, locks);
    MomentaryAcquireResult result{MomentaryLease(control_, generation), std::nullopt};
    locker.unlock();
    emit resourcesChanged();
    return result;
}

DatasetAcquireResult OperationCoordinator::acquireDataset(const QString &datasetJsonPath,
                                                          DatasetAccess access)
{
    QString pathError;
    const auto key = datasetKey(datasetJsonPath, false, &pathError);
    if (!key) {
        OperationFault fault;
        fault.reason = pathError;
        fault.recovery = QStringLiteral("Choose an existing Dataset manifest.");
        return {{}, fault};
    }

    QMutexLocker locker(&control_->mutex);
    for (auto it = control_->datasets.cbegin(); it != control_->datasets.cend(); ++it) {
        if (datasetConflicts(it.value(), *key, access))
            return {{}, datasetConflictFault(*control_, it.key())};
    }
    const quint64 generation = control_->nextGeneration++;
    control_->datasets.insert(generation, {*key, access});
    return {DatasetLease(control_, generation), std::nullopt};
}

ModelAcquireResult OperationCoordinator::acquireModel(const QString &modelPackagePath,
                                                      ModelAccess access)
{
    QString pathError;
    const auto key = modelKey(modelPackagePath, &pathError);
    if (!key) {
        OperationFault fault;
        fault.reason = pathError;
        fault.recovery = QStringLiteral("Choose an existing Model package.");
        return {{}, fault};
    }

    QMutexLocker locker(&control_->mutex);
    for (auto it = control_->models.cbegin(); it != control_->models.cend(); ++it) {
        if (modelConflicts(it.value(), *key, access))
            return {{}, modelConflictFault(*control_, it.key())};
    }
    const quint64 generation = control_->nextGeneration++;
    control_->models.insert(generation, {*key, access});
    return {ModelLease(control_, generation), std::nullopt};
}

bool OperationCoordinator::momentaryAvailable(ResourceLocks locks) const
{
    QMutexLocker locker(&control_->mutex);
    return !momentaryConflict(*control_, locks).has_value();
}

OperationSnapshot OperationCoordinator::snapshot() const
{
    QMutexLocker locker(&control_->mutex);
    if (!control_->active)
        return {};
    return {control_->active->kind, control_->active->lifecycle, control_->active->locks};
}

} // namespace desktop_app::v2
