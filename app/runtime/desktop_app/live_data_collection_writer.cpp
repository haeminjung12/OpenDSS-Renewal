#include "live_data_collection_writer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

namespace {

QString csvQuote(const QString& value) {
    QString out = value;
    out.replace("\"", "\"\"");
    return "\"" + out + "\"";
}

QString utcText(const QDateTime& value) {
    return value.toUTC().toString("yyyy-MM-ddTHH:mm:ss.zzz'Z'");
}

QString defaultCollectionsRoot() {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
        return QDir::home().filePath("Documents/OpenDSS/collections");
    return QDir(documents).filePath("OpenDSS/collections");
}

QString fieldForInt(int value, bool present) {
    return present ? QString::number(value) : QString();
}

QString fieldForDouble(double value, bool present, char format = 'f', int precision = 3) {
    return present ? QString::number(value, format, precision) : QString();
}

} // namespace

bool LiveDataCollectionWriter::start(const QString& collectionsRoot, std::string& err) {
    reset();
    const QString rootPath = collectionsRoot.trimmed().isEmpty() ? defaultCollectionsRoot() : collectionsRoot.trimmed();
    QDir root(rootPath);
    if (!root.mkpath(".")) {
        err = QString("Failed to create collection root: %1").arg(rootPath).toStdString();
        return false;
    }

    const QString stamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
    const QString suffix = QUuid::createUuid().toString(QUuid::Id128).left(8).toLower();
    sessionId_ = QString("collection_%1_%2").arg(stamp, suffix);
    sessionDir_ = root.filePath(sessionId_);
    streamDir_ = QDir(sessionDir_).filePath("stream");
    if (!root.mkpath(sessionId_) || !QDir(sessionDir_).mkpath("stream")) {
        err = QString("Failed to create collection session: %1").arg(sessionDir_).toStdString();
        reset();
        return false;
    }

    csvPath_ = QDir(sessionDir_).filePath("detections.csv");
    QFile csv(csvPath_);
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Text)) {
        err = QString("Failed to create detections CSV: %1").arg(csvPath_).toStdString();
        reset();
        return false;
    }
    QTextStream ts(&csv);
    ts << "image,event_detected,crop_id,timestamp_utc,frame_number,x,y,width,height,centroid_x,centroid_y,area,"
          "crop_raw_path,crop_64_path\n";
    ts.flush();
    csv.close();

    startedAtUtc_ = QDateTime::currentDateTimeUtc();
    active_ = true;
    if (!writeMetadata(QStringLiteral("active"), err)) {
        reset();
        return false;
    }
    return true;
}

bool LiveDataCollectionWriter::writeFrame(const QImage& image, const PipelineEvent& event, bool detectorProcessed,
                                          qint64 sourceFrameNumber, std::string& err) {
    if (!active_) {
        err = "Collection writer is not active";
        return false;
    }
    if (image.isNull()) {
        err = "Cannot write a null collection frame";
        return false;
    }

    const std::uint64_t nextIndex = framesSaved_ + 1;
    const QString fileName = QString("frame_%1.tiff").arg(static_cast<qulonglong>(nextIndex), 6, 10, QChar('0'));
    const QString relativePath = QString("stream/%1").arg(fileName);
    const QString imagePath = QDir(streamDir_).filePath(fileName);
    if (!image.save(imagePath, "TIFF")) {
        err = QString("Failed to write TIFF frame: %1").arg(imagePath).toStdString();
        return false;
    }

    const QDateTime timestampUtc = QDateTime::currentDateTimeUtc();
    if (!appendDetectionRow(relativePath, event, detectorProcessed, sourceFrameNumber, timestampUtc, err)) {
        return false;
    }
    framesSaved_ = nextIndex;
    rowsLogged_++;
    return true;
}

bool LiveDataCollectionWriter::finish(const QString& stopReason, std::string& err) {
    if (!active_)
        return true;
    stoppedAtUtc_ = QDateTime::currentDateTimeUtc();
    active_ = false;
    return writeMetadata(stopReason, err);
}

bool LiveDataCollectionWriter::isActive() const {
    return active_;
}

QString LiveDataCollectionWriter::sessionId() const {
    return sessionId_;
}

QString LiveDataCollectionWriter::sessionDir() const {
    return sessionDir_;
}

std::uint64_t LiveDataCollectionWriter::framesSaved() const {
    return framesSaved_;
}

std::uint64_t LiveDataCollectionWriter::rowsLogged() const {
    return rowsLogged_;
}

bool LiveDataCollectionWriter::writeMetadata(const QString& stopReason, std::string& err) const {
    if (sessionDir_.isEmpty())
        return true;
    QJsonObject root;
    root["session_id"] = sessionId_;
    root["mode"] = "live_data_collection";
    root["detector_only"] = true;
    root["classifier_enabled"] = false;
    root["daq_enabled"] = false;
    root["crop_extraction_enabled"] = false;
    root["started_at_utc"] = utcText(startedAtUtc_);
    root["stopped_at_utc"] = stoppedAtUtc_.isValid() ? utcText(stoppedAtUtc_) : QString();
    root["stop_reason"] = stopReason;
    root["frames_saved"] = QString::number(static_cast<qulonglong>(framesSaved_));
    root["detection_rows"] = QString::number(static_cast<qulonglong>(rowsLogged_));
    root["stream_dir"] = "stream";
    root["detections_csv"] = "detections.csv";

    QFile metadataFile(QDir(sessionDir_).filePath("collection_metadata.json"));
    if (!metadataFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        err = QString("Failed to write collection metadata: %1").arg(metadataFile.fileName()).toStdString();
        return false;
    }
    metadataFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    metadataFile.close();
    return true;
}

bool LiveDataCollectionWriter::appendDetectionRow(const QString& relativeImagePath, const PipelineEvent& event,
                                                  bool detectorProcessed, qint64 sourceFrameNumber,
                                                  const QDateTime& timestampUtc, std::string& err) {
    QFile csv(csvPath_);
    if (!csv.open(QIODevice::Append | QIODevice::Text)) {
        err = QString("Failed to append detections CSV: %1").arg(csvPath_).toStdString();
        return false;
    }

    const bool detected = detectorProcessed && event.detected;
    QTextStream ts(&csv);
    ts << csvQuote(relativeImagePath) << "," << (detected ? "1" : "0") << "," << "," << csvQuote(utcText(timestampUtc))
       << "," << (sourceFrameNumber > 0 ? QString::number(sourceFrameNumber)
                                         : QString::number(static_cast<qulonglong>(framesSaved_ + 1)))
       << "," << fieldForInt(event.bbox.x, detected) << "," << fieldForInt(event.bbox.y, detected) << ","
       << fieldForInt(event.bbox.width, detected) << "," << fieldForInt(event.bbox.height, detected) << ","
       << fieldForDouble(event.centroid.x, detected) << "," << fieldForDouble(event.centroid.y, detected) << ","
       << fieldForDouble(event.area, detected, 'f', 1) << "," << "," << "\n";
    ts.flush();
    csv.close();
    return true;
}

void LiveDataCollectionWriter::reset() {
    sessionId_.clear();
    sessionDir_.clear();
    streamDir_.clear();
    csvPath_.clear();
    startedAtUtc_ = QDateTime();
    stoppedAtUtc_ = QDateTime();
    framesSaved_ = 0;
    rowsLogged_ = 0;
    active_ = false;
}
