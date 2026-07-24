#pragma once

#include "dataset_manifest_v2.h"
#include "../operation/operation_coordinator.h"
#include "../../desktop_app/live_frame_dispatcher.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <mutex>
#include <optional>

class IDropletDetector;

namespace desktop_app::v2::dataset {

struct DatasetCaptureRequest {
    QString saveRoot;
    QString name;
    QString experimentType;
    QString notes;
    std::optional<double> durationSeconds;
    QString opendssVersion;
    QJsonObject cameraSettings;
    QJsonObject detectionSettings;
    QJsonObject programSettings;
};

struct DatasetCaptureSnapshot {
    OperationLifecycle lifecycle = OperationLifecycle::Idle;
    QString folder;
    qint64 savedFrameCount = 0;
    qint64 savedCropCount = 0;
    double activeElapsedSeconds = 0.0;
    sequence::SequenceIntegrity integrity;
    QString error;
};

class DatasetCaptureService final {
  public:
    using MonotonicNow = std::function<qint64()>;

    DatasetCaptureService(OperationCoordinator& operations, IDropletDetector& detector,
                          MonotonicNow monotonicNow);
    ~DatasetCaptureService();

    bool start(const DatasetCaptureRequest& request, QString* error = nullptr);
    bool offerFrame(const QImage& image, const FrameMeta& meta, double nominalFps,
                    QString* error = nullptr);
    bool pause(QString* error = nullptr);
    bool resume(QString* error = nullptr);
    bool stop(QString* error = nullptr);
    bool pollDuration(QString* error = nullptr);
    DatasetCaptureSnapshot snapshot();

  private:
    void consumeFrame(const QImage& image, const FrameMeta& meta, double fps,
                      std::uint64_t handoffId, LiveFrameDispatcher::Membership membership);
    bool finish(const QString& reason, QString* error);
    bool failAndRelease(const QString& reason, const QString& message, QString* error);
    void refreshFailure();
    bool saveManifest(const QString& status, const QString& reason, QString* error);
    double activeElapsedLocked(qint64 now) const;
    sequence::SequenceIntegrity integrityLocked() const;

    OperationCoordinator& operations_;
    IDropletDetector& detector_;
    MonotonicNow monotonicNow_;
    mutable std::mutex mutex_;
    OperationLease lease_;
    OperationLifecycle lifecycle_ = OperationLifecycle::Idle;
    DatasetCaptureRequest request_;
    QString datasetId_;
    QString displayName_;
    QString folder_;
    QString sequenceFolder_;
    QString cropsFolder_;
    QString partialPath_;
    QString createdAt_;
    QString startedAt_;
    QString error_;
    qint64 savedFrameCount_ = 0;
    qint64 activeElapsedNs_ = 0;
    std::optional<qint64> activeStartedNs_;
    bool acceptingOffers_ = false;
    bool formatFixed_ = false;
    int width_ = 0;
    int height_ = 0;
    int bitDepth_ = 0;
    double fps_ = 0.0;
    qint64 dispatcherDelivery_ = 0;
    std::optional<qint64> lastSourceDelivery_;
    std::uint64_t lastAcceptedHandoff_ = 0;
    sequence::SequenceLossCategory sourceGaps_;
    QVector<DatasetRecord> records_;
    LiveFrameDispatcher dispatcher_;
};

} // namespace desktop_app::v2::dataset
