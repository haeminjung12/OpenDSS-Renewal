#pragma once

#include <QFlags>
#include <QString>

#include <memory>
#include <optional>

namespace desktop_app::v2 {

enum class OperationKind {
    ImageSequence,
    DatasetCapture,
    Training,
    ModelTest,
    SequenceTest,
    LiveSorting,
};

enum class OperationLifecycle {
    Idle,
    Starting,
    Running,
    Paused,
    Stopping,
    Completed,
    Interrupted,
    Failed,
};

enum class ResourceLock : quint32 {
    Camera = 1U << 0,
    Daq = 1U << 1,
    Storage = 1U << 2,
    Model = 1U << 3,
    Sequence = 1U << 5,
    Training = 1U << 6,
    Run = 1U << 7,
};
Q_DECLARE_FLAGS(ResourceLocks, ResourceLock)

enum class DatasetAccess {
    Read,
    Write,
};

enum class ModelAccess {
    Read,
    Write,
};

struct OperationSnapshot {
    std::optional<OperationKind> kind;
    OperationLifecycle lifecycle = OperationLifecycle::Idle;
    ResourceLocks locks;
};

struct OperationFault {
    std::optional<OperationKind> currentKind;
    OperationLifecycle currentLifecycle = OperationLifecycle::Idle;
    ResourceLocks currentLocks;
    QString reason;
    QString recovery;
};

class OperationCoordinator;
class OperationControl;

class ModelLease final
{
public:
    ModelLease() = default;
    ~ModelLease();
    ModelLease(ModelLease &&other) noexcept;
    ModelLease &operator=(ModelLease &&other) noexcept;
    ModelLease(const ModelLease &) = delete;
    ModelLease &operator=(const ModelLease &) = delete;

    bool isValid() const;
    void release();

private:
    friend class OperationCoordinator;
    ModelLease(std::weak_ptr<OperationControl> control, quint64 generation);

    std::weak_ptr<OperationControl> control_;
    quint64 generation_ = 0;
};

class DatasetLease final
{
public:
    DatasetLease() = default;
    ~DatasetLease();
    DatasetLease(DatasetLease &&other) noexcept;
    DatasetLease &operator=(DatasetLease &&other) noexcept;
    DatasetLease(const DatasetLease &) = delete;
    DatasetLease &operator=(const DatasetLease &) = delete;

    bool isValid() const;
    void release();

private:
    friend class OperationCoordinator;
    DatasetLease(std::weak_ptr<OperationControl> control, quint64 generation);

    std::weak_ptr<OperationControl> control_;
    quint64 generation_ = 0;
};

class OperationLease final
{
public:
    OperationLease() = default;
    ~OperationLease();
    OperationLease(OperationLease &&other) noexcept;
    OperationLease &operator=(OperationLease &&other) noexcept;
    OperationLease(const OperationLease &) = delete;
    OperationLease &operator=(const OperationLease &) = delete;

    bool isValid() const;
    bool transition(OperationLifecycle next, OperationFault *fault = nullptr);
    void release();

private:
    friend class OperationCoordinator;
    OperationLease(std::weak_ptr<OperationControl> control, quint64 generation);

    std::weak_ptr<OperationControl> control_;
    quint64 generation_ = 0;
};

class MomentaryLease final
{
public:
    MomentaryLease() = default;
    ~MomentaryLease();
    MomentaryLease(MomentaryLease &&other) noexcept;
    MomentaryLease &operator=(MomentaryLease &&other) noexcept;
    MomentaryLease(const MomentaryLease &) = delete;
    MomentaryLease &operator=(const MomentaryLease &) = delete;

    bool isValid() const;
    void release();

private:
    friend class OperationCoordinator;
    MomentaryLease(std::weak_ptr<OperationControl> control, quint64 generation);

    std::weak_ptr<OperationControl> control_;
    quint64 generation_ = 0;
};

struct OperationAcquireResult {
    OperationLease lease;
    std::optional<OperationFault> fault;

    bool acquired() const;
};

struct MomentaryAcquireResult {
    MomentaryLease lease;
    std::optional<OperationFault> fault;

    bool acquired() const;
};

struct DatasetAcquireResult {
    DatasetLease lease;
    std::optional<OperationFault> fault;

    bool acquired() const;
};

struct ModelAcquireResult {
    ModelLease lease;
    std::optional<OperationFault> fault;

    bool acquired() const;
};

class OperationCoordinator final
{
public:
    OperationCoordinator();
    ~OperationCoordinator();
    OperationCoordinator(const OperationCoordinator &) = delete;
    OperationCoordinator &operator=(const OperationCoordinator &) = delete;

    OperationAcquireResult acquire(OperationKind kind, ResourceLocks locks);
    OperationAcquireResult acquireWithDataset(OperationKind kind, ResourceLocks locks,
                                              const QString &datasetJsonPath,
                                              DatasetAccess access);
    OperationAcquireResult acquireWithModel(OperationKind kind, ResourceLocks locks,
                                            const QString &modelPackagePath,
                                            ModelAccess access);
    MomentaryAcquireResult acquireMomentary(ResourceLocks locks);
    DatasetAcquireResult acquireDataset(const QString &datasetJsonPath, DatasetAccess access);
    ModelAcquireResult acquireModel(const QString &modelPackagePath, ModelAccess access);
    OperationSnapshot snapshot() const;

private:
    std::shared_ptr<OperationControl> control_;
};

} // namespace desktop_app::v2

Q_DECLARE_OPERATORS_FOR_FLAGS(desktop_app::v2::ResourceLocks)
