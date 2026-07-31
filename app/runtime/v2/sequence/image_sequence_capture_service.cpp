#include "image_sequence_capture_service.h"

#include "../camera/camera_service.h"
#include "../camera/frame_conversion.h"
#include "../../desktop_app/json_persistence.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryFile>

#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <limits>
#include <stdexcept>
#include <thread>

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

constexpr qint64 kSpoolChunkBytes = 32 * 1024 * 1024;
constexpr int kSpoolBufferCount = 4;
constexpr int kSpoolFlushMilliseconds = 50;
constexpr qsizetype kSpoolRecordHeaderBytes = 40;
constexpr quint32 kSpoolRecordMagic = 0x5353444f; // ODSS

void appendLe32(QByteArray& bytes, quint32 value) {
    for (int shift = 0; shift != 32; shift += 8)
        bytes.append(static_cast<char>((value >> shift) & 0xff));
}

void appendLe64(QByteArray& bytes, quint64 value) {
    for (int shift = 0; shift != 64; shift += 8)
        bytes.append(static_cast<char>((value >> shift) & 0xff));
}

quint32 readLe32(const char* bytes) {
    const auto* value = reinterpret_cast<const uchar*>(bytes);
    return quint32(value[0]) | (quint32(value[1]) << 8) |
           (quint32(value[2]) << 16) | (quint32(value[3]) << 24);
}

quint64 readLe64(const char* bytes) {
    quint64 value = 0;
    for (int index = 7; index >= 0; --index)
        value = (value << 8) | static_cast<uchar>(bytes[index]);
    return value;
}

} // namespace

class ImageSequenceSpool final {
  public:
    struct Metrics {
        qint64 acceptedFrames = 0;
        qint64 persistedFrames = 0;
        qint64 rejectedFrames = 0;
        qint64 failureCount = 0;
        qint64 sequentialBytes = 0;
        qint64 peakBufferedBytes = 0;
        int queueHighWater = 0;
        int poolHighWater = 0;
    };

    ~ImageSequenceSpool() { stop(nullptr); }

