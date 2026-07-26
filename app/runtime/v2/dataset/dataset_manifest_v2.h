#pragma once

#include <QRect>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

#include "../sequence/sequence_manifest_v2.h"

namespace desktop_app::v2::dataset {

struct DatasetClass {
    QString id;
    QString name;
};

struct DatasetRecord {
    QString recordId;
    QString cropPath;
    QString cropSha256;
    QString sourceFrameId;
    QString sourceEventId;
    QString timestamp;
    QRect cropRect;
    qint64 sourceFrameIndex = 0;
};

struct UserLabelRecord {
    QString labelId;
    QString recordId;
    QString classId;
    bool excluded = false;
};

struct TrainingSample {
    QString recordId;
    QString classId;
    QString cropPath;
};

struct DatasetSequenceInfo {
    QString folder = "sequence";
    QString frameFilenamePattern = "sequence/frame_%08d.tif";
    qint64 frameCount = 0;
    int imageWidth = 0;
    int imageHeight = 0;
    int bitDepth = 0;
    double nominalFps = 0.0;
    sequence::SequenceIntegrity integrity;
};

struct DatasetCropSettings {
    int width = 64;
    int height = 64;
    QString pixelFormat = "gray8";
    QString fileFormat = "png";
    QString method = "centered_max_bbox_clamped";
    QString interpolation = "area";
};

struct DatasetCaptureProvenance {
    QString provenanceMode;
    QString name;
    QString experimentType;
    QString notes;
    QString opendssVersion;
    QString createdAt;
    QString updatedAt;
    QString captureStartedAt;
    QString captureEndedAt;
    std::optional<double> requestedDurationSeconds;
    QString stopReason;
    QString status;
    DatasetSequenceInfo sequence;
    DatasetCropSettings crop;
    QJsonObject cameraSettings;
    QJsonObject detectionSettings;
    QJsonObject programSettings;
};

struct DatasetCounts {
    qint64 total = 0;
    qint64 unlabeled = 0;
    qint64 labeled = 0;
    qint64 removed = 0;
    QJsonObject byClass;
};

struct DatasetManifestData {
    QString datasetId;
    DatasetCaptureProvenance provenance;
    QVector<DatasetClass> classes;
    QVector<DatasetRecord> records;
    QVector<UserLabelRecord> labels;
};

class DatasetManifestV2 {
  public:
    static constexpr auto SchemaVersion = "opendss.dataset.v2";

    static std::optional<DatasetManifestV2> load(const QString& path, QString* error = nullptr);
    static bool save(const QString& path, const DatasetManifestData& data,
                     QString* error = nullptr);

    const DatasetManifestData& data() const noexcept;
    const QString& datasetId() const noexcept;
    const QVector<DatasetClass>& classes() const noexcept;
    const QVector<DatasetRecord>& records() const noexcept;
    const QVector<UserLabelRecord>& labels() const noexcept;
    QVector<TrainingSample> trainingSamples(QString* error = nullptr) const;
    DatasetCounts counts() const;

  private:
    static std::optional<DatasetManifestV2> fromJsonObject(const QJsonObject& root,
                                                           const QString& path,
                                                           QString* error);

    QString datasetRoot_;
    DatasetManifestData data_;
};

} // namespace desktop_app::v2::dataset
