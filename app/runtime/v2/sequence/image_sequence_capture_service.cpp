#include "image_sequence_capture_service.h"
#include "../persistence/frame_persistence_service.h"

#include "../camera/camera_service.h"
#include "../camera/frame_conversion.h"
#include "../../detection/droplet_frame_processor.h"
#include "../../desktop_app/json_persistence.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace desktop_app::v2::sequence {
namespace {

void setError(QString* error, const QString& message) {
    if (error)
        *error = message;
}

QString sanitizedName(const QString& requested) {
    QString leaf = requested.trimmed();
    leaf.replace('\\', '/');
    QString name = QFileInfo(leaf).fileName();
    name.replace(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N} _.-])")),
                 QStringLiteral("_"));
    name.remove(QRegularExpression(QStringLiteral(R"(^[ ._]+|[ ._]+$)")));
    if (name.isEmpty())
        name = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    static const QRegularExpression reserved(
        QStringLiteral(R"(^(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\..*)?$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (reserved.match(name).hasMatch())
        name.prepend('_');
    return name;
}

QString uniqueFolder(const QString& root, const QString& baseName) {
    QDir directory(root);
    QString candidate = directory.absoluteFilePath(baseName);
    for (int suffix = 2; QFileInfo::exists(candidate); ++suffix)
        candidate = directory.absoluteFilePath(baseName + QStringLiteral("-%1").arg(suffix));
    return candidate;
}

QString operationConflict(const OperationFault& fault) {
    QString message = fault.reason.trimmed();
    if (!fault.recovery.trimmed().isEmpty())
        message += (message.isEmpty() ? QString{} : QStringLiteral(" ")) + fault.recovery.trimmed();
    return message.isEmpty() ? QStringLiteral("Camera or storage is in use by another operation.")
                             : message;
}

SequenceLossCategory category(const std::vector<LiveFrameDispatcher::Range>& ranges,
                              std::uint64_t count) {
    SequenceLossCategory result;
    result.count = static_cast<qint64>(count);
    for (const auto& range : ranges)
        result.ranges.push_back({static_cast<qint64>(range.first),
                                 static_cast<qint64>(range.last)});
    return result;
}

QJsonObject categoryJson(const SequenceLossCategory& value) {
    QJsonArray ranges;
    for (const auto& range : value.ranges)
        ranges.push_back(QJsonObject{{"first", range.first}, {"last", range.last}});
    return QJsonObject{{"count", value.count}, {"ranges", ranges}};
}

QJsonObject integrityJson(const SequenceIntegrity& value) {
    return QJsonObject{
        {"source_frame_gaps", categoryJson(value.sourceFrameGaps)},
        {"queue_rejections", categoryJson(value.queueRejections)},
        {"consumer_failures", categoryJson(value.consumerFailures)},
    };
}

} // namespace

ImageSequenceCaptureService::ImageSequenceCaptureService(CameraService& camera,
                                                         OperationCoordinator& operations,
                                                         DropletFrameProcessor& processor,
                                                         MonotonicNow monotonicNow,
                                                         FrameConverter frameConverter,
                                                         FrameWriter frameWriter)
    : camera_(camera),
      operations_(operations),
      processor_(processor),
      monotonicNow_(std::move(monotonicNow)),
      frameConverter_(std::move(frameConverter)),
      frameWriter_(std::move(frameWriter)),
      dispatcher_([this](const QImage& image, const FrameMeta& meta, double fps,
                         std::uint64_t handoffId, LiveFrameDispatcher::Membership membership) {
          consumeFrame(image, meta, fps, handoffId, membership);
      }) {
    if (!frameConverter_) {
        frameConverter_ = [](const CameraFrame& frame, QString* error) {
            return convertCameraFrame(frame, error);
        };
    }
    if (!frameWriter_)
        frameWriter_ = persistence::FramePersistenceService::writeTiffWithoutReplace;
}

ImageSequenceCaptureService::~ImageSequenceCaptureService() = default;