    bool start(const QString& path, QString* error) {
        path_ = path;
        file_.setFileName(path_);
        if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setError(error, QStringLiteral("Could not create the Image Sequence spool: %1.")
                                .arg(file_.errorString()));
            return false;
        }
        for (int index = 0; index != kSpoolBufferCount; ++index)
            free_.emplace_back();
        worker_ = std::thread([this] { writeLoop(); });
        return true;
    }

    bool append(const QImage& image, const FrameMeta& meta, std::uint64_t handoffId,
                QString* error) {
        if (image.format() != QImage::Format_Grayscale8 || image.width() <= 0 ||
            image.height() <= 0) {
            setError(error, QStringLiteral("Image Sequence persistence requires an 8-bit grayscale frame."));
            return false;
        }
        const qint64 payloadBytes = static_cast<qint64>(image.width()) * image.height();
        const qint64 recordBytes = kSpoolRecordHeaderBytes + payloadBytes;
        if (payloadBytes > (std::numeric_limits<quint32>::max)() ||
            recordBytes > kSpoolChunkBytes) {
            setError(error, QStringLiteral("The Image Sequence frame is too large for the bounded spool."));
            return false;
        }

        std::lock_guard lock(mutex_);
        if (stopping_ || failureCount_ > 0) {
            ++rejectedFrames_;
            setError(error, failureMessage_.isEmpty()
                                ? QStringLiteral("The Image Sequence spool is no longer accepting frames.")
                                : failureMessage_);
            return false;
        }
        if (current_.isEmpty()) {
            if (free_.empty()) {
                ++rejectedFrames_;
                setError(error, QStringLiteral("The bounded Image Sequence spool buffer pool is full."));
                return false;
            }
            current_ = std::move(free_.front());
            free_.pop_front();
            current_.clear();
        }
        if (current_.size() + recordBytes > kSpoolChunkBytes) {
            if (free_.empty()) {
                ++rejectedFrames_;
                setError(error, QStringLiteral("The bounded Image Sequence spool buffer pool is full."));
                return false;
            }
            queueCurrentLocked();
            current_ = std::move(free_.front());
            free_.pop_front();
            current_.clear();
        }

        appendLe32(current_, kSpoolRecordMagic);
        appendLe32(current_, 1);
        appendLe64(current_, handoffId);
        appendLe64(current_, static_cast<quint64>(meta.frameIndex));
        appendLe32(current_, static_cast<quint32>(image.width()));
        appendLe32(current_, static_cast<quint32>(image.height()));
        appendLe32(current_, static_cast<quint32>(payloadBytes));
        appendLe32(current_, 8);
        for (int row = 0; row != image.height(); ++row) {
            current_.append(reinterpret_cast<const char*>(image.constScanLine(row)),
                            image.width());
        }
        ++currentFrames_;
        ++acceptedFrames_;
        bufferedBytes_ += recordBytes;
        peakBufferedBytes_ = (std::max)(peakBufferedBytes_, bufferedBytes_);
        poolHighWater_ = (std::max)(poolHighWater_, kSpoolBufferCount - int(free_.size()));
        return true;
    }

    bool stop(QString* error) {
        {
            std::lock_guard lock(mutex_);
            if (!stopping_) {
                stopping_ = true;
                queueCurrentLocked();
            }
        }
        condition_.notify_one();
        if (worker_.joinable())
            worker_.join();
        if (file_.isOpen()) {
            if (!file_.flush())
                recordFailure(QStringLiteral("Could not flush the Image Sequence spool: %1.")
                                  .arg(file_.errorString()));
            file_.close();
        }
        std::lock_guard lock(mutex_);
        if (failureCount_ > 0) {
            setError(error, failureMessage_.isEmpty()
                                ? QStringLiteral("Image Sequence spool writing failed.")
                                : failureMessage_);
            return false;
        }
        return true;
    }

    Metrics metrics() const {
        std::lock_guard lock(mutex_);
        return {acceptedFrames_, persistedFrames_, rejectedFrames_, failureCount_,
                sequentialBytes_, peakBufferedBytes_, queueHighWater_, poolHighWater_};
    }

  private:
    struct ReadyChunk {
        QByteArray bytes;
        qint64 frames = 0;
    };

    void queueCurrentLocked() {
        if (current_.isEmpty())
            return;
        ready_.push_back({std::move(current_), currentFrames_});
        current_.clear();
        currentFrames_ = 0;
        queueHighWater_ = (std::max)(queueHighWater_, int(ready_.size()));
        condition_.notify_one();
    }

    void recordFailure(const QString& message) {
        std::lock_guard lock(mutex_);
        ++failureCount_;
        if (failureMessage_.isEmpty())
            failureMessage_ = message;
    }

    void writeLoop() {
        for (;;) {
            ReadyChunk chunk;
            {
                std::unique_lock lock(mutex_);
                condition_.wait_for(lock, std::chrono::milliseconds(kSpoolFlushMilliseconds),
                                    [this] { return stopping_ || !ready_.empty(); });
                if (ready_.empty() && !current_.isEmpty())
                    queueCurrentLocked();
                if (!ready_.empty()) {
                    chunk = std::move(ready_.front());
                    ready_.pop_front();
                } else if (stopping_) {
                    break;
                } else {
                    continue;
                }
            }

            const qint64 written = file_.write(chunk.bytes);
            const bool flushed = written == chunk.bytes.size() && file_.flush();
            {
                std::lock_guard lock(mutex_);
                if (!flushed) {
                    ++failureCount_;
                    if (failureMessage_.isEmpty()) {
                        failureMessage_ = QStringLiteral("Could not append the Image Sequence spool: %1.")
                                              .arg(file_.errorString());
                    }
                } else {
                    persistedFrames_ += chunk.frames;
                    sequentialBytes_ += written;
                }
                bufferedBytes_ -= chunk.bytes.size();
                chunk.bytes.clear();
                free_.push_back(std::move(chunk.bytes));
            }
        }
    }

    QString path_;
    QFile file_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<QByteArray> free_;
    std::deque<ReadyChunk> ready_;
    QByteArray current_;
    qint64 currentFrames_ = 0;
    std::thread worker_;
    bool stopping_ = false;
    qint64 acceptedFrames_ = 0;
    qint64 persistedFrames_ = 0;
    qint64 rejectedFrames_ = 0;
    qint64 failureCount_ = 0;
    qint64 sequentialBytes_ = 0;
    qint64 bufferedBytes_ = 0;
    qint64 peakBufferedBytes_ = 0;
    int queueHighWater_ = 0;
    int poolHighWater_ = 0;
    QString failureMessage_;
};

