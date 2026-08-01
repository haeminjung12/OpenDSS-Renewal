#include "dataset_capture_service.h"

#include "../../detection/droplet_frame_processor.h"
#include "../../desktop_app/json_persistence.h"
#include "../persistence/frame_persistence_service.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QUuid>

#include <cmath>
#include <stdexcept>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

namespace desktop_app::v2::dataset {
namespace {
void setError(QString* output, const QString& value) {
    if (output)
        *output = value;
}

QString cleanName(const QString& requested) {
    QString value = QFileInfo(QString(requested).replace('\\', '/')).fileName().trimmed();
    value.replace(QRegularExpression(R"([^\p{L}\p{N} _.-])"), "_");
    value.remove(QRegularExpression(R"(^[ ._]+|[ ._]+$)"));
    if (value.isEmpty())
        value = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    return value;
}

QString createUniqueFolder(const QString& root, const QString& name) {
    QDir rootDirectory(root);
    for (int suffix = 1;; ++suffix) {
        const QString leaf = suffix == 1 ? name : name + "-" + QString::number(suffix);
        const QString candidate = rootDirectory.absoluteFilePath(leaf);
        if (rootDirectory.mkdir(leaf))
            return candidate;
        if (!QFileInfo::exists(candidate))
            return {};
    }
}

bool publish(const QString& temporary, const QString& target, QString* error) {
#ifdef Q_OS_WIN
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(temporary.utf16()),
                    reinterpret_cast<LPCWSTR>(target.utf16()), MOVEFILE_WRITE_THROUGH))
        return true;
    setError(error, QString("Could not atomically publish %1 (Windows error %2).")
                        .arg(QFileInfo(target).fileName())
                        .arg(GetLastError()));
#else
    const QByteArray source = QFile::encodeName(temporary);
    const QByteArray destination = QFile::encodeName(target);
    if (::link(source.constData(), destination.constData()) == 0 &&
        ::unlink(source.constData()) == 0)
        return true;
    setError(error, QString("Could not atomically publish %1: %2.")
                        .arg(QFileInfo(target).fileName(), QString::fromLocal8Bit(strerror(errno))));
#endif
    return false;
}

bool writeImage(const QImage& image, const QString& target, const char* format,
                QString* error) {
    QString temporaryPath;
    {
        QTemporaryFile temporary(
            QDir(QFileInfo(target).absolutePath()).filePath(".write-XXXXXX"));
        if (!temporary.open()) {
            setError(error, "Could not create temporary image file.");
            return false;
        }
        QImageWriter writer(&temporary, format);
        if (!writer.write(image) || !temporary.flush()) {
            setError(error, writer.errorString());
            return false;
        }
        temporaryPath = temporary.fileName();
        temporary.close();
        temporary.setAutoRemove(false);
    }
    if (!publish(temporaryPath, target, error)) {
        QFile::remove(temporaryPath);
        return false;
    }
    return true;
}

QString hashFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

sequence::SequenceLossCategory loss(const std::vector<LiveFrameDispatcher::Range>& ranges,
                                    std::uint64_t count) {
    sequence::SequenceLossCategory value;
    value.count = static_cast<qint64>(count);
    for (const auto& range : ranges)
        value.ranges.push_back({static_cast<qint64>(range.first),
                                static_cast<qint64>(range.last)});
    return value;
}

QString rangesText(const sequence::SequenceLossCategory& value) {
    QStringList ranges;
    for (const auto& range : value.ranges)
        ranges.push_back(QString("%1-%2").arg(range.first).arg(range.last));
    return ranges.isEmpty() ? QStringLiteral("none") : ranges.join(',');
}

void logFinalIntegrity(const sequence::SequenceIntegrity& value) {
    qWarning().noquote()
        << "Dataset Capture final integrity:"
        << "source_frame_gaps count" << value.sourceFrameGaps.count
        << "ranges" << rangesText(value.sourceFrameGaps)
        << "queue_rejections count" << value.queueRejections.count
        << "ranges" << rangesText(value.queueRejections)
        << "consumer_failures count" << value.consumerFailures.count
        << "ranges" << rangesText(value.consumerFailures);
}
} // namespace