bool ImageSequenceCaptureService::start(const ImageSequenceCaptureRequest& request, QString* error) {
    setError(error, {});
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Idle) {
            setError(error, QStringLiteral("This Image Sequence capture has already started."));
            return false;
        }
    }
    if (!monotonicNow_) {
        setError(error, QStringLiteral("A monotonic clock is required."));
        return false;
    }
    if (request.durationSeconds &&
        (!std::isfinite(*request.durationSeconds) || *request.durationSeconds <= 0.0)) {
        setError(error, QStringLiteral("Duration must be empty or a finite positive number."));
        return false;
    }
    if (camera_.state().status != CameraStatus::Streaming) {
        setError(error, QStringLiteral("The camera must be streaming."));
        return false;
    }
    const QFileInfo rootInfo(request.saveRoot);
    if (!rootInfo.isDir() || !rootInfo.isWritable()) {
        setError(error, QStringLiteral("The selected save root must be a writable directory."));
        return false;
    }

    auto acquired = operations_.acquire(
        OperationKind::ImageSequence,
        ResourceLock::Camera | ResourceLock::Storage | ResourceLock::Sequence);
    if (!acquired.acquired()) {
        setError(error, acquired.fault ? operationConflict(*acquired.fault)
                                      : QStringLiteral("Image Sequence resources are in use."));
        return false;
    }

    const QString displayName = sanitizedName(request.name);
    const QString folder = uniqueFolder(request.saveRoot, displayName);
    const QString framesFolder = QDir(folder).filePath(QStringLiteral("frames"));
    if (!QDir().mkpath(framesFolder)) {
        setError(error, QStringLiteral("The Image Sequence folder could not be created."));
        return false;
    }
    const QString createdAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString partialPath = QDir(folder).filePath(QStringLiteral("sequence.partial.json"));
    const QString spoolPath = QDir(folder).filePath(QStringLiteral("sequence.frames.partial"));
    QString persistenceError;
    if (!desktop_app::writeJsonObjectAtomically(
            partialPath,
            QJsonObject{{"schema_version", "opendss.sequence.partial.v1"},
                         {"sequence_id", QFileInfo(folder).fileName()},
                         {"status", "in_progress"},
                         {"persistence_bit_depth", 8},
                         {"spool_file", QFileInfo(spoolPath).fileName()},
                         {"created_at", createdAt}},
            &persistenceError)) {
        setError(error, persistenceError);
        return false;
    }
    auto spool = std::make_unique<persistence::FramePersistenceService>();
    if (!spool->start(spoolPath, &persistenceError)) {
        setError(error, persistenceError);
        return false;
    }
    processor_.reset();
    if (!acquired.lease.transition(OperationLifecycle::Running)) {
        setError(error, QStringLiteral("Image Sequence could not enter Running state."));
        return false;
    }

    dispatcher_.openCollectionBoundary();
    const qint64 now = monotonicNow_();
    std::lock_guard lock(mutex_);
    lease_ = std::move(acquired.lease);
    lifecycle_ = OperationLifecycle::Running;
    request_ = request;
    sequenceId_ = QFileInfo(folder).fileName();
    displayName_ = displayName;
    folder_ = QFileInfo(folder).absoluteFilePath();
    framesFolder_ = QFileInfo(framesFolder).absoluteFilePath();
    partialPath_ = partialPath;
    spoolPath_ = spoolPath;
    createdAt_ = createdAt;
    startedAt_ = createdAt;
    activeStartedNs_ = now;
    acceptingOffers_ = true;
    spool_ = std::move(spool);
    return true;
}

