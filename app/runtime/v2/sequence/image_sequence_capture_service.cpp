#include "image_sequence_capture_service.h"

#include "../camera/camera_service.h"
#include "../camera/frame_conversion.h"
#include "../../desktop_app/json_persistence.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryFile>

#include <cmath>
#include <cstring>
#include <limits>
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

bool publishWithoutReplace(const QString& temporaryPath, const QString& targetPath,
                           QString* detail) {
#ifdef Q_OS_WIN
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()),
                    reinterpret_cast<LPCWSTR>(targetPath.utf16()),
                    MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    if (detail)
        *detail = QStringLiteral("Windows error %1").arg(GetLastError());
    return false;
#else
    const QByteArray temporaryNative = QFile::encodeName(temporaryPath);
    const QByteArray targetNative = QFile::encodeName(targetPath);
    if (::link(temporaryNative.constData(), targetNative.constData()) == 0 &&
        ::unlink(temporaryNative.constData()) == 0) {
        return true;
    }
    if (detail)
        *detail = QString::fromLocal8Bit(std::strerror(errno));
    return false;
#endif
}

bool writeTiffWithoutReplace(const QImage& image, const QString& target, QString* error) {
    if (QFileInfo::exists(target)) {
        setError(error, QStringLiteral("A frame already exists at %1.")
                            .arg(QDir::toNativeSeparators(target)));
        return false;
    }
    QString temporaryPath;
    {
        QTemporaryFile temporary(QDir(QFileInfo(target).absolutePath())
                                     .absoluteFilePath(QStringLiteral(".frame-XXXXXX.tmp")));
        if (!temporary.open()) {
            setError(error, QStringLiteral("Could not create a temporary frame: %1.")
                                .arg(temporary.errorString()));
            return false;
        }
        QImageWriter writer(&temporary, "tiff");
        if (!writer.write(image) || !temporary.flush()) {
            setError(error, QStringLiteral("Could not write a TIFF frame: %1.")
                                .arg(writer.errorString()));
            return false;
        }
        temporaryPath = temporary.fileName();
        temporary.close();
        temporary.setAutoRemove(false);
    }
    QString detail;
    if (!publishWithoutReplace(temporaryPath, target, &detail)) {
        QFile::remove(temporaryPath);
        setError(error, QStringLiteral("Could not publish the TIFF frame without replacement: %1.")
                            .arg(detail));
        return false;
    }
    QImageReader reader(target);
    if (!reader.canRead()) {
        setError(error, QStringLiteral("The published TIFF frame is not readable."));
        return false;
    }
    return true;
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
                                                         MonotonicNow monotonicNow,
                                                         FrameConverter frameConverter)
    : camera_(camera),
      operations_(operations),
      monotonicNow_(std::move(monotonicNow)),
      frameConverter_(std::move(frameConverter)),
      dispatcher_([this](const QImage& image, const FrameMeta& meta, double fps,
                         std::uint64_t handoffId, LiveFrameDispatcher::Membership membership) {
          consumeFrame(image, meta, fps, handoffId, membership);
      }) {
    if (!frameConverter_) {
        frameConverter_ = [](const CameraFrame& frame, QString* error) {
            return convertCameraFrame(frame, error);
        };
    }
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
    QString persistenceError;
    if (!desktop_app::writeJsonObjectAtomically(
            partialPath,
            QJsonObject{{"schema_version", "opendss.sequence.partial.v1"},
                        {"sequence_id", QFileInfo(folder).fileName()},
                        {"status", "in_progress"},
                        {"created_at", createdAt}},
            &persistenceError)) {
        setError(error, persistenceError);
        return false;
    }
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
    createdAt_ = createdAt;
    startedAt_ = createdAt;
    activeStartedNs_ = now;
    acceptingOffers_ = true;
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
    if (!result.accepted)
        qWarning().noquote() << "Image Sequence queue rejected handoff" << result.handoffId;
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

bool ImageSequenceCaptureService::pollDuration(QString* error) {
    setError(error, {});
    refreshAsyncFailure();
    bool expired = false;
    {
        std::lock_guard lock(mutex_);
        if (lifecycle_ == OperationLifecycle::Running && request_.durationSeconds)
            expired = activeElapsedLocked(monotonicNow_()) >= *request_.durationSeconds;
    }
    return expired ? stopWithReason(QStringLiteral("duration"), error) : true;
}

ImageSequenceCaptureSnapshot ImageSequenceCaptureService::snapshot() {
    refreshAsyncFailure();
    std::lock_guard lock(mutex_);
    return {lifecycle_, folder_, savedFrameCount_, activeElapsedLocked(monotonicNow_()),
            combinedIntegrity(), error_};
}

void ImageSequenceCaptureService::consumeFrame(const QImage& image, const FrameMeta& meta,
                                               double fps, std::uint64_t handoffId,
                                               LiveFrameDispatcher::Membership) {
    QString target;
    {
        std::lock_guard lock(mutex_);
        if (formatFixed_ &&
            (meta.width != imageWidth_ || meta.height != imageHeight_ ||
             meta.bits != bitDepth_ || fps != nominalFps_)) {
            error_ = QStringLiteral("Image Sequence frame format changed during capture.");
            qWarning().noquote() << error_ << "Failed handoff" << handoffId << "-"
                                 << handoffId;
            throw std::runtime_error("frame format mismatch");
        }
        target = QDir(framesFolder_)
                     .filePath(QStringLiteral("frame_%1.tif")
                                   .arg(savedFrameCount_ + 1, 8, 10, QLatin1Char('0')));
    }
    QString writeError;
    if (!writeTiffWithoutReplace(image, target, &writeError)) {
        std::lock_guard lock(mutex_);
        error_ = writeError;
        qWarning().noquote() << "Image Sequence consumer failure at handoff" << handoffId
                             << "-" << handoffId << ":" << error_;
        throw std::runtime_error("frame write failure");
    }
    std::lock_guard lock(mutex_);
    if (!formatFixed_) {
        imageWidth_ = meta.width;
        imageHeight_ = meta.height;
        bitDepth_ = meta.bits;
        nominalFps_ = fps;
        formatFixed_ = true;
    }
    ++savedFrameCount_;
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

    SequenceManifestData manifest;
    bool noFrames = false;
    {
        std::lock_guard lock(mutex_);
        noFrames = savedFrameCount_ == 0;
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
            manifest.frameCount = savedFrameCount_;
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
    QString manifestError;
    const QString manifestPath = QDir(folder_).filePath(QStringLiteral("sequence.json"));
    if (!SequenceManifestV2::save(manifestPath, manifest, &manifestError))
        return failAndRelease(manifestError, QStringLiteral("manifest_error"), error);
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
    qint64 savedCount = 0;
    SequenceIntegrity integrity;
    {
        std::lock_guard lock(mutex_);
        partialPath = partialPath_;
        sequenceId = sequenceId_;
        createdAt = createdAt_;
        savedCount = savedFrameCount_;
        integrity = combinedIntegrity();
    }
    if (partialPath.isEmpty())
        return;
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
                        {"saved_frame_count", savedCount},
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
    return integrity;
}

} // namespace desktop_app::v2::sequence