DatasetCaptureService::DatasetCaptureService(OperationCoordinator& operations,
                                             DropletFrameProcessor& processor,
                                             MonotonicNow monotonicNow)
    : operations_(operations),
      processor_(processor),
      monotonicNow_(std::move(monotonicNow)),
      dispatcher_([this](const QImage& image, const FrameMeta& meta, double fps,
                         std::uint64_t handoffId, LiveFrameDispatcher::Membership membership) {
          consumeFrame(image, meta, fps, handoffId, membership);
      }) {}

DatasetCaptureService::~DatasetCaptureService() noexcept {
    try {
        bool active = false;
        {
            std::lock_guard lock(mutex_);
            active = lifecycle_ == OperationLifecycle::Starting ||
                     lifecycle_ == OperationLifecycle::Running ||
                     lifecycle_ == OperationLifecycle::Paused ||
                     lifecycle_ == OperationLifecycle::Stopping;
        }
        if (active) {
            qWarning().noquote()
                << "Dataset Capture scope exit requires interrupted recovery.";
            failAndRelease("scope_exit", "Dataset Capture ended before finalization.", nullptr);
        }
    } catch (const std::exception& exception) {
        qWarning().noquote() << "Dataset Capture scope-exit recovery failed:"
                             << exception.what();
        lease_.release();
    } catch (...) {
        qWarning().noquote() << "Dataset Capture scope-exit recovery failed.";
        lease_.release();
    }
}

bool DatasetCaptureService::start(const DatasetCaptureRequest& request, QString* error) {
    setError(error, {});
    if (!monotonicNow_)
        return setError(error, "A monotonic clock is required."), false;
    if (request.durationSeconds &&
        (!std::isfinite(*request.durationSeconds) || *request.durationSeconds <= 0))
        return setError(error, "Duration must be empty or finite and positive."), false;
    const QFileInfo root(request.saveRoot);
    if (!root.isDir() || !root.isWritable())
        return setError(error, "The save root must be a writable directory."), false;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Idle)
            return setError(error, "This Dataset Capture has already started."), false;
    }

    const QString displayName = cleanName(request.name);
    const QString folder = createUniqueFolder(request.saveRoot, displayName);
    if (folder.isEmpty())
        return setError(error, "Could not create a unique Dataset folder."), false;
    const QString sequenceFolder = QDir(folder).filePath("sequence");
    const QString cropsFolder = QDir(folder).filePath("crops");
    if (!QDir().mkpath(sequenceFolder) || !QDir().mkpath(cropsFolder)) {
        QDir(folder).removeRecursively();
        return setError(error, "Could not create Dataset folders."), false;
    }
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString partial = QDir(folder).filePath("dataset.partial.json");
    const QString spoolPath = QDir(folder).filePath("sequence.frames.partial");
    QString persistenceError;
    if (!desktop_app::writeJsonObjectAtomically(
            partial, QJsonObject{{"schema_version", "opendss.dataset.partial.v1"},
                                 {"status", "in_progress"},
                                 {"persistence_bit_depth", 8},
                                 {"spool_file", QFileInfo(spoolPath).fileName()},
                                 {"created_at", now}},
            &persistenceError)) {
        QDir(folder).removeRecursively();
        return setError(error, persistenceError), false;
    }
    const QString datasetPath = QDir(folder).filePath("dataset.json");
    auto acquired = operations_.acquireWithDataset(
        OperationKind::DatasetCapture, ResourceLock::Camera | ResourceLock::Storage,
        datasetPath, DatasetAccess::Write);
    if (!acquired.acquired()) {
        QDir(folder).removeRecursively();
        return setError(error, acquired.fault ? acquired.fault->reason
                                             : "Dataset Capture resources are in use."), false;
    }
    auto spool = std::make_unique<persistence::FramePersistenceService>();
    if (!spool->start(spoolPath, &persistenceError)) {
        QDir(folder).removeRecursively();
        return setError(error, persistenceError), false;
    }
    processor_.reset();
    if (!acquired.lease.transition(OperationLifecycle::Running)) {
        QDir(folder).removeRecursively();
        return setError(error, "Dataset Capture could not enter Running state."), false;
    }

    dispatcher_.openDatasetBoundary();
    std::lock_guard lock(mutex_);
    lease_ = std::move(acquired.lease);
    lifecycle_ = OperationLifecycle::Running;
    request_ = request;
    datasetId_ = QFileInfo(folder).fileName();
    displayName_ = displayName;
    folder_ = QFileInfo(folder).absoluteFilePath();
    sequenceFolder_ = QFileInfo(sequenceFolder).absoluteFilePath();
    cropsFolder_ = QFileInfo(cropsFolder).absoluteFilePath();
    partialPath_ = partial;
    spoolPath_ = spoolPath;
    createdAt_ = now;
    startedAt_ = now;
    activeStartedNs_ = monotonicNow_();
    acceptingOffers_ = true;
    spool_ = std::move(spool);
    return true;
}

