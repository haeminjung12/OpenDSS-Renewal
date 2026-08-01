#include "detection/droplet_detector_adapters.h"
#include "detection/droplet_frame_processor.h"
#include "v2/camera/camera_service.h"
#include "v2/camera/dcam_camera_device.h"
#include "v2/camera/frame_conversion.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/sequence/image_sequence_capture_service.h"
#include "v2/state/application_state_store.h"

#include <QBuffer>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <tiffio.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace desktop_app::v2;
using namespace desktop_app::v2::sequence;

namespace {

using Clock = std::chrono::steady_clock;

qint64 nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch())
        .count();
}

double nsToMs(qint64 value)
{
    return static_cast<double>(value) / 1'000'000.0;
}

struct Samples
{
    std::vector<qint64> values;

    void add(qint64 value) { values.push_back(value); }

    QJsonObject json() const
    {
        if (values.empty())
            return {{"count", 0}};
        std::vector<qint64> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        qint64 total = 0;
        for (const qint64 value : values)
            total += value;
        const auto percentile = [&](double fraction) {
            const auto index = static_cast<std::size_t>(
                std::ceil(fraction * static_cast<double>(sorted.size()))) - 1;
            return nsToMs(sorted[(std::min)(index, sorted.size() - 1)]);
        };
        return {
            {"count", static_cast<qint64>(values.size())},
            {"total_ms", nsToMs(total)},
            {"average_ms", nsToMs(total) / static_cast<double>(values.size())},
            {"p50_ms", percentile(0.50)},
            {"p95_ms", percentile(0.95)},
            {"p99_ms", percentile(0.99)},
            {"max_ms", percentile(1.00)},
        };
    }
};

struct WriterMetrics
{
    mutable std::mutex mutex;
    std::atomic_bool active{false};
    Samples total;
    Samples tempOpen;
    Samples encodeAndTempWrite;
    Samples encodeMemory;
    Samples tempFileWrite;
    Samples flush;
    Samples publishWriteThrough;
    Samples readabilityProbe;
    qint64 encodedBytes = 0;

    QJsonObject json(const QString &mode) const
    {
        std::lock_guard lock(mutex);
        return {
            {"mode", mode},
            {"successful_frames", static_cast<qint64>(total.values.size())},
            {"encoded_bytes", encodedBytes},
            {"total", total.json()},
            {"temp_open", tempOpen.json()},
            {"encode_and_temp_write", encodeAndTempWrite.json()},
            {"encode_memory", encodeMemory.json()},
            {"temp_file_write", tempFileWrite.json()},
            {"flush", flush.json()},
            {"publish_write_through", publishWriteThrough.json()},
            {"readability_probe", readabilityProbe.json()},
        };
    }
};

struct ActiveWriterGuard
{
    explicit ActiveWriterGuard(std::atomic_bool &active) : active_(active)
    {
        active_.store(true, std::memory_order_release);
    }
    ~ActiveWriterGuard() { active_.store(false, std::memory_order_release); }
    std::atomic_bool &active_;
};

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