bool ImageSequenceCaptureService::offerFrame(const CameraFrame& frame, double nominalFps,
                                             QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running || !acceptingOffers_) {
            setError(error, QStringLiteral("Image Sequence is not accepting frames."));
            return false;
        }
    }
    if (frame.deliveryId >
        static_cast<quint64>((std::numeric_limits<qint64>::max)())) {
        return failAndRelease(QStringLiteral("Camera delivery ID is outside the supported range."),
                              QStringLiteral("delivery_error"), error);
    }
    QString conversionError;
    const QImage image = frameConverter_(frame, &conversionError);
    if (image.isNull())
        return failAndRelease(conversionError, QStringLiteral("conversion_error"), error);
    if (!std::isfinite(nominalFps) || nominalFps <= 0.0)
        return failAndRelease(QStringLiteral("Frame rate must be finite and positive."),
                              QStringLiteral("frame_rate_error"), error);

    FrameMeta meta;
    bool nonIncreasingDelivery = false;
    LiveFrameDispatcher::OfferResult result;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running || !acceptingOffers_) {
            setError(error, QStringLiteral("Image Sequence is not accepting frames."));
            return false;
        }
        nonIncreasingDelivery =
            lastSourceDelivery_ && frame.deliveryId <= *lastSourceDelivery_;
        if (!nonIncreasingDelivery && lastSourceDelivery_ &&
            frame.deliveryId > *lastSourceDelivery_ + 1) {
            const qint64 first = static_cast<qint64>(*lastSourceDelivery_ + 1);
            const qint64 last = static_cast<qint64>(frame.deliveryId - 1);
            sourceGaps_.ranges.push_back({first, last});
            sourceGaps_.count += last - first + 1;
            qWarning().noquote() << "Image Sequence source frame gap" << first << "-" << last;
        }
        lastSourceDelivery_ = frame.deliveryId;
        meta.width = frame.width;
        meta.height = frame.height;
        meta.bits = frame.bitDepth;
        meta.frameIndex = static_cast<qint64>(frame.deliveryId);
        meta.delivered = dispatcherDelivery_ + 1;
        if (!nonIncreasingDelivery) {
            LiveFrameDispatcher::Membership membership;
            membership.collection = true;
            membership.sequenceRunning = true;
            result = dispatcher_.offer(image, meta, nominalFps, membership);
            if (result.accepted) {
                dispatcherDelivery_ = meta.delivered;
                lastAcceptedHandoff_ = result.handoffId;
            }
        }
    }
    if (nonIncreasingDelivery)
        return failAndRelease(QStringLiteral("Camera delivery IDs must increase."),
                              QStringLiteral("delivery_error"), error);
    if (!result.accepted) {
        qWarning().noquote() << "Image Sequence queue rejected handoff" << result.handoffId;
        const auto dispatcherIntegrity = dispatcher_.integrity();
        qint64 savedFrames = 0;
        {
            std::lock_guard lock(mutex_);
            savedFrames = capturedFrameCount_;
            error_ =
                QStringLiteral("Image Sequence save queue is degraded. Attempted so far: %1; "
                               "spooled so far: %2; rejected: %3. This recording will fail.")
                    .arg(dispatcherIntegrity.handoffAccepted +
                         dispatcherIntegrity.queueRejectedCount)
                    .arg(savedFrames)
                    .arg(dispatcherIntegrity.queueRejectedCount);
            setError(error, error_);
        }
        return false;
    }
    return true;
}

bool ImageSequenceCaptureService::pause(QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    std::uint64_t checkpoint = 0;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running) {
            setError(error, QStringLiteral("Only a running Image Sequence can be paused."));
            return false;
        }
        acceptingOffers_ = false;
        const qint64 now = monotonicNow_();
        activeElapsedNs_ += (std::max)(qint64(0), now - *activeStartedNs_);
        activeStartedNs_.reset();
        checkpoint = lastAcceptedHandoff_;
        if (!lease_.transition(OperationLifecycle::Paused)) {
            setError(error, QStringLiteral("Image Sequence could not enter Paused state."));
            return false;
        }
        lifecycle_ = OperationLifecycle::Paused;
    }
    dispatcher_.waitThrough(checkpoint);
    refreshAsyncFailure();
    return snapshot().lifecycle == OperationLifecycle::Paused;
}

bool ImageSequenceCaptureService::resume(QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    std::lock_guard lock(mutex_);
    if (lifecycle_ != OperationLifecycle::Paused) {
        setError(error, QStringLiteral("Only a paused Image Sequence can be resumed."));
        return false;
    }
    if (!lease_.transition(OperationLifecycle::Running)) {
        setError(error, QStringLiteral("Image Sequence could not resume."));
        return false;
    }
    lifecycle_ = OperationLifecycle::Running;
    activeStartedNs_ = monotonicNow_();
    lastSourceDelivery_.reset();
    acceptingOffers_ = true;
    return true;
}

bool ImageSequenceCaptureService::stop(QString* error) {
    return stopWithReason(QStringLiteral("user"), error);
}

bool ImageSequenceCaptureService::stopForDuration(QString* error) {
    return stopWithReason(QStringLiteral("duration"), error);
}

bool ImageSequenceCaptureService::durationExpired() {
    std::lock_guard lock(mutex_);
    return lifecycle_ == OperationLifecycle::Running && request_.durationSeconds &&
           activeElapsedLocked(monotonicNow_()) >= *request_.durationSeconds;
}

bool ImageSequenceCaptureService::pollDuration(QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    bool expired = false;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ == OperationLifecycle::Running && request_.durationSeconds)
            expired = activeElapsedLocked(monotonicNow_()) >= *request_.durationSeconds;
    }
    return expired ? stopForDuration(error) : true;
}

