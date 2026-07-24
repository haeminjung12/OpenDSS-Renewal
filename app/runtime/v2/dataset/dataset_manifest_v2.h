#pragma once

#include <QRect>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

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

class DatasetManifestV2 {
  public:
    static constexpr auto SchemaVersion = "opendss.dataset.v2";

    static std::optional<DatasetManifestV2> load(const QString& path, QString* error = nullptr);
    static bool save(const QString& path, const QString& datasetId,
                     const QVector<DatasetClass>& classes,
                     const QVector<DatasetRecord>& records,
                     const QVector<UserLabelRecord>& labels, QString* error = nullptr);

    const QString& datasetId() const noexcept;
    const QVector<DatasetClass>& classes() const noexcept;
    const QVector<DatasetRecord>& records() const noexcept;
    const QVector<UserLabelRecord>& labels() const noexcept;
    QVector<TrainingSample> trainingSamples(QString* error = nullptr) const;

  private:
    static std::optional<DatasetManifestV2> fromJsonObject(const QJsonObject& root,
                                                           const QString& path,
                                                           QString* error);

    QString datasetRoot_;
    QString datasetId_;
    QVector<DatasetClass> classes_;
    QVector<DatasetRecord> records_;
    QVector<UserLabelRecord> labels_;
};

} // namespace desktop_app::v2::dataset