bool DatasetCaptureService::offerFrame(const QImage& image, const FrameMeta& meta,
                                       double nominalFps, QString* error) {
    setError(error, {});
    refreshFailure();
    if (image.isNull() || image.format() != QImage::Format_Grayscale8 ||
        meta.width != image.width() || meta.height != image.height() || meta.bits != 8 ||
        meta.delivered < 0 || meta.frameIndex < 0 ||
        !std::isfinite(nominalFps) || nominalFps <= 0)
        return failAndRelease("invalid_frame", "Dataset Capture requires valid gray8 frames.",
                              error);
    LiveFrameDispatcher::OfferResult offered;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running || !acceptingOffers_)
            return setError(error, "Dataset Capture is not accepting frames."), false;
        if (lastSourceDelivery_ && meta.delivered > *lastSourceDelivery_ + 1) {
            const qint64 first = *lastSourceDelivery_ + 1;
            const qint64 last = meta.delivered - 1;
            sourceGaps_.ranges.push_back({first, last});
            sourceGaps_.count += last - first + 1;
            qWarning().noquote() << "Dataset Capture source frame gap" << first << "-" << last;
        }
        if (lastSourceDelivery_ && meta.delivered <= *lastSourceDelivery_)
            return setError(error, "Source delivery IDs must increase."), false;
        lastSourceDelivery_ = meta.delivered;
        FrameMeta dispatchMeta = meta;
        dispatchMeta.delivered = dispatcherDelivery_ + 1;
        LiveFrameDispatcher::Membership membership;
        membership.datasetCapture = true;
        offered = dispatcher_.offer(image, dispatchMeta, nominalFps, membership);
        if (offered.accepted) {
            ++dispatcherDelivery_;
            lastAcceptedHandoff_ = offered.handoffId;
        }
    }
    if (!offered.accepted)
        qWarning().noquote() << "Dataset Capture queue rejected handoff"
                             << offered.handoffId << "-" << offered.handoffId;
    return true;
}