ImageSequenceCaptureSnapshot ImageSequenceCaptureService::snapshot() {
    refreshAsyncFailure();
    std::lock_guard lock(mutex_);
    return {lifecycle_, folder_, capturedFrameCount_, savedFrameCount_,
            activeElapsedLocked(monotonicNow_()), combinedIntegrity(), error_};
}

void ImageSequenceCaptureService::consumeFrame(const QImage& image, const FrameMeta& meta,
                                               double fps, std::uint64_t handoffId,
                                               LiveFrameDispatcher::Membership) {
    QString spoolError;
    {
        std::lock_guard lock(mutex_);
        if (formatFixed_ &&
            (meta.width != imageWidth_ || meta.height != imageHeight_ ||
             fps != nominalFps_)) {
            error_ = QStringLiteral("Image Sequence frame format changed during capture.");
            qWarning().noquote() << "Image Sequence frame format changed. Failed handoff"
                                 << handoffId << "-" << handoffId;
            throw std::runtime_error("frame format mismatch");
        }
        if (!formatFixed_) {
            imageWidth_ = image.width();
            imageHeight_ = image.height();
            bitDepth_ = 8;
            nominalFps_ = fps;
            formatFixed_ = true;
        }
    }
    cv::Mat frame(image.height(), image.width(), CV_8UC1,
                  const_cast<uchar*>(image.constBits()), image.bytesPerLine());
    const DropletFrameProcessingResult processed = processor_.process(frame);
    if (processed.detection.capacityExceeded) {
        std::lock_guard lock(mutex_);
        error_ = QStringLiteral("Detector track capacity exceeded.");
        throw std::runtime_error("track capacity exceeded");
    }
    if (processed.cropFailed) {
        std::lock_guard lock(mutex_);
        error_ = processed.cropError.isEmpty()
                     ? QStringLiteral("Detector crop extraction failed.")
                     : processed.cropError;
        throw std::runtime_error("crop failed");
    }
    if (!spool_ || !spool_->append(image, meta, handoffId, &spoolError)) {
        {
            std::lock_guard lock(mutex_);
            error_ = spoolError.isEmpty() ? QStringLiteral("Image Sequence spooling failed.")
                                          : spoolError;
        }
        qWarning().noquote() << "Image Sequence consumer failure initiating handoff"
                             << handoffId << "-" << handoffId << ":" << spoolError;
        throw std::runtime_error("frame spool failure");
    }
    std::lock_guard lock(mutex_);
    ++capturedFrameCount_;
}

bool ImageSequenceCaptureService::finalizeSpool(QString* error) {
    qint64 totalFrames = 0;
    {
        std::lock_guard lock(mutex_);
        totalFrames = capturedFrameCount_;
        savedFrameCount_ = 0;
    }
    qint64 savedFrames = 0;
    qint64 failedOutputIndex = 0;
    if (!spool_->finalize(framesFolder_, totalFrames, imageWidth_, imageHeight_, frameWriter_,
                          &savedFrames, &failedOutputIndex, error)) {
        std::lock_guard lock(mutex_);
        persistenceFailures_.count = 0;
        persistenceFailures_.ranges.clear();
        if (failedOutputIndex > 0) {
            persistenceFailures_.count = totalFrames - failedOutputIndex + 1;
            persistenceFailures_.ranges = {{failedOutputIndex, totalFrames}};
        }
        savedFrameCount_ = savedFrames;
        return false;
    }
    std::lock_guard lock(mutex_);
    savedFrameCount_ = savedFrames;
    return true;
}

