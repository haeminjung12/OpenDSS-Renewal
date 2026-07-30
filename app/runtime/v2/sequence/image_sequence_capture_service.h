#pragma once

#include "sequence_manifest_v2.h"
#include "../camera/camera_device.h"
#include "../operation/operation_coordinator.h"
#include "../../desktop_app/live_frame_dispatcher.h"

#include <QJsonObject>
#include <QString>

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace desktop_app::v2 {

class CameraService;

namespace sequence {

struct ImageSequenceCaptureRequest {
    QString saveRoot;
    QString name;
    QString experimentType;
    QString notes;
    std::optional<double> durationSeconds;
    QString opendssVersion;
    QJsonObject cameraSettings;
};

struct ImageSequenceCaptureSnapshot {
    OperationLifecycle lifecycle = OperationLifecycle::Idle;
    QString folder;
    qint64 savedFrameCount = 0;
    double activeElapsedSeconds = 0.0;
    SequenceIntegrity integrity;
    QString error;
};

class ImageSequenceCaptureService final {
  public:
    using MonotonicNow = std::function<qint64()>;
    using FrameConverter = std::function<QImage(const CameraFrame&, QString*)>;
    using FrameWriter = std::function<bool(const QImage&, const QString&, QString*)>;

    ImageSequenceCaptureService(CameraService& camera, OperationCoordinator& operations,
                                MonotonicNow monotonicNow,
                                FrameConverter frameConverter = {},
                                FrameWriter frameWriter = {});
    ~ImageSequenceCaptureService();

    bool start(const ImageSequenceCaptureRequest& request, QString* error = nullptr);
    bool offerFrame(const CameraFrame& frame, double nominalFps, QString* error = nullptr);
    bool pause(QString* error = nullptr);
    bool resume(QString* error = nullptr);
    bool stop(QString* error = nullptr);
    bool pollDuration(QString* error = nullptr);
    ImageSequenceCaptureSnapshot snapshot();

  private:
    void consumeFrame(const QImage& image, const FrameMeta& meta, double fps,
                      std::uint64_t handoffId, LiveFrameDispatcher::Membership membership);
    bool stopWithReason(const QString& reason, QString* error);
    bool failAndRelease(const QString& message, const QString& stopReason, QString* error);
    void updateFailedRecovery(const QString& stopReason, const QString& message);
    void refreshAsyncFailure();
    double activeElapsedLocked(qint64 now) const;
    SequenceIntegrity combinedIntegrity() const;

    CameraService& camera_;
    OperationCoordinator& operations_;
    MonotonicNow monotonicNow_;
    FrameConverter frameConverter_;
    FrameWriter frameWriter_;

    mutable std::mutex mutex_;
    OperationLease lease_;
    OperationLifecycle lifecycle_ = OperationLifecycle::Idle;
    ImageSequenceCaptureRequest request_;
    QString sequenceId_;
    QString displayName_;
    QString folder_;
    QString framesFolder_;
    QString partialPath_;
    QString createdAt_;
    QString startedAt_;
    QString error_;
    qint64 savedFrameCount_ = 0;
    qint64 activeElapsedNs_ = 0;
    std::optional<qint64> activeStartedNs_;
    bool acceptingOffers_ = false;
    std::optional<quint64> lastSourceDelivery_;
    SequenceLossCategory sourceGaps_;
    std::uint64_t lastAcceptedHandoff_ = 0;
    qint64 dispatcherDelivery_ = 0;
    bool formatFixed_ = false;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    int bitDepth_ = 0;
    double nominalFps_ = 0.0;

    LiveFrameDispatcher dispatcher_;
};

} // namespace sequence
} // namespace desktop_app::v2