bool publishWithoutReplace(const QString &temporaryPath, const QString &targetPath,
                           QString *detail)
{
    if (MoveFileExW(reinterpret_cast<LPCWSTR>(temporaryPath.utf16()),
                    reinterpret_cast<LPCWSTR>(targetPath.utf16()),
                    MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    if (detail)
        *detail = QStringLiteral("Windows error %1").arg(GetLastError());
    return false;
}

void appendLe16(QByteArray &bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray &bytes, quint32 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
    bytes.append(static_cast<char>((value >> 16) & 0xff));
    bytes.append(static_cast<char>((value >> 24) & 0xff));
}

void appendIfdEntry(QByteArray &bytes, quint16 tag, quint16 type,
                    quint32 count, quint32 value)
{
    appendLe16(bytes, tag);
    appendLe16(bytes, type);
    appendLe32(bytes, count);
    appendLe32(bytes, value);
}

QByteArray mono8TiffHeader(int width, int height)
{
    constexpr quint16 entryCount = 11;
    constexpr quint32 ifdOffset = 8;
    constexpr quint32 pixelOffset = ifdOffset + 2 + entryCount * 12 + 4;
    const quint32 pixelBytes = static_cast<quint32>(width)
        * static_cast<quint32>(height);

    QByteArray header;
    header.reserve(static_cast<qsizetype>(pixelOffset));
    header.append('I');
    header.append('I');
    appendLe16(header, 42);
    appendLe32(header, ifdOffset);
    appendLe16(header, entryCount);
    appendIfdEntry(header, 256, 4, 1, static_cast<quint32>(width));  // ImageWidth
    appendIfdEntry(header, 257, 4, 1, static_cast<quint32>(height)); // ImageLength
    appendIfdEntry(header, 258, 3, 1, 8);                            // BitsPerSample
    appendIfdEntry(header, 259, 3, 1, 1);                            // Compression: none
    appendIfdEntry(header, 262, 3, 1, 1);                            // BlackIsZero
    appendIfdEntry(header, 273, 4, 1, pixelOffset);                  // StripOffsets
    appendIfdEntry(header, 277, 3, 1, 1);                            // SamplesPerPixel
    appendIfdEntry(header, 278, 4, 1, static_cast<quint32>(height)); // RowsPerStrip
    appendIfdEntry(header, 279, 4, 1, pixelBytes);                   // StripByteCounts
    appendIfdEntry(header, 284, 3, 1, 1);                            // Chunky planar
    appendIfdEntry(header, 339, 3, 1, 1);                            // Unsigned integer
    appendLe32(header, 0);
    return header;
}

QImage preserveNativeCameraFrame(const CameraFrame &frame, QString *error)
{
    if (frame.pixelFormat == CameraPixelFormat::Mono8)
        return convertCameraFrame(frame, error);
    if (frame.pixelFormat != CameraPixelFormat::Mono16
        || frame.bitDepth < 9 || frame.bitDepth > 16
        || frame.width <= 0 || frame.height <= 0
        || frame.width > (std::numeric_limits<int>::max)() / 2) {
        setError(error, QStringLiteral("Native persistence received an invalid Mono16 frame."));
        return {};
    }
    const int activeRowBytes = frame.width * 2;
    if (frame.rowBytes < activeRowBytes
        || frame.bytes.size() < static_cast<qsizetype>(frame.rowBytes) * frame.height) {
        setError(error, QStringLiteral("Native persistence received incomplete Mono16 data."));
        return {};
    }
    QImage image(frame.width, frame.height, QImage::Format_Grayscale16);
    if (image.isNull()) {
        setError(error, QStringLiteral("Native Mono16 persistence image allocation failed."));
        return {};
    }
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(image.scanLine(row),
                    frame.bytes.constData() + static_cast<qsizetype>(row) * frame.rowBytes,
                    static_cast<size_t>(activeRowBytes));
    }
    setError(error, {});
    return image;
}

ImageSequenceCaptureService::FrameWriter diagnosticWriter(
    const QString &mode, WriterMetrics &metrics)
{
    return [mode, &metrics](const QImage &image, const QString &target,
                            QString *error) {
        ActiveWriterGuard active(metrics.active);
        const qint64 totalStart = nowNs();
        if (QFileInfo::exists(target)) {
            setError(error, QStringLiteral("A frame already exists at %1.")
                                .arg(QDir::toNativeSeparators(target)));
            return false;
        }

        QByteArray encoded;
        qint64 encodeNs = 0;
        if (mode == QStringLiteral("split")) {
            QBuffer buffer(&encoded);
            if (!buffer.open(QIODevice::WriteOnly)) {
                setError(error, QStringLiteral("Could not open the TIFF memory buffer."));
                return false;
            }
            QImageWriter writer(&buffer, "tiff");
            const qint64 start = nowNs();
            const bool wrote = writer.write(image);
            encodeNs = nowNs() - start;
            if (!wrote) {
                setError(error, QStringLiteral("Could not encode a TIFF frame: %1.")
                                    .arg(writer.errorString()));
                return false;
            }
        }

        qint64 tempOpenNs = 0;
        qint64 combinedNs = 0;
        qint64 fileWriteNs = 0;
        qint64 flushNs = 0;
        QString temporaryPath;
        {
            const QString temporaryTemplate =
                (mode == QStringLiteral("opencv") || mode == QStringLiteral("libtiff"))
                ? QStringLiteral(".frame-XXXXXX.tif")
                : QStringLiteral(".frame-XXXXXX.tmp");
            QTemporaryFile temporary(QDir(QFileInfo(target).absolutePath())
                                         .absoluteFilePath(temporaryTemplate));
            const qint64 openStart = nowNs();
            const bool opened = temporary.open();
            tempOpenNs = nowNs() - openStart;
            if (!opened) {
                setError(error, QStringLiteral("Could not create a temporary frame: %1.")
                                    .arg(temporary.errorString()));
                return false;
            }

            if (mode == QStringLiteral("libtiff")) {
                if ((image.format() != QImage::Format_Grayscale8
                     && image.format() != QImage::Format_Grayscale16)
                    || image.width() <= 0 || image.height() <= 0) {
                    setError(error, QStringLiteral(
                                        "libtiff requires a valid grayscale image."));
                    return false;
                }
                const int bytesPerSample = image.format() == QImage::Format_Grayscale16 ? 2 : 1;
                const qint64 activeRowBytes = static_cast<qint64>(image.width())
                    * bytesPerSample;
                if (image.bytesPerLine() != activeRowBytes) {
                    setError(error, QStringLiteral(
                                        "libtiff diagnostic requires contiguous image rows."));
                    return false;
                }
                temporaryPath = temporary.fileName();
                temporary.close();
                temporary.setAutoRemove(false);
                const qint64 start = nowNs();
                TIFF *tiff = TIFFOpen(QFile::encodeName(temporaryPath).constData(), "w");
                bool wrote = tiff != nullptr;
                if (wrote) {
                    wrote = TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH,
                                         static_cast<uint32_t>(image.width())) == 1
                        && TIFFSetField(tiff, TIFFTAG_IMAGELENGTH,
                                        static_cast<uint32_t>(image.height())) == 1
                        && TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE,
                                        static_cast<uint16_t>(bytesPerSample * 8)) == 1
                        && TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL,
                                        static_cast<uint16_t>(1)) == 1
                        && TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC,
                                        static_cast<uint16_t>(PHOTOMETRIC_MINISBLACK)) == 1
                        && TIFFSetField(tiff, TIFFTAG_PLANARCONFIG,
                                        static_cast<uint16_t>(PLANARCONFIG_CONTIG)) == 1
                        && TIFFSetField(tiff, TIFFTAG_COMPRESSION,
                                        static_cast<uint16_t>(COMPRESSION_NONE)) == 1
                        && TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT,
                                        static_cast<uint16_t>(SAMPLEFORMAT_UINT)) == 1
                        && TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP,
                                        static_cast<uint32_t>(image.height())) == 1;
                    const tmsize_t pixelBytes = static_cast<tmsize_t>(activeRowBytes)
                        * image.height();
                    if (wrote) {
                        wrote = TIFFWriteEncodedStrip(
                                    tiff, 0,
                                    const_cast<uchar *>(image.constBits()), pixelBytes)
                            == pixelBytes;
                    }
                    TIFFClose(tiff);
                }
                combinedNs = nowNs() - start;
                if (!wrote) {
                    setError(error, QStringLiteral("libtiff could not write the TIFF frame."));
                    return false;
                }
            } else if (mode == QStringLiteral("opencv")) {
                if ((image.format() != QImage::Format_Grayscale8
                     && image.format() != QImage::Format_Grayscale16)
                    || image.width() <= 0 || image.height() <= 0) {
                    setError(error, QStringLiteral(
                                        "OpenCV TIFF requires a valid grayscale image."));
                    return false;
                }
                temporaryPath = temporary.fileName();
                temporary.close();
                temporary.setAutoRemove(false);
                const int cvType = image.format() == QImage::Format_Grayscale16
                    ? CV_16UC1 : CV_8UC1;
                cv::Mat matrix(image.height(), image.width(), cvType,
                               const_cast<uchar *>(image.constBits()),
                               static_cast<size_t>(image.bytesPerLine()));
                const qint64 start = nowNs();
                bool wrote = false;
                try {
                    wrote = cv::imwrite(
                        QFile::encodeName(temporaryPath).toStdString(), matrix,
                        {cv::IMWRITE_TIFF_COMPRESSION,
                         cv::IMWRITE_TIFF_COMPRESSION_NONE});
                } catch (const cv::Exception &exception) {
                    setError(error, QStringLiteral("OpenCV TIFF write failed: %1")
                                        .arg(QString::fromUtf8(exception.what())));
                    return false;
                }
                combinedNs = nowNs() - start;
                if (!wrote) {
                    setError(error, QStringLiteral("OpenCV could not write the TIFF frame."));
                    return false;
                }
            } else if (mode == QStringLiteral("direct")) {
                if (image.format() != QImage::Format_Grayscale8
                    || image.width() <= 0 || image.height() <= 0
                    || image.bytesPerLine() < image.width()) {
                    setError(error, QStringLiteral(
                                        "Direct TIFF requires a valid Grayscale8 image."));
                    return false;
                }
                const QByteArray header = mono8TiffHeader(image.width(), image.height());
                const qint64 start = nowNs();
                bool complete = temporary.write(header) == header.size();
                if (complete && image.bytesPerLine() == image.width()) {
                    const qint64 pixelBytes = static_cast<qint64>(image.width())
                        * static_cast<qint64>(image.height());
                    complete = temporary.write(
                                   reinterpret_cast<const char *>(image.constBits()),
                                   pixelBytes)
                        == pixelBytes;
                } else if (complete) {
                    for (int row = 0; row < image.height() && complete; ++row) {
                        complete = temporary.write(
                                       reinterpret_cast<const char *>(image.constScanLine(row)),
                                       image.width())
                            == image.width();
                    }
                }
                fileWriteNs = nowNs() - start;
                if (!complete) {
                    setError(error, QStringLiteral(
                                        "Could not write the complete direct TIFF."));
                    return false;
                }
            } else if (mode != QStringLiteral("split")) {
                QImageWriter writer(&temporary, "tiff");
                const qint64 start = nowNs();
                const bool wrote = writer.write(image);
                combinedNs = nowNs() - start;
                if (!wrote) {
                    setError(error, QStringLiteral("Could not write a TIFF frame: %1.")
                                        .arg(writer.errorString()));
                    return false;
                }
            } else {
                const qint64 start = nowNs();
                const qint64 written = temporary.write(encoded);
                fileWriteNs = nowNs() - start;
                if (written != encoded.size()) {
                    setError(error, QStringLiteral("Could not write the complete encoded TIFF."));
                    return false;
                }
            }

            if (mode != QStringLiteral("opencv") && mode != QStringLiteral("libtiff")) {
                const qint64 flushStart = nowNs();
                const bool flushed = temporary.flush();
                flushNs = nowNs() - flushStart;
                if (!flushed) {
                    setError(error, QStringLiteral("Could not flush the temporary TIFF frame."));
                    return false;
                }
                temporaryPath = temporary.fileName();
                temporary.close();
                temporary.setAutoRemove(false);
            }
        }

        QString detail;
        const qint64 publishStart = nowNs();
        const bool published = publishWithoutReplace(temporaryPath, target, &detail);
        const qint64 publishNs = nowNs() - publishStart;
        if (!published) {
            QFile::remove(temporaryPath);
            setError(error,
                     QStringLiteral("Could not publish the TIFF frame without replacement: %1.")
                         .arg(detail));
            return false;
        }

        qint64 probeNs = 0;
        if (mode == QStringLiteral("exact") || mode == QStringLiteral("split")) {
            const qint64 probeStart = nowNs();
            const bool readable = QImageReader(target).canRead();
            probeNs = nowNs() - probeStart;
            if (!readable) {
                setError(error, QStringLiteral("The published TIFF frame is not readable."));
                return false;
            }
        }

        const qint64 totalNs = nowNs() - totalStart;
        const qint64 bytes = mode == QStringLiteral("split")
            ? encoded.size()
            : QFileInfo(target).size();
        {
            std::lock_guard lock(metrics.mutex);
            metrics.total.add(totalNs);
            metrics.tempOpen.add(tempOpenNs);
            metrics.flush.add(flushNs);
            metrics.publishWriteThrough.add(publishNs);
            if (mode != QStringLiteral("no-probe"))
                metrics.readabilityProbe.add(probeNs);
            metrics.encodedBytes += bytes;
            if (mode == QStringLiteral("exact") || mode == QStringLiteral("no-probe")
                || mode == QStringLiteral("opencv") || mode == QStringLiteral("libtiff"))
                metrics.encodeAndTempWrite.add(combinedNs);
            else {
                if (mode == QStringLiteral("split"))
                    metrics.encodeMemory.add(encodeNs);
                metrics.tempFileWrite.add(fileWriteNs);
            }
        }
        return true;
    };
}