ImageSequenceCaptureService::ImageSequenceCaptureService(CameraService& camera,
                                                         OperationCoordinator& operations,
                                                         MonotonicNow monotonicNow,
                                                         FrameConverter frameConverter,
                                                         FrameWriter frameWriter)
    : camera_(camera),
      operations_(operations),
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
        frameWriter_ = writeTiffWithoutReplace;
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
    auto spool = std::make_unique<ImageSequenceSpool>();
    if (!spool->start(spoolPath, &persistenceError)) {
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
    setError(error, {});
    QFile spool(spoolPath_);
    if (!spool.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not open the Image Sequence spool for finalization: %1.")
                            .arg(spool.errorString()));
        return false;
    }

    qint64 totalFrames = 0;
    {
        std::lock_guard lock(mutex_);
        totalFrames = capturedFrameCount_;
        savedFrameCount_ = 0;
    }
    quint64 previousHandoff = 0;
    for (qint64 outputIndex = 1; outputIndex <= totalFrames; ++outputIndex) {
        const QByteArray header = spool.read(kSpoolRecordHeaderBytes);
        if (header.size() != kSpoolRecordHeaderBytes) {
            std::lock_guard lock(mutex_);
            persistenceFailures_.count = totalFrames - outputIndex + 1;
            persistenceFailures_.ranges = {{outputIndex, totalFrames}};
            setError(error, QStringLiteral("Image Sequence spool ended before frame %1.")
                                .arg(outputIndex));
            return false;
        }
        const quint32 magic = readLe32(header.constData());
        const quint32 version = readLe32(header.constData() + 4);
        const quint64 handoffId = readLe64(header.constData() + 8);
        const quint64 sourceDelivery = readLe64(header.constData() + 16);
        const quint32 width = readLe32(header.constData() + 24);
        const quint32 height = readLe32(header.constData() + 28);
        const quint32 payloadBytes = readLe32(header.constData() + 32);
        const quint32 bits = readLe32(header.constData() + 36);
        const quint64 expectedPayload = static_cast<quint64>(width) * height;
        const bool orderValid = outputIndex == 1 || handoffId == previousHandoff + 1;
        if (magic != kSpoolRecordMagic || version != 1 || bits != 8 || width == 0 ||
            height == 0 || expectedPayload != payloadBytes ||
            width != static_cast<quint32>(imageWidth_) ||
            height != static_cast<quint32>(imageHeight_) || !orderValid ||
            sourceDelivery > static_cast<quint64>((std::numeric_limits<qint64>::max)())) {
            std::lock_guard lock(mutex_);
            persistenceFailures_.count = totalFrames - outputIndex + 1;
            persistenceFailures_.ranges = {{outputIndex, totalFrames}};
            setError(error, QStringLiteral("Image Sequence spool record %1 is invalid.")
                                .arg(outputIndex));
            return false;
        }
        previousHandoff = handoffId;

        const QByteArray pixels = spool.read(payloadBytes);
        if (pixels.size() != payloadBytes) {
            std::lock_guard lock(mutex_);
            persistenceFailures_.count = totalFrames - outputIndex + 1;
            persistenceFailures_.ranges = {{outputIndex, totalFrames}};
            setError(error, QStringLiteral("Image Sequence spool frame %1 is truncated.")
                                .arg(outputIndex));
            return false;
        }
        QImage image(static_cast<int>(width), static_cast<int>(height),
                     QImage::Format_Grayscale8);
        if (image.isNull()) {
            std::lock_guard lock(mutex_);
            persistenceFailures_.count = totalFrames - outputIndex + 1;
            persistenceFailures_.ranges = {{outputIndex, totalFrames}};
            setError(error, QStringLiteral("Could not allocate Image Sequence frame %1 during finalization.")
                                .arg(outputIndex));
            return false;
        }
        for (quint32 row = 0; row != height; ++row) {
            std::memcpy(image.scanLine(static_cast<int>(row)),
                        pixels.constData() + static_cast<qsizetype>(row) * width, width);
        }
        const QString target = QDir(framesFolder_)
                                   .filePath(QStringLiteral("frame_%1.tif")
                                                 .arg(outputIndex, 8, 10, QLatin1Char('0')));
        QString writeError;
        if (!frameWriter_(image, target, &writeError)) {
            std::lock_guard lock(mutex_);
            persistenceFailures_.count = totalFrames - outputIndex + 1;
            persistenceFailures_.ranges = {{outputIndex, totalFrames}};
            setError(error, writeError.isEmpty()
                                ? QStringLiteral("Could not finalize Image Sequence frame %1.")
                                      .arg(outputIndex)
                                : writeError);
            return false;
        }
        {
            std::lock_guard lock(mutex_);
            ++savedFrameCount_;
        }
    }
    if (!spool.atEnd()) {
        setError(error, QStringLiteral("Image Sequence spool contains unexpected trailing data."));
        return false;
    }
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