void DatasetCaptureService::consumeFrame(const QImage& image, const FrameMeta& meta,
                                         double fps, std::uint64_t handoffId,
                                         LiveFrameDispatcher::Membership) {
    qint64 frameIndex = 0;
    {
        std::lock_guard lock(mutex_);
        if (formatFixed_ && (width_ != image.width() || height_ != image.height() ||
                             bitDepth_ != meta.bits || fps_ != fps)) {
            error_ = "Dataset Capture frame format changed.";
            throw std::runtime_error("format changed");
        }
        frameIndex = savedFrameCount_ + 1;
    }
    QString spoolError;
    if (!spool_ || !spool_->append(image, meta, handoffId, &spoolError)) {
        std::lock_guard lock(mutex_);
        error_ = spoolError.isEmpty() ? "Dataset Capture spooling failed." : spoolError;
        qWarning().noquote() << "Dataset Capture consumer failure handoff"
                             << handoffId << "-" << handoffId << ":" << error_;
        throw std::runtime_error("frame spool failed");
    }
    {
        std::lock_guard lock(mutex_);
        if (!formatFixed_) {
            width_ = image.width();
            height_ = image.height();
            bitDepth_ = meta.bits;
            fps_ = fps;
            formatFixed_ = true;
        }
        ++savedFrameCount_;
    }

    cv::Mat frame(image.height(), image.width(), CV_8UC1,
                  const_cast<uchar*>(image.constBits()), image.bytesPerLine());
    const DropletFrameProcessingResult processed = processor_.process(frame);
    if (processed.detection.capacityExceeded) {
        std::lock_guard lock(mutex_);
        error_ = "Detector track capacity exceeded.";
        throw std::runtime_error("track capacity exceeded");
    }
    if (processed.cropFailed) {
        std::lock_guard lock(mutex_);
        error_ = processed.cropError;
        throw std::runtime_error("crop failed");
    }
    QVector<DatasetRecord> records;
    records.reserve(static_cast<qsizetype>(processed.enteredCropCount));
    for (std::size_t entryIndex = 0; entryIndex < processed.enteredCropCount; ++entryIndex) {
        const desktop_app::DatasetCrop& crop = processed.enteredCrops[entryIndex].crop;
        QString cropError;
        qint64 cropIndex = 0;
        {
            std::lock_guard lock(mutex_);
            cropIndex = records_.size() + records.size() + 1;
        }
        const QString eventId = QString::number(cropIndex);
        const QString cropPath = QDir(cropsFolder_)
                                     .filePath(QString("droplet_%1.png")
                                                   .arg(cropIndex, 6, 10,
                                                        QLatin1Char('0')));
        const QImage cropImage(crop.image.data, crop.image.cols, crop.image.rows,
                               crop.image.step, QImage::Format_Grayscale8);
        if (!writeImage(cropImage, cropPath, "PNG", &cropError)) {
            std::lock_guard lock(mutex_);
            error_ = cropError;
            throw std::runtime_error("crop write failed");
        }
        records.push_back(DatasetRecord{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            "crops/" + QFileInfo(cropPath).fileName(),
            hashFile(cropPath),
            QString::number(meta.frameIndex),
            eventId,
            QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
            QRect(crop.sourceRect.x, crop.sourceRect.y, crop.sourceRect.width,
                  crop.sourceRect.height),
            frameIndex});
    }
    std::lock_guard lock(mutex_);
    for (DatasetRecord& record : records)
        records_.push_back(std::move(record));
}

bool DatasetCaptureService::pause(QString* error) {
    setError(error, {});
    refreshFailure();
    std::uint64_t checkpoint = 0;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running)
            return setError(error, "Only a running Dataset Capture can pause."), false;
        acceptingOffers_ = false;
        activeElapsedNs_ += (std::max)(qint64(0), monotonicNow_() - *activeStartedNs_);
        activeStartedNs_.reset();
        checkpoint = lastAcceptedHandoff_;
        if (!lease_.transition(OperationLifecycle::Paused))
            return setError(error, "Dataset Capture could not pause."), false;
        lifecycle_ = OperationLifecycle::Paused;
    }
    dispatcher_.waitThrough(checkpoint);
    refreshFailure();
    return snapshot().lifecycle == OperationLifecycle::Paused;
}

bool DatasetCaptureService::resume(QString* error) {
    setError(error, {});
    refreshFailure();
    std::lock_guard lock(mutex_);
    if (lifecycle_ != OperationLifecycle::Paused)
        return setError(error, "Only a paused Dataset Capture can resume."), false;
    if (!lease_.transition(OperationLifecycle::Running))
        return setError(error, "Dataset Capture could not resume."), false;
    lifecycle_ = OperationLifecycle::Running;
    activeStartedNs_ = monotonicNow_();
    lastSourceDelivery_.reset();
    acceptingOffers_ = true;
    return true;
}

bool DatasetCaptureService::stop(QString* error) { return finish("user", error); }

bool DatasetCaptureService::pollDuration(QString* error) {
    refreshFailure();
    bool expired = false;
    {
        std::lock_guard lock(mutex_);
        expired = lifecycle_ == OperationLifecycle::Running && request_.durationSeconds &&
                  activeElapsedLocked(monotonicNow_()) >= *request_.durationSeconds;
    }
    return expired ? finish("duration", error) : true;
}

