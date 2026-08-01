#pragma once

#include "image_sequence_capture_service.h"
#include "../dataset/dataset_capture_service.h"

#include <QObject>
#include <QTimer>

#include <functional>
#include <future>
#include <memory>

class DropletFrameProcessor;

namespace desktop_app::v2 {

class CameraController;
class CameraService;

namespace sequence {

class CaptureWorkflowController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sequencePresentation READ sequencePresentation NOTIFY changed)
    Q_PROPERTY(QString datasetPresentation READ datasetPresentation NOTIFY changed)
    Q_PROPERTY(qint64 sequenceFrameCount READ sequenceFrameCount NOTIFY changed)
    Q_PROPERTY(qint64 sequenceFinalizedFrameCount READ sequenceFinalizedFrameCount NOTIFY changed)
    Q_PROPERTY(qint64 datasetFrameCount READ datasetFrameCount NOTIFY changed)
    Q_PROPERTY(qint64 datasetCropCount READ datasetCropCount NOTIFY changed)
    Q_PROPERTY(QString sequenceLocation READ sequenceLocation WRITE setSequenceLocation NOTIFY changed)
    Q_PROPERTY(QString datasetLocation READ datasetLocation WRITE setDatasetLocation NOTIFY changed)
    Q_PROPERTY(QString sequenceFolder READ sequenceFolder NOTIFY changed)
    Q_PROPERTY(QString datasetFolder READ datasetFolder NOTIFY changed)
    Q_PROPERTY(QString sequenceError READ sequenceError NOTIFY changed)
    Q_PROPERTY(QString datasetError READ datasetError NOTIFY changed)
    Q_PROPERTY(bool captureActive READ captureActive NOTIFY changed)
    Q_PROPERTY(bool captureStartAvailable READ captureStartAvailable NOTIFY changed)

public:
    using MonotonicNow = std::function<qint64()>;
    using CameraSettingsProvider = std::function<QJsonObject()>;

    CaptureWorkflowController(CameraService &cameraService,
                              CameraController &cameraController,
                              OperationCoordinator &operations,
                              DropletFrameProcessor &processor,
                              MonotonicNow monotonicNow,
                              CameraSettingsProvider cameraSettingsProvider,
                              QString opendssVersion,
                              QObject *parent = nullptr);
    ~CaptureWorkflowController() override;

    QString sequencePresentation() const;
    QString datasetPresentation() const;
    qint64 sequenceFrameCount() const;
    qint64 sequenceFinalizedFrameCount() const;
    qint64 datasetFrameCount() const;
    qint64 datasetCropCount() const;
    QString sequenceLocation() const;
    QString datasetLocation() const;
    QString sequenceFolder() const;
    QString datasetFolder() const;
    QString sequenceError() const;
    QString datasetError() const;
    bool captureActive() const;
    bool captureStartAvailable() const;

    void setSequenceLocation(const QString &path);
    void setDatasetLocation(const QString &path);

    Q_INVOKABLE bool startSequence(const QString &name = {},
                                   const QString &experimentType = {},
                                   const QString &notes = {},
                                   const QString &duration = {});
    Q_INVOKABLE bool pauseOrResumeSequence();
    Q_INVOKABLE bool stopSequence();
    Q_INVOKABLE void newSequence();
    Q_INVOKABLE bool startDataset(const QString &name = {},
                                  const QString &experimentType = {},
                                  const QString &notes = {},
                                  const QString &duration = {});
    Q_INVOKABLE bool pauseOrResumeDataset();
    Q_INVOKABLE bool stopDataset();
    Q_INVOKABLE void newDataset();

signals:
    void changed();

private:
    static QString presentation(OperationLifecycle lifecycle);
    static std::optional<double> parseDuration(const QString &text, QString *error);
    void acceptFrame(const CameraFrame &frame);
    bool launchSequenceStop(bool durationExpired);
    void collectSequenceStop(bool wait);
    void refresh();
    double stableNominalFps() const;

    CameraService &cameraService_;
    CameraController &cameraController_;
    OperationCoordinator &operations_;
    DropletFrameProcessor &processor_;
    MonotonicNow monotonicNow_;
    CameraSettingsProvider cameraSettingsProvider_;
    QString opendssVersion_;
    QString sequenceLocation_;
    QString datasetLocation_;
    QString sequenceActionError_;
    QString datasetActionError_;
    std::unique_ptr<ImageSequenceCaptureService> sequenceService_;
    std::unique_ptr<dataset::DatasetCaptureService> datasetService_;
    ImageSequenceCaptureSnapshot sequenceSnapshot_;
    dataset::DatasetCaptureSnapshot datasetSnapshot_;
    QTimer pollTimer_;
    qint64 previousTimestampNs_ = 0;
    double estimatedFps_ = 100.0;
    double activeCaptureFps_ = 100.0;

    struct SequenceStopResult {
        bool ok = false;
        QString error;
    };
    std::future<SequenceStopResult> sequenceStopFuture_;
};

} // namespace sequence
} // namespace desktop_app::v2
