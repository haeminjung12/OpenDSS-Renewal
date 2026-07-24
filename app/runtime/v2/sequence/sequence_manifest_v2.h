#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <optional>

namespace desktop_app::v2::sequence {

struct SequenceFrameRange {
    qint64 first = 0;
    qint64 last = 0;
};

struct SequenceLossCategory {
    qint64 count = 0;
    QVector<SequenceFrameRange> ranges;
};

struct SequenceIntegrity {
    SequenceLossCategory sourceFrameGaps;
    SequenceLossCategory queueRejections;
    SequenceLossCategory consumerFailures;
};

struct SequenceManifestData {
    QString sequenceId;
    QString name;
    QString experimentType;
    QString notes;
    QString status;
    QString createdAt;
    QString startedAt;
    QString endedAt;
    std::optional<double> requestedDurationSeconds;
    QString stopReason;
    QString opendssVersion;
    qint64 frameCount = 0;
    QJsonObject cameraSettings;
    int imageWidth = 0;
    int imageHeight = 0;
    int bitDepth = 0;
    double nominalFps = 0.0;
    SequenceIntegrity integrity;
};

class SequenceManifestV2 {
  public:
    static constexpr auto SchemaVersion = "opendss.sequence.v2";

    static std::optional<SequenceManifestV2> load(const QString& path, QString* error = nullptr);
    static bool save(const QString& path, const SequenceManifestData& data,
                     QString* error = nullptr);

    const SequenceManifestData& data() const noexcept;

  private:
    static std::optional<SequenceManifestV2> fromJsonObject(const QJsonObject& root,
                                                            QString* error);

    SequenceManifestData data_;
};

} // namespace desktop_app::v2::sequence