bool DatasetCaptureService::finish(const QString& reason, QString* error) {
    setError(error, {});
    refreshFailure();
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running &&
            lifecycle_ != OperationLifecycle::Paused)
            return setError(error, error_.isEmpty() ? "Dataset Capture is not active." : error_),
                   false;
        if (activeStartedNs_) {
            activeElapsedNs_ += (std::max)(qint64(0), monotonicNow_() - *activeStartedNs_);
            activeStartedNs_.reset();
        }
        acceptingOffers_ = false;
        lease_.transition(OperationLifecycle::Stopping);
        lifecycle_ = OperationLifecycle::Stopping;
    }
    const auto checkpoint = dispatcher_.closeDatasetBoundary();
    dispatcher_.waitThrough(checkpoint);
    dispatcher_.stopAndDrain();
    if (dispatcher_.faulted()) {
        QString message;
        {
            std::lock_guard lock(mutex_);
            message = error_.isEmpty() ? "Dataset Capture writing failed." : error_;
        }
        return failAndRelease("consumer_failure", message, error);
    }
    QString spoolError;
    if (!spool_ || !spool_->stop(&spoolError))
        return failAndRelease("spool_write_error",
                              spoolError.isEmpty() ? "Dataset Capture spool writing failed."
                                                   : spoolError,
                              error);
    const auto spoolMetrics = spool_->metrics();
    qint64 capturedFrames = 0;
    {
        std::lock_guard lock(mutex_);
        capturedFrames = savedFrameCount_;
    }
    if (spoolMetrics.acceptedFrames != capturedFrames ||
        spoolMetrics.persistedFrames != capturedFrames)
        return failAndRelease(
            "spool_integrity_error",
            QString("Dataset Capture spool count mismatch: captured %1, accepted %2, persisted %3.")
                .arg(capturedFrames)
                .arg(spoolMetrics.acceptedFrames)
                .arg(spoolMetrics.persistedFrames),
            error);
    if (capturedFrames == 0)
        return failAndRelease("no_frames", "No valid frames were captured.", error);
    QString finalizationError;
    if (!finalizeSpool(&finalizationError))
        return failAndRelease("finalization_error", finalizationError, error);
    if (!saveManifest("completed", reason, error))
        return failAndRelease("manifest_error", error ? *error : "Manifest save failed.", error);
    if (!QFile::remove(spoolPath_))
        return failAndRelease("spool_cleanup_error",
                              "The completed Dataset Capture spool could not be removed.", error);
    QFile::remove(partialPath_);
    std::lock_guard lock(mutex_);
    lease_.transition(OperationLifecycle::Completed);
    lease_.release();
    lifecycle_ = OperationLifecycle::Completed;
    return true;
}

bool DatasetCaptureService::finalizeSpool(QString* error) {
    qint64 totalFrames = 0;
    int width = 0;
    int height = 0;
    {
        std::lock_guard lock(mutex_);
        totalFrames = savedFrameCount_;
        width = width_;
        height = height_;
    }
    qint64 savedFrames = 0;
    qint64 failedOutputIndex = 0;
    if (!spool_->finalize(sequenceFolder_, totalFrames, width, height,
                          persistence::FramePersistenceService::writeTiffWithoutReplace,
                          &savedFrames, &failedOutputIndex, error))
        return false;
    return savedFrames == totalFrames ||
           (setError(error, "Dataset Capture finalization frame count mismatch."), false);
}

bool DatasetCaptureService::saveManifest(const QString& status, const QString& reason,
                                         QString* error) {
    DatasetManifestData data;
    {
        std::lock_guard lock(mutex_);
        if (savedFrameCount_ == 0)
            return setError(error, "No valid frames were captured."), false;
        data.datasetId = datasetId_;
        data.provenance.name = displayName_;
        data.provenance.experimentType = request_.experimentType;
        data.provenance.notes = request_.notes;
        data.provenance.opendssVersion = request_.opendssVersion;
        data.provenance.createdAt = createdAt_;
        data.provenance.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        data.provenance.captureStartedAt = startedAt_;
        data.provenance.captureEndedAt = data.provenance.updatedAt;
        data.provenance.requestedDurationSeconds = request_.durationSeconds;
        data.provenance.stopReason = reason;
        data.provenance.status = status;
        data.provenance.sequence.frameCount = savedFrameCount_;
        data.provenance.sequence.imageWidth = width_;
        data.provenance.sequence.imageHeight = height_;
        data.provenance.sequence.bitDepth = bitDepth_;
        data.provenance.sequence.nominalFps = fps_;
        data.provenance.sequence.integrity = integrityLocked();
        data.provenance.cameraSettings = request_.cameraSettings;
        data.provenance.detectionSettings = request_.detectionSettings;
        data.provenance.programSettings = request_.programSettings;
        data.records = records_;
    }
    if (!DatasetManifestV2::save(QDir(folder_).filePath("dataset.json"), data, error))
        return false;
    logFinalIntegrity(data.provenance.sequence.integrity);
    return true;
}