QJsonObject settingsJson(const CameraAppliedSettings &settings)
{
    return {
        {"width", settings.width},
        {"height", settings.height},
        {"bit_depth", settings.bitDepth},
        {"pixel_type", settings.pixelType == CameraPixelType::Mono8 ? "Mono8" : "Mono16"},
        {"exposure_ms", settings.exposureMs},
        {"readout_mode", settings.readoutMode == CameraReadoutMode::Fast ? "Fast" : "Slow"},
    };
}

bool sameSettings(const CameraAppliedSettings &left,
                  const CameraAppliedSettings &right)
{
    return left.width == right.width && left.height == right.height
        && left.bitDepth == right.bitDepth && left.pixelType == right.pixelType
        && std::abs(left.exposureMs - right.exposureMs) <= 0.01
        && left.readoutMode == right.readoutMode;
}

void writeOutput(const QJsonObject &output, const QString &reportPath,
                 QJsonArray &failures)
{
    const QByteArray json = QJsonDocument(output).toJson(QJsonDocument::Indented);
    if (!reportPath.isEmpty()) {
        QSaveFile report(reportPath);
        if (!report.open(QIODevice::WriteOnly)
            || report.write(json) != json.size() || !report.commit()) {
            failures.append(QStringLiteral("Could not write report %1.").arg(reportPath));
        }
    }
    QTextStream(stdout) << QJsonDocument(output).toJson(QJsonDocument::Compact)
                        << Qt::endl;
}

// Test-only append spool.  Records are self describing so the partial file can be
// audited or finalized without retaining capture frames in memory.
class ChunkSpool
{
public:
    struct Metrics {
        quint64 accepted = 0, persisted = 0, rejected = 0, failures = 0, chunks = 0;
        qint64 bytes = 0, peakBufferedBytes = 0;
        std::size_t queueHighWater = 0, poolHighWater = 0;
        Samples writes;
        QJsonObject json(double elapsed) const {
            return {{"accepted_frame_count", static_cast<qint64>(accepted)},
                    {"persisted_frame_count", static_cast<qint64>(persisted)},
                    {"rejected_frame_count", static_cast<qint64>(rejected)},
                    {"failure_count", static_cast<qint64>(failures)},
                    {"chunk_count", static_cast<qint64>(chunks)}, {"sequential_bytes", bytes},
                    {"sequential_bytes_per_second", elapsed > 0 ? bytes / elapsed : 0.0},
                    {"peak_buffered_bytes", peakBufferedBytes},
                    {"chunk_queue_high_water", static_cast<qint64>(queueHighWater)},
                    {"chunk_pool_high_water", static_cast<qint64>(poolHighWater)},
                    {"chunk_write_latency", writes.json()}};
        }
    };

