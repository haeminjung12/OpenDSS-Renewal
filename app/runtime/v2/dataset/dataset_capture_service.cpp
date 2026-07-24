#include "dataset_capture_service.h"

#include "../../crops/crop_service.h"
#include "../../detection/droplet_detector.h"
#include "../../desktop_app/json_persistence.h"

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

QString uniqueFolder(const QString& root, const QString& name) {
    QString candidate = QDir(root).absoluteFilePath(name);
    for (int suffix = 2; QFileInfo::exists(candidate); ++suffix)
        candidate = QDir(root).absoluteFilePath(name + "-" + QString::number(suffix));
    return candidate;
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
} // namespace

DatasetCaptureService::DatasetCaptureService(OperationCoordinator& operations,
                                             IDropletDetector& detector,
                                             MonotonicNow monotonicNow)
    : operations_(operations),
      detector_(detector),
      monotonicNow_(std::move(monotonicNow)),
      dispatcher_([this](const QImage& image, const FrameMeta& meta, double fps,
                         std::uint64_t handoffId, LiveFrameDispatcher::Membership membership) {
          consumeFrame(image, meta, fps, handoffId, membership);
      }) {}

DatasetCaptureService::~DatasetCaptureService() = default;

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

    auto acquired = operations_.acquire(OperationKind::DatasetCapture,
                                        ResourceLock::Camera | ResourceLock::Storage |
                                            ResourceLock::Dataset);
    if (!acquired.acquired())
        return setError(error, acquired.fault ? acquired.fault->reason
                                             : "Dataset Capture resources are in use."), false;
    const QString displayName = cleanName(request.name);
    const QString folder = uniqueFolder(request.saveRoot, displayName);
    const QString sequenceFolder = QDir(folder).filePath("sequence");
    const QString cropsFolder = QDir(folder).filePath("crops");
    if (!QDir().mkpath(sequenceFolder) || !QDir().mkpath(cropsFolder))
        return setError(error, "Could not create Dataset folders."), false;
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const QString partial = QDir(folder).filePath("dataset.partial.json");
    QString persistenceError;
    if (!desktop_app::writeJsonObjectAtomically(
            partial, QJsonObject{{"schema_version", "opendss.dataset.partial.v1"},
                                 {"status", "in_progress"}, {"created_at", now}},
            &persistenceError))
        return setError(error, persistenceError), false;
    detector_.reset();
    if (!acquired.lease.transition(OperationLifecycle::Running))
        return setError(error, "Dataset Capture could not enter Running state."), false;

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
    createdAt_ = now;
    startedAt_ = now;
    activeStartedNs_ = monotonicNow_();
    acceptingOffers_ = true;
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
    QString framePath;
    qint64 frameIndex = 0;
    {
        std::lock_guard lock(mutex_);
        if (formatFixed_ && (width_ != image.width() || height_ != image.height() ||
                             bitDepth_ != meta.bits || fps_ != fps)) {
            error_ = "Dataset Capture frame format changed.";
            throw std::runtime_error("format changed");
        }
        frameIndex = savedFrameCount_ + 1;
        framePath = QDir(sequenceFolder_)
                        .filePath(QString("frame_%1.tif").arg(frameIndex, 8, 10, QLatin1Char('0')));
    }
    QString writeError;
    if (!writeImage(image, framePath, "TIFF", &writeError)) {
        std::lock_guard lock(mutex_);
        error_ = writeError;
        qWarning().noquote() << "Dataset Capture consumer failure handoff"
                             << handoffId << "-" << handoffId << ":" << writeError;
        throw std::runtime_error("frame write failed");
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
    const DropletDetectionFrame detection = detector_.processFrame(frame);
    std::optional<DatasetRecord> record;
    if (detection.eventEntered) {
        desktop_app::DatasetCrop crop;
        QString cropError;
        if (!desktop_app::CropService::makeDatasetCrop(frame, detection.bbox, &crop,
                                                       &cropError)) {
            std::lock_guard lock(mutex_);
            error_ = cropError;
            throw std::runtime_error("crop failed");
        }
        qint64 cropIndex = 0;
        {
            std::lock_guard lock(mutex_);
            cropIndex = records_.size() + 1;
        }
        const QString eventId = QString::number(cropIndex);
        const QString cropPath = QDir(cropsFolder_)
                                     .filePath(QString("crop_%1.png")
                                                   .arg(cropIndex, 8, 10,
                                                        QLatin1Char('0')));
        const QImage cropImage(crop.image.data, crop.image.cols, crop.image.rows,
                               crop.image.step,
                               QImage::Format_Grayscale8);
        if (!writeImage(cropImage, cropPath, "PNG", &cropError)) {
            std::lock_guard lock(mutex_);
            error_ = cropError;
            throw std::runtime_error("crop write failed");
        }
        record = DatasetRecord{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            "crops/" + QFileInfo(cropPath).fileName(),
            hashFile(cropPath),
            QString::number(meta.frameIndex),
            eventId,
            QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
            QRect(crop.sourceRect.x, crop.sourceRect.y, crop.sourceRect.width,
                  crop.sourceRect.height),
            frameIndex};
    }
    std::lock_guard lock(mutex_);
    if (record)
        records_.push_back(std::move(*record));
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
    if (!saveManifest("completed", reason, error))
        return failAndRelease("manifest_error", error ? *error : "Manifest save failed.", error);
    QFile::remove(partialPath_);
    std::lock_guard lock(mutex_);
    lease_.transition(OperationLifecycle::Completed);
    lease_.release();
    lifecycle_ = OperationLifecycle::Completed;
    return true;
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
    return DatasetManifestV2::save(QDir(folder_).filePath("dataset.json"), data, error);
}

bool DatasetCaptureService::failAndRelease(const QString& reason, const QString& message,
                                           QString* error) {
    dispatcher_.closeDatasetBoundary();
    dispatcher_.stopAndDrain();
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
    if (hasFrames)
        saveManifest("interrupted", reason, &recoveryError);
    else if (!partialPath_.isEmpty())
        desktop_app::writeJsonObjectAtomically(
            partialPath_, QJsonObject{{"schema_version", "opendss.dataset.partial.v1"},
                                      {"status", "failed"}, {"stop_reason", reason},
                                      {"error", message}, {"saved_frame_count", 0}},
            &recoveryError);
    const auto integrity = dispatcher_.datasetIntegrity();
    if (integrity.consumerFailureCount > 0) {
        QStringList ranges;
        for (const auto& range : integrity.consumerFailures)
            ranges.push_back(QString("%1-%2").arg(range.first).arg(range.last));
        qWarning().noquote() << "Dataset Capture aggregate consumer failures: count"
                             << integrity.consumerFailureCount << "ranges"
                             << ranges.join(',');
    }
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
