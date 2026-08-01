#include "frame_persistence_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QTemporaryFile>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
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

namespace desktop_app::v2::persistence {
namespace {

constexpr qint64 kSpoolChunkBytes = 32 * 1024 * 1024;
constexpr int kSpoolBufferCount = 4;
constexpr int kSpoolFlushMilliseconds = 50;
constexpr qsizetype kSpoolRecordHeaderBytes = 40;
constexpr quint32 kSpoolRecordMagic = 0x5353444f; // ODSS

void setError(QString* error, const QString& message) {
    if (error)
        *error = message;
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

class FramePersistenceService::Spool final {
  public:
    ~Spool() { stop(nullptr); }

    bool start(const QString& path, QString* error) {
        file_.setFileName(path);
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
        for (int row = 0; row != image.height(); ++row)
            current_.append(reinterpret_cast<const char*>(image.constScanLine(row)), image.width());
        ++currentFrames_;
        ++acceptedFrames_;
        bufferedBytes_ += recordBytes;
        peakBufferedBytes_ = (std::max)(peakBufferedBytes_, bufferedBytes_);
        poolHighWater_ = (std::max)(poolHighWater_, kSpoolBufferCount - int(free_.size()));
        return true;
    }

    bool stop(QString* error) {
        { std::lock_guard lock(mutex_); if (!stopping_) { stopping_ = true; queueCurrentLocked(); } }
        condition_.notify_one();
        if (worker_.joinable()) worker_.join();
        if (file_.isOpen()) { if (!file_.flush()) recordFailure(QStringLiteral("Could not flush the Image Sequence spool: %1.").arg(file_.errorString())); file_.close(); }
        std::lock_guard lock(mutex_);
        if (failureCount_ > 0) { setError(error, failureMessage_.isEmpty() ? QStringLiteral("Image Sequence spool writing failed.") : failureMessage_); return false; }
        return true;
    }

    Metrics metrics() const { std::lock_guard lock(mutex_); return {acceptedFrames_, persistedFrames_, rejectedFrames_, failureCount_, sequentialBytes_, peakBufferedBytes_, queueHighWater_, poolHighWater_}; }

  private:
    struct ReadyChunk { QByteArray bytes; qint64 frames = 0; };
    void queueCurrentLocked() { if (current_.isEmpty()) return; ready_.push_back({std::move(current_), currentFrames_}); current_.clear(); currentFrames_ = 0; queueHighWater_ = (std::max)(queueHighWater_, int(ready_.size())); condition_.notify_one(); }
    void recordFailure(const QString& message) { std::lock_guard lock(mutex_); ++failureCount_; if (failureMessage_.isEmpty()) failureMessage_ = message; }
    void writeLoop() {
        for (;;) {
            ReadyChunk chunk;
            { std::unique_lock lock(mutex_); condition_.wait_for(lock, std::chrono::milliseconds(kSpoolFlushMilliseconds), [this] { return stopping_ || !ready_.empty(); }); if (ready_.empty() && !current_.isEmpty()) queueCurrentLocked(); if (!ready_.empty()) { chunk = std::move(ready_.front()); ready_.pop_front(); } else if (stopping_) break; else continue; }
            const qint64 written = file_.write(chunk.bytes); const bool flushed = written == chunk.bytes.size() && file_.flush();
            { std::lock_guard lock(mutex_); if (!flushed) { ++failureCount_; if (failureMessage_.isEmpty()) failureMessage_ = QStringLiteral("Could not append the Image Sequence spool: %1.").arg(file_.errorString()); } else { persistedFrames_ += chunk.frames; sequentialBytes_ += written; } bufferedBytes_ -= chunk.bytes.size(); chunk.bytes.clear(); free_.push_back(std::move(chunk.bytes)); }
        }
    }
    QFile file_; mutable std::mutex mutex_; std::condition_variable condition_; std::deque<QByteArray> free_; std::deque<ReadyChunk> ready_; QByteArray current_; qint64 currentFrames_ = 0; std::thread worker_; bool stopping_ = false; qint64 acceptedFrames_ = 0; qint64 persistedFrames_ = 0; qint64 rejectedFrames_ = 0; qint64 failureCount_ = 0; qint64 sequentialBytes_ = 0; qint64 bufferedBytes_ = 0; qint64 peakBufferedBytes_ = 0; int queueHighWater_ = 0; int poolHighWater_ = 0; QString failureMessage_;
};

FramePersistenceService::FramePersistenceService() : spool_(std::make_unique<Spool>()) {}
FramePersistenceService::~FramePersistenceService() = default;
bool FramePersistenceService::start(const QString& path, QString* error) { path_ = path; return spool_->start(path, error); }
bool FramePersistenceService::append(const QImage& image, const FrameMeta& meta, std::uint64_t handoffId, QString* error) { return spool_->append(image, meta, handoffId, error); }
bool FramePersistenceService::stop(QString* error) { return spool_->stop(error); }
FramePersistenceService::Metrics FramePersistenceService::metrics() const { return spool_->metrics(); }

bool FramePersistenceService::writeTiffWithoutReplace(const QImage& image, const QString& target, QString* error) {
    if (QFileInfo::exists(target)) { setError(error, QStringLiteral("A frame already exists at %1.").arg(QDir::toNativeSeparators(target))); return false; }
    QString temporaryPath;
    { QTemporaryFile temporary(QDir(QFileInfo(target).absolutePath()).absoluteFilePath(QStringLiteral(".frame-XXXXXX.tmp"))); if (!temporary.open()) { setError(error, QStringLiteral("Could not create a temporary frame: %1.").arg(temporary.errorString())); return false; } QImageWriter writer(&temporary, "tiff"); if (!writer.write(image) || !temporary.flush()) { setError(error, QStringLiteral("Could not write a TIFF frame: %1.").arg(writer.errorString())); return false; } temporaryPath = temporary.fileName(); temporary.close(); temporary.setAutoRemove(false); }
    QString detail; if (!publishWithoutReplace(temporaryPath, target, &detail)) { QFile::remove(temporaryPath); setError(error, QStringLiteral("Could not publish the TIFF frame without replacement: %1.").arg(detail)); return false; } return true;
}

bool FramePersistenceService::finalize(const QString& framesFolder, qint64 totalFrames, int imageWidth, int imageHeight, const FrameWriter& frameWriter, qint64* savedFrameCount, qint64* failedOutputIndex, QString* error) const {
    setError(error, {}); if (savedFrameCount) *savedFrameCount = 0; if (failedOutputIndex) *failedOutputIndex = 0;
    QFile spool(path_); if (!spool.open(QIODevice::ReadOnly)) { setError(error, QStringLiteral("Could not open the Image Sequence spool for finalization: %1.").arg(spool.errorString())); return false; }
    quint64 previousHandoff = 0;
    for (qint64 outputIndex = 1; outputIndex <= totalFrames; ++outputIndex) {
        const QByteArray header = spool.read(kSpoolRecordHeaderBytes);
        const auto fail = [&](const QString& message) { if (failedOutputIndex) *failedOutputIndex = outputIndex; setError(error, message); return false; };
        if (header.size() != kSpoolRecordHeaderBytes) return fail(QStringLiteral("Image Sequence spool ended before frame %1.").arg(outputIndex));
        const quint32 magic = readLe32(header.constData()), version = readLe32(header.constData() + 4), width = readLe32(header.constData() + 24), height = readLe32(header.constData() + 28), payloadBytes = readLe32(header.constData() + 32), bits = readLe32(header.constData() + 36); const quint64 handoffId = readLe64(header.constData() + 8), sourceDelivery = readLe64(header.constData() + 16), expectedPayload = static_cast<quint64>(width) * height; const bool orderValid = outputIndex == 1 || handoffId == previousHandoff + 1;
        if (magic != kSpoolRecordMagic || version != 1 || bits != 8 || width == 0 || height == 0 || expectedPayload != payloadBytes || width != static_cast<quint32>(imageWidth) || height != static_cast<quint32>(imageHeight) || !orderValid || sourceDelivery > static_cast<quint64>((std::numeric_limits<qint64>::max)())) return fail(QStringLiteral("Image Sequence spool record %1 is invalid.").arg(outputIndex));
        previousHandoff = handoffId; const QByteArray pixels = spool.read(payloadBytes); if (pixels.size() != payloadBytes) return fail(QStringLiteral("Image Sequence spool frame %1 is truncated.").arg(outputIndex));
        QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_Grayscale8); if (image.isNull()) return fail(QStringLiteral("Could not allocate Image Sequence frame %1 during finalization.").arg(outputIndex));
        for (quint32 row = 0; row != height; ++row) std::memcpy(image.scanLine(static_cast<int>(row)), pixels.constData() + static_cast<qsizetype>(row) * width, width);
        const QString target = QDir(framesFolder).filePath(QStringLiteral("frame_%1.tif").arg(outputIndex, 8, 10, QLatin1Char('0'))); QString writeError; if (!frameWriter(image, target, &writeError)) return fail(writeError.isEmpty() ? QStringLiteral("Could not finalize Image Sequence frame %1.").arg(outputIndex) : writeError); if (savedFrameCount) ++*savedFrameCount;
    }
    if (!spool.atEnd()) { setError(error, QStringLiteral("Image Sequence spool contains unexpected trailing data.")); return false; }
    return true;
}

} // namespace desktop_app::v2::persistence