    ChunkSpool(const QString &path, qint64 chunkBytes, int flushMs)
        : path_(path), chunkBytes_(chunkBytes), flushMs_(flushMs) {}
    ~ChunkSpool() { stop(); }

    bool start(QString *error) {
        file_.setFileName(path_);
        if (!file_.open(QIODevice::WriteOnly | QIODevice::Truncate)) { setError(error, file_.errorString()); return false; }
        for (int i = 0; i != 4; ++i) free_.push_back(QByteArray()); // bounded reusable pool
        worker_ = std::thread([this] { run(); });
        return true;
    }
    bool offer(const CameraFrame &frame, QString *error) {
        QString conversionError;
        const QImage image = convertCameraFrame(frame, &conversionError); // always produces 8-bit
        if (image.isNull()) { reject(error, conversionError); return false; }
        const qint64 pixels = static_cast<qint64>(image.width()) * image.height();
        const qint64 recordBytes = 32 + pixels;
        if (recordBytes > chunkBytes_) { reject(error, QStringLiteral("Frame exceeds chunk byte threshold.")); return false; }
        std::lock_guard lock(mutex_);
        if (stopping_ || (free_.empty() && (current_.isEmpty() || current_.size() + recordBytes > chunkBytes_))) {
            rejectLocked(error, QStringLiteral("Bounded chunk pool exhausted.")); return false;
        }
        if (current_.isEmpty()) { current_ = std::move(free_.front()); free_.pop_front(); current_.clear(); }
        if (current_.size() + recordBytes > chunkBytes_) { ready_.push_back(std::move(current_)); current_.clear(); cv_.notify_one(); current_ = std::move(free_.front()); free_.pop_front(); current_.clear(); }
        appendLe32(current_, 0x5344504f); // OPDS
        appendLe32(current_, 1); appendLe32(current_, static_cast<quint32>(frame.deliveryId));
        appendLe32(current_, static_cast<quint32>(image.width())); appendLe32(current_, static_cast<quint32>(image.height()));
        appendLe32(current_, static_cast<quint32>(pixels)); appendLe32(current_, 8); appendLe32(current_, 0);
        for (int y = 0; y < image.height(); ++y) current_.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width());
        ++metrics_.accepted; buffered_ += recordBytes; metrics_.peakBufferedBytes = (std::max)(metrics_.peakBufferedBytes, buffered_);
        metrics_.queueHighWater = (std::max)(metrics_.queueHighWater, ready_.size()); metrics_.poolHighWater = (std::max)(metrics_.poolHighWater, static_cast<std::size_t>(4 - free_.size()));
        return true;
    }
    bool stop(QString *error = nullptr) {
        { std::lock_guard lock(mutex_); if (stopped_) return metrics_.failures == 0; stopping_ = true; if (!current_.isEmpty()) ready_.push_back(std::move(current_)); }
        cv_.notify_one(); if (worker_.joinable()) worker_.join(); stopped_ = true;
        if (file_.isOpen()) { if (!file_.flush()) ++metrics_.failures; file_.close(); }
        if (metrics_.failures) setError(error, QStringLiteral("Spool write failed."));
        return metrics_.failures == 0;
    }
    Metrics metrics() const { std::lock_guard lock(mutex_); return metrics_; }
private:
    void reject(QString *error, const QString &message) { std::lock_guard lock(mutex_); rejectLocked(error, message); }
    void rejectLocked(QString *error, const QString &message) { ++metrics_.rejected; setError(error, message); }
    void run() {
        for (;;) { QByteArray chunk; { std::unique_lock lock(mutex_); cv_.wait_for(lock, std::chrono::milliseconds(flushMs_), [&] { return stopping_ || !ready_.empty(); }); if (ready_.empty() && !current_.isEmpty()) { ready_.push_back(std::move(current_)); current_.clear(); } if (!ready_.empty()) { chunk = std::move(ready_.front()); ready_.pop_front(); } else if (stopping_) break; else continue; }
            const qint64 started = nowNs(); const qint64 written = file_.write(chunk); const bool flushed = file_.flush(); const qint64 elapsed = nowNs() - started;
            std::lock_guard lock(mutex_); metrics_.writes.add(elapsed); ++metrics_.chunks;
            if (written != chunk.size() || !flushed) ++metrics_.failures; else { metrics_.bytes += written; }
            // Count records cheaply from the fixed header/payload layout while recycling the buffer.
            int offset = 0; quint64 records = 0; while (offset + 32 <= chunk.size()) { const uchar *p = reinterpret_cast<const uchar *>(chunk.constData() + offset); const quint32 n = quint32(p[20]) | quint32(p[21]) << 8 | quint32(p[22]) << 16 | quint32(p[23]) << 24; if (n > static_cast<quint32>(chunk.size() - offset - 32)) break; ++records; offset += 32 + static_cast<int>(n); }
            if (written == chunk.size() && flushed) metrics_.persisted += records;
            buffered_ -= chunk.size(); chunk.clear(); free_.push_back(std::move(chunk)); if (stopping_ && ready_.empty()) cv_.notify_one();
        }
    }
    QString path_; qint64 chunkBytes_; int flushMs_; QFile file_; mutable std::mutex mutex_; std::condition_variable cv_; std::deque<QByteArray> free_, ready_; QByteArray current_; std::thread worker_; bool stopping_ = false, stopped_ = false; qint64 buffered_ = 0; Metrics metrics_;
};