bool ImageSequenceCaptureService::stopWithReason(const QString& reason, QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ != OperationLifecycle::Running &&
            lifecycle_ != OperationLifecycle::Paused) {
            setError(error, error_.isEmpty() ? QStringLiteral("Image Sequence is not active.")
                                             : error_);
            return false;
        }
        if (activeStartedNs_) {
            const qint64 now = monotonicNow_();
            activeElapsedNs_ += (std::max)(qint64(0), now - *activeStartedNs_);
            activeStartedNs_.reset();
        }
        acceptingOffers_ = false;
        lease_.transition(OperationLifecycle::Stopping);
        lifecycle_ = OperationLifecycle::Stopping;
    }
    const std::uint64_t checkpoint = dispatcher_.closeCollectionBoundary();
    dispatcher_.waitThrough(checkpoint);
    dispatcher_.stopAndDrain();
    if (dispatcher_.faulted()) {
        QString message;
        {
            std::lock_guard lock(mutex_);
            message = error_.isEmpty() ? QStringLiteral("Image Sequence frame writing failed.")
                                       : error_;
        }
        return failAndRelease(message, QStringLiteral("consumer_failure"), error);
    }
    QString spoolError;
    if (!spool_ || !spool_->stop(&spoolError))
        return failAndRelease(spoolError.isEmpty()
                                  ? QStringLiteral("Image Sequence spool writing failed.")
                                  : spoolError,
                              QStringLiteral("spool_write_error"), error);
    const auto spoolMetrics = spool_->metrics();
    qint64 capturedFrames = 0;
    {
        std::lock_guard lock(mutex_);
        capturedFrames = capturedFrameCount_;
    }
    if (spoolMetrics.acceptedFrames != capturedFrames ||
        spoolMetrics.persistedFrames != capturedFrames) {
        return failAndRelease(
            QStringLiteral("Image Sequence spool count mismatch: captured %1, accepted %2, persisted %3.")
                .arg(capturedFrames)
                .arg(spoolMetrics.acceptedFrames)
                .arg(spoolMetrics.persistedFrames),
            QStringLiteral("spool_integrity_error"), error);
    }

    SequenceManifestData manifest;
    bool noFrames = false;
    qint64 rejectedFrames = 0;
    qint64 attemptedFrames = 0;
    double activeElapsedSeconds = 0.0;
    {
        std::lock_guard lock(mutex_);
        noFrames = capturedFrameCount_ == 0;
        rejectedFrames = combinedIntegrity().queueRejections.count;
        attemptedFrames = capturedFrameCount_ + rejectedFrames;
        activeElapsedSeconds = activeElapsedLocked(monotonicNow_());
        if (!noFrames) {
            manifest.sequenceId = sequenceId_;
            manifest.name = displayName_;
            manifest.experimentType = request_.experimentType;
            manifest.notes = request_.notes;
            manifest.status = QStringLiteral("completed");
            manifest.createdAt = createdAt_;
            manifest.startedAt = startedAt_;
            manifest.endedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
            manifest.requestedDurationSeconds = request_.durationSeconds;
            manifest.stopReason = reason;
            manifest.opendssVersion = request_.opendssVersion;
            manifest.frameCount = capturedFrameCount_;
            manifest.cameraSettings = request_.cameraSettings;
            manifest.imageWidth = imageWidth_;
            manifest.imageHeight = imageHeight_;
            manifest.bitDepth = bitDepth_;
            manifest.nominalFps = nominalFps_;
            manifest.integrity = combinedIntegrity();
        }
    }
    if (noFrames)
        return failAndRelease(QStringLiteral("No frames were captured."), reason, error);
    if (rejectedFrames > 0) {
        QString message =
            QStringLiteral("Image Sequence failed because the save queue rejected frame "
                           "handoffs. Attempted: %1; spooled: %2; rejected: %3.")
                .arg(attemptedFrames)
                .arg(attemptedFrames - rejectedFrames)
                .arg(rejectedFrames);
        if (activeElapsedSeconds > 0.0) {
            message +=
                QStringLiteral(" Rates: attempted %1 fps; spooled %2 fps; rejected %3 fps.")
                    .arg(attemptedFrames / activeElapsedSeconds, 0, 'f', 2)
                    .arg((attemptedFrames - rejectedFrames) / activeElapsedSeconds,
                         0, 'f', 2)
                    .arg(rejectedFrames / activeElapsedSeconds, 0, 'f', 2);
        }
        return failAndRelease(message, QStringLiteral("queue_rejection"), error);
    }
    QString finalizationError;
    if (!finalizeSpool(&finalizationError))
        return failAndRelease(finalizationError, QStringLiteral("finalization_error"), error);
    {
        std::lock_guard lock(mutex_);
        manifest.frameCount = savedFrameCount_;
    }
    QString manifestError;
    const QString manifestPath = QDir(folder_).filePath(QStringLiteral("sequence.json"));
    if (!SequenceManifestV2::save(manifestPath, manifest, &manifestError))
        return failAndRelease(manifestError, QStringLiteral("manifest_error"), error);
    if (!QFile::remove(spoolPath_))
        return failAndRelease(QStringLiteral("The completed Image Sequence spool could not be removed."),
                              QStringLiteral("spool_cleanup_error"), error);
    if (!QFile::remove(partialPath_))
        return failAndRelease(QStringLiteral("The recovery marker could not be removed."),
                              QStringLiteral("recovery_marker_error"), error);

    std::lock_guard lock(mutex_);
    lease_.transition(OperationLifecycle::Completed);
    lease_.release();
    lifecycle_ = OperationLifecycle::Completed;
    return true;
}