bool DatasetCaptureService::failAndRelease(const QString& reason, const QString& message,
                                           QString* error) {
    dispatcher_.closeDatasetBoundary();
    dispatcher_.stopAndDrain();
    if (spool_)
        spool_->stop(nullptr);
    bool hasFrames = false;
    {
        std::lock_guard lock(mutex_);
        hasFrames = savedFrameCount_ > 0;
        error_ = message;
        acceptingOffers_ = false;
        if (activeStartedNs_) {
            activeElapsedNs_ += (std::max)(qint64(0), monotonicNow_() - *activeStartedNs_);
            activeStartedNs_.reset();
        }
    }
    QString recoveryError;
    bool recoverySaved = false;
    if (hasFrames)
        recoverySaved = saveManifest("interrupted", reason, &recoveryError);
    else if (!partialPath_.isEmpty())
        desktop_app::writeJsonObjectAtomically(
            partialPath_, QJsonObject{{"schema_version", "opendss.dataset.partial.v1"},
                                      {"status", "failed"}, {"stop_reason", reason},
                                      {"error", message}, {"saved_frame_count", 0}},
            &recoveryError);
    if (!recoverySaved) {
        sequence::SequenceIntegrity integrity;
        {
            std::lock_guard lock(mutex_);
            integrity = integrityLocked();
        }
        logFinalIntegrity(integrity);
    }
    if (!recoveryError.isEmpty())
        qWarning().noquote() << "Dataset Capture recovery persistence failed:"
                             << recoveryError;
    {
        std::lock_guard lock(mutex_);
        if (lease_.isValid()) {
            lease_.transition(OperationLifecycle::Interrupted);
            lease_.release();
        }
        lifecycle_ = OperationLifecycle::Interrupted;
    }
    setError(error, message);
    return false;
}

void DatasetCaptureService::refreshFailure() {
    if (!dispatcher_.faulted())
        return;
    QString message;
    bool active = false;
    {
        std::lock_guard lock(mutex_);
        active = lifecycle_ == OperationLifecycle::Running ||
                 lifecycle_ == OperationLifecycle::Paused ||
                 lifecycle_ == OperationLifecycle::Stopping;
        message = error_.isEmpty() ? "Dataset Capture writing failed." : error_;
    }
    if (active)
        failAndRelease("consumer_failure", message, nullptr);
}

double DatasetCaptureService::activeElapsedLocked(qint64 now) const {
    qint64 elapsed = activeElapsedNs_;
    if (activeStartedNs_)
        elapsed += (std::max)(qint64(0), now - *activeStartedNs_);
    return static_cast<double>(elapsed) / 1'000'000'000.0;
}

sequence::SequenceIntegrity DatasetCaptureService::integrityLocked() const {
    sequence::SequenceIntegrity value;
    value.sourceFrameGaps = sourceGaps_;
    const auto dispatcherIntegrity = dispatcher_.datasetIntegrity();
    value.queueRejections =
        loss(dispatcherIntegrity.queueRejected, dispatcherIntegrity.queueRejectedCount);
    value.consumerFailures =
        loss(dispatcherIntegrity.consumerFailures, dispatcherIntegrity.consumerFailureCount);
    return value;
}

DatasetCaptureSnapshot DatasetCaptureService::snapshot() {
    refreshFailure();
    std::lock_guard lock(mutex_);
    return {lifecycle_, folder_, savedFrameCount_, records_.size(),
            activeElapsedLocked(monotonicNow_()), integrityLocked(), error_};
}
} // namespace desktop_app::v2::dataset