quint32 readLe32(const char *bytes)
{
    const auto *p = reinterpret_cast<const uchar *>(bytes);
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

QJsonObject finalizeChunkSpool(const QString &spoolPath, QString *failure)
{
    const qint64 started = nowNs();
    QFile spool(spoolPath);
    QJsonObject result{{"attempted", true}, {"source", spoolPath}};
    quint64 records = 0, orderFaults = 0, headerFaults = 0, written = 0, readable = 0;
    Samples writeTimes;
    if (!spool.open(QIODevice::ReadOnly)) {
        setError(failure, QStringLiteral("Could not open spool for finalization: %1").arg(spool.errorString()));
        result.insert("failure", *failure); return result;
    }
    const QString finalRoot = spoolPath + QStringLiteral(".finalized");
    if (!QDir().mkpath(finalRoot)) {
        setError(failure, QStringLiteral("Could not create diagnostic finalization folder."));
        result.insert("failure", *failure); return result;
    }
    quint64 previousDelivery = 0;
    WriterMetrics finalWriterMetrics;
    const ImageSequenceCaptureService::FrameWriter finalWriter =
        diagnosticWriter(QStringLiteral("no-probe"), finalWriterMetrics);
    for (;;) {
        const QByteArray header = spool.read(32);
        if (header.isEmpty() && spool.atEnd()) break;
        if (header.size() != 32) { ++headerFaults; break; }
        const quint32 magic = readLe32(header.constData());
        const quint32 version = readLe32(header.constData() + 4);
        const quint32 delivery = readLe32(header.constData() + 8);
        const quint32 width = readLe32(header.constData() + 12);
        const quint32 height = readLe32(header.constData() + 16);
        const quint32 bytes = readLe32(header.constData() + 20);
        const quint32 bits = readLe32(header.constData() + 24);
        if (magic != 0x5344504f || version != 1 || bits != 8 || width == 0 || height == 0
            || bytes != width * height) { ++headerFaults; break; }
        if (records != 0 && delivery != previousDelivery + 1) ++orderFaults;
        previousDelivery = delivery;
        const QByteArray pixels = spool.read(bytes);
        if (pixels.size() != static_cast<qsizetype>(bytes)) { ++headerFaults; break; }
        ++records;
        QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_Grayscale8);
        if (image.isNull()) { ++headerFaults; break; }
        for (quint32 row = 0; row < height; ++row)
            std::memcpy(image.scanLine(static_cast<int>(row)), pixels.constData() + row * width, width);
        const QString target = QDir(finalRoot).absoluteFilePath(
            QStringLiteral("frame_%1.tif").arg(delivery, 10, 10, QLatin1Char('0')));
        const qint64 writeStart = nowNs();
        QString writeError;
        const bool complete = finalWriter(image, target, &writeError);
        writeTimes.add(nowNs() - writeStart);
        if (!complete) { ++headerFaults; continue; }
        ++written;
        QImageReader reader(target);
        const QImage readImage = reader.read();
        if (!readImage.isNull() && readImage.width() == static_cast<int>(width)
            && readImage.height() == static_cast<int>(height)) ++readable;
    }
    const qint64 elapsed = nowNs() - started;
    result.insert("diagnostic_folder", finalRoot);
    result.insert("record_count", static_cast<qint64>(records));
    result.insert("order_fault_count", static_cast<qint64>(orderFaults));
    result.insert("header_fault_count", static_cast<qint64>(headerFaults));
    result.insert("written_count", static_cast<qint64>(written));
    result.insert("readable_count", static_cast<qint64>(readable));
    result.insert("duration_seconds", static_cast<double>(elapsed) / 1e9);
    result.insert("frames_per_second", elapsed > 0 ? static_cast<double>(written) * 1e9 / elapsed : 0.0);
    result.insert("writer_timing", writeTimes.json());
    result.insert("writer", finalWriterMetrics.json(QStringLiteral("no-probe")));
    if (headerFaults || orderFaults || written != records || readable != records)
        setError(failure, QStringLiteral("Chunk spool finalization validation failed."));
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("opendss_dbg019_sequence_persistence_headless");

    QCommandLineParser parser;
    parser.setApplicationDescription("DBG-020 real-DCAM bounded chunk-spool characterization.");
    parser.addHelpOption();
    QCommandLineOption durationOption({"d", "duration"},
                                      "Capture duration in seconds.", "seconds", "3");
    QCommandLineOption outputOption({"o", "output-root"},
                                    "Existing writable Image Sequence root.", "path");
    QCommandLineOption writerOption({"w", "writer"},
                                    "Writer mode: production, exact, split, no-probe, direct, opencv, libtiff, or chunk-spool.",
                                    "mode", "exact");
    QCommandLineOption bitDepthOption({"b", "bit-depth"},
                                      "Camera/persistence bit depth: 8 or 16.",
                                      "bits", "8");
    QCommandLineOption reportOption({"r", "report"},
                                    "Optional JSON report path.", "path");
    QCommandLineOption widthOption("roi-width", "Requested ROI width (144 or 2304 diagnostic profiles supported).", "pixels", "2304");
    QCommandLineOption heightOption("roi-height", "Requested ROI height (144 or 2304 diagnostic profiles supported).", "pixels", "2304");
    QCommandLineOption chunkBytesOption("chunk-bytes", "Append spool chunk byte threshold.", "bytes", "33554432");
    QCommandLineOption flushMsOption("flush-ms", "Append spool flush interval threshold.", "milliseconds", "50");
    QCommandLineOption exposureOption("exposure-ms", "Requested camera exposure in milliseconds.", "milliseconds", "1.0");
    QCommandLineOption finalizeOption("finalize-spool", "After stop, stream the diagnostic spool into separate 8-bit TIFF files.");
    parser.addOption(durationOption);
    parser.addOption(outputOption);
    parser.addOption(writerOption);
    parser.addOption(bitDepthOption);
    parser.addOption(reportOption);
    parser.addOption(widthOption); parser.addOption(heightOption);
    parser.addOption(chunkBytesOption); parser.addOption(flushMsOption);
    parser.addOption(exposureOption);
    parser.addOption(finalizeOption);
    parser.process(app);

    bool durationOk = false;
    const double durationSeconds = parser.value(durationOption).toDouble(&durationOk);
    const QString outputRoot = QFileInfo(parser.value(outputOption)).absoluteFilePath();
    const QString writerMode = parser.value(writerOption).trimmed().toLower();
    bool bitDepthOk = false;
    const int requestedBitDepth = parser.value(bitDepthOption).toInt(&bitDepthOk);
    const QString reportPath = parser.value(reportOption).trimmed();
    bool widthOk = false, heightOk = false, chunkBytesOk = false, flushMsOk = false;
    bool exposureOk = false;
    const int roiWidth = parser.value(widthOption).toInt(&widthOk);
    const int roiHeight = parser.value(heightOption).toInt(&heightOk);
    const qint64 chunkBytes = parser.value(chunkBytesOption).toLongLong(&chunkBytesOk);
    const int flushMs = parser.value(flushMsOption).toInt(&flushMsOk);
    const double exposureMs = parser.value(exposureOption).toDouble(&exposureOk);
    const bool finalizeSpool = parser.isSet(finalizeOption);

    QJsonArray failures;
    auto fail = [&](const QString &message) { failures.append(message); };
    if (!durationOk || !std::isfinite(durationSeconds)
        || durationSeconds <= 0.0 || durationSeconds > 30.0) {
        fail("Duration must be greater than 0 and no more than 30 seconds.");
    }
    if (writerMode != QStringLiteral("production")
        && writerMode != QStringLiteral("exact")
        && writerMode != QStringLiteral("split")
        && writerMode != QStringLiteral("no-probe")
        && writerMode != QStringLiteral("direct")
        && writerMode != QStringLiteral("opencv")
        && writerMode != QStringLiteral("libtiff") && writerMode != QStringLiteral("chunk-spool")) {
        fail("Writer mode must be production, exact, split, no-probe, direct, opencv, libtiff, or chunk-spool.");
    }
    if (!bitDepthOk || (requestedBitDepth != 8 && requestedBitDepth != 16))
        fail("Bit depth must be 8 or 16.");
    if (writerMode == QStringLiteral("direct") && requestedBitDepth != 8)
        fail("The hand-written direct diagnostic supports only 8-bit TIFF.");
    if (!QFileInfo(outputRoot).isDir() || !QFileInfo(outputRoot).isWritable())
        fail("Output root must be an existing writable directory.");
    if (!widthOk || !heightOk || roiWidth <= 0 || roiHeight <= 0)
        fail("ROI dimensions must be positive integers.");
    if (!chunkBytesOk || chunkBytes < 1024 * 1024 || !flushMsOk || flushMs <= 0 || flushMs > 1000)
        fail("Chunk bytes must be at least 1 MiB and flush-ms must be 1..1000.");
    if (!exposureOk || !std::isfinite(exposureMs) || exposureMs <= 0.0)
        fail("Exposure must be finite and greater than zero milliseconds.");
    if (finalizeSpool && writerMode != QStringLiteral("chunk-spool"))
        fail("--finalize-spool requires --writer chunk-spool.");

    QJsonObject output{
        {"program", "opendss_dbg019_sequence_persistence_headless"},
        {"bug", writerMode == QStringLiteral("chunk-spool") ? "DBG-020" : "DBG-019"},
        {"source", "REAL_DCAM_CURRENT_CAMERA_SERVICE"},
        {"device", "DCAM:0"},
        {"writer_mode", writerMode},
        {"requested_bit_depth", requestedBitDepth},
        {"persistence_bit_depth", 8}, {"roi_width", roiWidth}, {"roi_height", roiHeight},
        {"chunk_bytes", chunkBytes}, {"flush_ms", flushMs},
        {"requested_exposure_ms", exposureMs},
        {"finalize_spool_requested", finalizeSpool},
        {"capture_duration_requested_seconds", durationSeconds},
        {"output_root", outputRoot},
        {"pass", false},
    };
    if (!failures.isEmpty()) {
        output.insert("failures", failures);
        writeOutput(output, reportPath, failures);
        return 2;
    }

    ApplicationStateStore stateStore;
    QThread cameraThread;
    auto *camera = new CameraService(std::make_unique<DcamCameraDevice>(), stateStore);
    camera->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished,
                     camera, &QObject::deleteLater);
    cameraThread.start();
    OperationCoordinator operations;
    WriterMetrics writerMetrics;
    FastEventConfig detectorConfig{};
    FastEventDetectorAdapter detector(detectorConfig);
    detector.reset();
    DropletFrameProcessor processor(detector);

    bool commandSuccess = false;
    QString commandError;
    QObject::connect(camera, &CameraService::commandFinished, &app,
                     [&](bool success, const QString &error) {
                         commandSuccess = success;
                         commandError = error;
                     }, Qt::DirectConnection);
    QObject::connect(camera, &CameraService::frameError, &app,
                     [&](const QString &error) { fail(error); }, Qt::QueuedConnection);

    const auto invokeCamera = [&](std::function<void()> command) {
        commandSuccess = false;
        commandError.clear();
        return QMetaObject::invokeMethod(camera, std::move(command),
                                         Qt::BlockingQueuedConnection);
    };
    const auto shutdownCameraThread = [&] {
        cameraThread.quit();
        cameraThread.wait();
    };

    if (!invokeCamera([&] { camera->open(); }) || !commandSuccess) {
        fail(QStringLiteral("DCAM open failed: %1").arg(commandError));
        output.insert("failures", failures);
        shutdownCameraThread();
        writeOutput(output, reportPath, failures);
        return 3;
    }
    const CameraAppliedSettings originalSettings = camera->state().appliedSettings;
    output.insert("profile_before", settingsJson(originalSettings));

    CameraAppliedSettings requested = originalSettings;
    requested.width = roiWidth;
    requested.height = roiHeight;
    requested.bitDepth = requestedBitDepth;
    requested.pixelType = requestedBitDepth == 8
        ? CameraPixelType::Mono8 : CameraPixelType::Mono16;
    requested.exposureMs = exposureMs;
    requested.readoutMode = CameraReadoutMode::Fast;
    invokeCamera([&] { camera->applyConfiguration(requested); });
    const CameraAppliedSettings appliedSettings = camera->state().appliedSettings;
    output.insert("profile_applied", settingsJson(appliedSettings));
    if (!commandSuccess || !sameSettings(requested, appliedSettings))
        fail(QStringLiteral("ROI %1x%2 %3-bit Fast profile application failed: %4")
                 .arg(roiWidth).arg(roiHeight).arg(requestedBitDepth).arg(commandError));

    invokeCamera([&] { camera->start(); });
    if (!commandSuccess)
        fail(QStringLiteral("DCAM start failed: %1").arg(commandError));

    ImageSequenceCaptureService::FrameConverter persistenceConverter;
    if (requestedBitDepth == 16 && writerMode != QStringLiteral("production"))
        persistenceConverter = preserveNativeCameraFrame;
    ImageSequenceCaptureService::FrameWriter persistenceWriter;
    if (writerMode != QStringLiteral("production"))
        persistenceWriter = diagnosticWriter(writerMode, writerMetrics);
    ImageSequenceCaptureService sequence(
        *camera, operations, processor, nowNs, persistenceConverter,
        persistenceWriter);
    const QString spoolPath = QDir(outputRoot).absoluteFilePath(
        QStringLiteral("DBG-020-%1.partial").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
    std::optional<ChunkSpool> spool;
    if (writerMode == QStringLiteral("chunk-spool")) {
        spool.emplace(spoolPath, chunkBytes, flushMs);
        QString spoolError;
        if (!spool->start(&spoolError))
            fail(QStringLiteral("Chunk spool start failed: %1").arg(spoolError));
    }
    ImageSequenceCaptureRequest request;
    request.saveRoot = outputRoot;
    request.name = QStringLiteral("DBG-019-%1-%2")
                       .arg(writerMode,
                            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    request.experimentType = QStringLiteral("DBG-019 characterization");
    request.notes = writerMode == QStringLiteral("production")
        ? QStringLiteral("Production-default Image Sequence writer after authorized fix.")
        : writerMode == QStringLiteral("exact")
        ? QStringLiteral("Exact production write sequence with diagnostic timing.")
        : writerMode == QStringLiteral("split")
            ? QStringLiteral("Diagnostic memory-encode split for independent encode and disk timing.")
            : writerMode == QStringLiteral("no-probe")
                ? QStringLiteral("Exact write sequence with only the post-publication readability probe omitted.")
                : writerMode == QStringLiteral("direct")
                    ? QStringLiteral("Direct uncompressed Mono8 TIFF write with no hot-path readback probe.")
                    : writerMode == QStringLiteral("opencv")
                        ? QStringLiteral("OpenCV uncompressed native-depth TIFF write with no hot-path readback probe.")
                        : QStringLiteral("Direct libtiff uncompressed native-depth write with no hot-path readback probe.");
    request.opendssVersion = QStringLiteral("DBG-019-headless");
    request.cameraSettings = settingsJson(appliedSettings);
    QString sequenceError;
    if (failures.isEmpty() && writerMode != QStringLiteral("chunk-spool")
        && !sequence.start(request, &sequenceError))
        fail(QStringLiteral("Image Sequence start failed: %1").arg(sequenceError));

    quint64 acquired = 0;
    quint64 detectorCompleted = 0;
    quint64 detectorOrderingFaults = 0;
    quint64 expectedDetectorDelivery = 0;
    quint64 offerAccepted = 0;
    quint64 offerRejected = 0;
    quint64 sourceGaps = 0;
    quint64 duplicates = 0;
    quint64 outOfOrder = 0;
    quint64 firstDelivery = 0;
    quint64 lastDelivery = 0;
    qint64 firstSourceTimestampNs = 0;
    qint64 lastSourceTimestampNs = 0;
    qint64 firstDetectorStartNs = 0;
    qint64 lastDetectorCompleteNs = 0;
    qint64 totalDetectorNs = 0;
    std::size_t maxDetectorQueueDepth = 0;
    qint64 maxQueueDepth = 0;
    std::map<qint64, qint64> queueDepthHistogram;
    bool acceptingFrames = failures.isEmpty();

    std::mutex detectorMutex;
    std::condition_variable detectorReady;
    std::deque<CameraFrame> detectorQueue;
    bool detectorInputDone = false;
    QString detectorFailure;
    std::thread detectorThread([&] {
        for (;;) {
            CameraFrame frame;
            {
                std::unique_lock lock(detectorMutex);
                detectorReady.wait(lock, [&] {
                    return detectorInputDone || !detectorQueue.empty();
                });
                if (detectorQueue.empty() && detectorInputDone)
                    break;
                frame = std::move(detectorQueue.front());
                detectorQueue.pop_front();
            }
            if (expectedDetectorDelivery != 0 && frame.deliveryId != expectedDetectorDelivery + 1)
                ++detectorOrderingFaults;
            expectedDetectorDelivery = frame.deliveryId;
            QString detectorConversionError;
            const QImage detectorImage = convertCameraFrame(frame, &detectorConversionError);
            if (detectorImage.isNull()) {
                detectorFailure = QStringLiteral("Detector conversion failed: %1")
                                      .arg(detectorConversionError);
                continue;
            }
            const qint64 detectorStart = nowNs();
            if (firstDetectorStartNs == 0)
                firstDetectorStartNs = detectorStart;
            cv::Mat image(detectorImage.height(), detectorImage.width(), CV_8UC1,
                          const_cast<uchar *>(detectorImage.constBits()),
                          static_cast<size_t>(detectorImage.bytesPerLine()));
            detector.processFrame(image);
            const qint64 detectorComplete = nowNs();
            totalDetectorNs += detectorComplete - detectorStart;
            lastDetectorCompleteNs = detectorComplete;
            ++detectorCompleted;
        }
    });

    QObject::connect(camera, &CameraService::frameReady, &app,
                     [&](CameraFrame frame) {
        if (!acceptingFrames)
            return;
        if (acquired == 0) {
            firstDelivery = frame.deliveryId;
            firstSourceTimestampNs = frame.monotonicTimestampNs;
        } else if (frame.deliveryId == lastDelivery) {
            ++duplicates;
        } else if (frame.deliveryId < lastDelivery) {
            ++outOfOrder;
        } else if (frame.deliveryId > lastDelivery + 1) {
            sourceGaps += frame.deliveryId - lastDelivery - 1;
        }
        lastDelivery = frame.deliveryId;
        lastSourceTimestampNs = frame.monotonicTimestampNs;
        ++acquired;

        {
            std::lock_guard lock(detectorMutex);
            detectorQueue.push_back(frame);
            maxDetectorQueueDepth = (std::max)(maxDetectorQueueDepth,
                                                detectorQueue.size());
        }
        detectorReady.notify_one();

        QString offerError;
        const bool accepted = spool ? spool->offer(frame, &offerError)
                                    : sequence.offerFrame(frame, 100.0, &offerError);
        if (accepted)
            ++offerAccepted;
        else
            ++offerRejected;

        const ImageSequenceCaptureSnapshot snapshot = sequence.snapshot();
        const qint64 outstanding = spool ? 0
            : static_cast<qint64>(offerAccepted) - snapshot.capturedFrameCount;
        qint64 queueDepth = outstanding;
        if (writerMetrics.active.load(std::memory_order_acquire) && queueDepth > 0)
            --queueDepth;
        queueDepth = (std::max)(qint64(0),
                                (std::min)(queueDepth,
                                           static_cast<qint64>(LiveFrameDispatcher::capacity())));
        if (!accepted)
            queueDepth = static_cast<qint64>(LiveFrameDispatcher::capacity());
        maxQueueDepth = (std::max)(maxQueueDepth, queueDepth);
        ++queueDepthHistogram[queueDepth];
    }, Qt::QueuedConnection);

    QEventLoop captureLoop;
    const qint64 captureStartNs = nowNs();
    invokeCamera([&] {
        QTimer::singleShot(static_cast<int>(std::ceil(durationSeconds * 1000.0)),
                           camera, [&] {
            camera->stop();
            QMetaObject::invokeMethod(&captureLoop, [&] {
                QTimer::singleShot(0, &captureLoop, &QEventLoop::quit);
            }, Qt::QueuedConnection);
        });
    });
    if (failures.isEmpty())
        captureLoop.exec();
    acceptingFrames = false;
    const qint64 captureStopNs = nowNs();

    {
        std::lock_guard lock(detectorMutex);
        detectorInputDone = true;
    }
    detectorReady.notify_one();

    const qint64 drainStartNs = nowNs();
    const bool sequenceStopped = spool ? spool->stop(&sequenceError) : sequence.stop(&sequenceError);
    const qint64 drainStopNs = nowNs();
    detectorThread.join();
    const ImageSequenceCaptureSnapshot snapshot = sequence.snapshot();
    if (!sequenceStopped && !spool && snapshot.integrity.queueRejections.count == 0)
        fail(QStringLiteral("Image Sequence stop failed without queue rejection: %1")
                 .arg(sequenceError));

    if (camera->state().status == CameraStatus::Streaming)
        invokeCamera([&] { camera->stop(); });
    invokeCamera([&] { camera->applyConfiguration(originalSettings); });
    const CameraAppliedSettings restoredSettings = camera->state().appliedSettings;
    output.insert("profile_restored", settingsJson(restoredSettings));
    if (!commandSuccess || !sameSettings(originalSettings, restoredSettings))
        fail(QStringLiteral("DCAM profile restoration failed: %1").arg(commandError));
    invokeCamera([&] { camera->close(); });
    if (!commandSuccess)
        fail(QStringLiteral("DCAM close failed: %1").arg(commandError));
    shutdownCameraThread();

    if (acquired == 0)
        fail("DCAM produced zero frames.");
    if (detectorCompleted != acquired)
        fail("Detector completion count differs from acquisition count.");
    if (detectorOrderingFaults != 0)
        fail("Detector completion ordering failed.");
    if (!detectorFailure.isEmpty())
        fail(detectorFailure);
    if (sourceGaps != 0 || duplicates != 0 || outOfOrder != 0)
        fail("Acquisition delivery integrity failed.");
    if (snapshot.integrity.consumerFailures.count != 0)
        fail("Image Sequence consumer failures were observed.");
    if (spool) {
        const ChunkSpool::Metrics spoolMetrics = spool->metrics();
        if (spoolMetrics.rejected != 0 || spoolMetrics.failures != 0
            || spoolMetrics.persisted != spoolMetrics.accepted)
            fail("Chunk spool rejection, write failure, or persisted-count mismatch.");
        output.insert("chunk_spool_path", spoolPath);
        output.insert("chunk_spool", spoolMetrics.json(static_cast<double>(drainStopNs - captureStartNs) / 1e9));
        if (finalizeSpool) {
            QString finalizationFailure;
            const QJsonObject finalization = finalizeChunkSpool(spoolPath, &finalizationFailure);
            output.insert("chunk_spool_finalization", finalization);
            if (!finalizationFailure.isEmpty())
                fail(finalizationFailure);
            else if (finalization.value("record_count").toInteger() != static_cast<qint64>(spoolMetrics.persisted))
                fail("Chunk spool finalization record count differs from persisted count.");
        }
    }

    const double sourceElapsedSeconds = lastSourceTimestampNs > firstSourceTimestampNs
        ? static_cast<double>(lastSourceTimestampNs - firstSourceTimestampNs) / 1e9
        : 0.0;
    const double detectorElapsedSeconds = lastDetectorCompleteNs > firstDetectorStartNs
        ? static_cast<double>(lastDetectorCompleteNs - firstDetectorStartNs) / 1e9
        : 0.0;
    const double captureWallSeconds = static_cast<double>(captureStopNs - captureStartNs) / 1e9;
    const double activeSeconds = snapshot.activeElapsedSeconds;

    QJsonObject histogram;
    for (const auto &[depth, count] : queueDepthHistogram)
        histogram.insert(QString::number(depth), count);

    output.insert("sequence_folder", snapshot.folder);
    output.insert("sequence_lifecycle", static_cast<int>(snapshot.lifecycle));
    output.insert("sequence_error", snapshot.error);
    output.insert("first_delivery_id", static_cast<qint64>(firstDelivery));
    output.insert("last_delivery_id", static_cast<qint64>(lastDelivery));
    output.insert("acquired_count", static_cast<qint64>(acquired));
    output.insert("acquisition_fps",
                  sourceElapsedSeconds > 0.0 && acquired > 1
                      ? static_cast<double>(acquired - 1) / sourceElapsedSeconds : 0.0);
    output.insert("detector_completed_count", static_cast<qint64>(detectorCompleted));
    output.insert("detector_completion_fps",
                  detectorElapsedSeconds > 0.0
                      ? static_cast<double>(detectorCompleted) / detectorElapsedSeconds : 0.0);
    output.insert("detector_service_fps",
                  totalDetectorNs > 0
                      ? static_cast<double>(detectorCompleted) * 1e9
                            / static_cast<double>(totalDetectorNs) : 0.0);
    output.insert("detector_queue_high_water",
                  static_cast<qint64>(maxDetectorQueueDepth));
    output.insert("detector_ordering_faults", static_cast<qint64>(detectorOrderingFaults));
    output.insert("source_frame_gaps", static_cast<qint64>(sourceGaps));
    output.insert("duplicates", static_cast<qint64>(duplicates));
    output.insert("out_of_order", static_cast<qint64>(outOfOrder));
    output.insert("offer_accepted_count", static_cast<qint64>(offerAccepted));
    output.insert("offer_rejected_count", static_cast<qint64>(offerRejected));
    output.insert("saved_frame_count", snapshot.savedFrameCount);
    output.insert("saved_fps_active_window",
                  activeSeconds > 0.0 ? snapshot.savedFrameCount / activeSeconds : 0.0);
    output.insert("saved_fps_source_window",
                  sourceElapsedSeconds > 0.0
                      ? snapshot.savedFrameCount / sourceElapsedSeconds : 0.0);
    output.insert("capture_wall_seconds", captureWallSeconds);
    output.insert("persistence_drain_seconds",
                  static_cast<double>(drainStopNs - drainStartNs) / 1e9);
    output.insert("save_queue_capacity",
                  static_cast<qint64>(LiveFrameDispatcher::capacity()));
    output.insert("save_queue_high_water", maxQueueDepth);
    output.insert("save_queue_depth_histogram", histogram);
    output.insert("manifest_queue_rejections", snapshot.integrity.queueRejections.count);
    output.insert("manifest_consumer_failures", snapshot.integrity.consumerFailures.count);
    output.insert("writer", writerMetrics.json(writerMode));
    output.insert("failures", failures);
    output.insert("pass", failures.isEmpty());
    writeOutput(output, reportPath, failures);
    return failures.isEmpty() ? 0 : 1;
}