bool ImageSequenceCaptureService::failAndRelease(const QString& message,
                                                 const QString& stopReason,
                                                 QString* error) {
    dispatcher_.closeCollectionBoundary();
    dispatcher_.stopAndDrain();
    if (spool_)
        spool_->stop(nullptr);
    {
        std::lock_guard lock(mutex_);
        error_ = message;
        acceptingOffers_ = false;
        if (activeStartedNs_) {
            activeElapsedNs_ +=
                (std::max)(qint64(0), monotonicNow_() - *activeStartedNs_);
            activeStartedNs_.reset();
        }
        if (lease_.isValid()) {
            lease_.transition(OperationLifecycle::Failed);
            lease_.release();
        }
        lifecycle_ = OperationLifecycle::Failed;
    }
    updateFailedRecovery(stopReason, message);
    setError(error, message);
    return false;
}

void ImageSequenceCaptureService::updateFailedRecovery(const QString& stopReason,
                                                       const QString& message) {
    QString partialPath;
    QString sequenceId;
    QString createdAt;
    QString spoolPath;
    qint64 capturedCount = 0;
    qint64 savedCount = 0;
    SequenceIntegrity integrity;
    {
        std::lock_guard lock(mutex_);
        partialPath = partialPath_;
        sequenceId = sequenceId_;
        createdAt = createdAt_;
        spoolPath = spoolPath_;
        capturedCount = capturedFrameCount_;
        savedCount = savedFrameCount_;
        integrity = combinedIntegrity();
    }
    if (partialPath.isEmpty())
        return;
    if (integrity.consumerFailures.count > 0) {
        QStringList ranges;
        for (const auto& range : integrity.consumerFailures.ranges)
            ranges.push_back(QStringLiteral("%1-%2").arg(range.first).arg(range.last));
        qWarning().noquote()
            << "Image Sequence aggregate consumer failures: count"
            << integrity.consumerFailures.count << "ranges" << ranges.join(',');
    }
    QString recoveryError;
    if (!desktop_app::writeJsonObjectAtomically(
            partialPath,
            QJsonObject{{"schema_version", "opendss.sequence.partial.v1"},
                        {"sequence_id", sequenceId},
                        {"status", "failed"},
                        {"created_at", createdAt},
                        {"updated_at", QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
                         {"stop_reason", stopReason},
                         {"error", message},
                         {"captured_frame_count", capturedCount},
                         {"saved_frame_count", savedCount},
                         {"spool_file", QFileInfo(spoolPath).fileName()},
                         {"integrity", integrityJson(integrity)}},
            &recoveryError)) {
        qWarning().noquote() << "Image Sequence failed recovery update:" << recoveryError;
    }
}

void ImageSequenceCaptureService::refreshAsyncFailure() {
    if (!dispatcher_.faulted())
        return;
    bool active = false;
    QString message;
    {
        std::lock_guard lock(mutex_);
        active = lifecycle_ == OperationLifecycle::Running ||
                 lifecycle_ == OperationLifecycle::Paused ||
                 lifecycle_ == OperationLifecycle::Stopping;
        message = error_.isEmpty() ? QStringLiteral("Image Sequence frame writing failed.") : error_;
    }
    if (active)
        failAndRelease(message, QStringLiteral("consumer_failure"), nullptr);
}

double ImageSequenceCaptureService::activeElapsedLocked(qint64 now) const {
    qint64 elapsed = activeElapsedNs_;
    if (activeStartedNs_)
        elapsed += (std::max)(qint64(0), now - *activeStartedNs_);
    return static_cast<double>(elapsed) / 1'000'000'000.0;
}

SequenceIntegrity ImageSequenceCaptureService::combinedIntegrity() const {
    SequenceIntegrity integrity;
    integrity.sourceFrameGaps = sourceGaps_;
    const auto dispatcherIntegrity = dispatcher_.integrity();
    integrity.queueRejections =
        category(dispatcherIntegrity.queueRejected, dispatcherIntegrity.queueRejectedCount);
    integrity.consumerFailures =
        category(dispatcherIntegrity.consumerFailures, dispatcherIntegrity.consumerFailureCount);
    integrity.consumerFailures.count += persistenceFailures_.count;
    for (const auto& range : persistenceFailures_.ranges)
        integrity.consumerFailures.ranges.push_back(range);
    return integrity;
}

} // namespace desktop_app::v2::sequence
