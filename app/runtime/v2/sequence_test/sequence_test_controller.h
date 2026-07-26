#pragma once

#include "sequence_test_service.h"

#include <QByteArray>
#include <QObject>
#include <QJsonObject>
#include <QUrl>
#include <QVariantList>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace desktop_app::v2::sequence_test {

using ActiveModelSnapshotProvider =
    std::function<std::optional<run::ModelSnapshot>(QString*)>;
using ResultsRefreshCallback = std::function<void()>;
using StorageRootProvider = std::function<QString()>;
using AvailableMemoryProvider = std::function<quint64()>;

class SequenceTestController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(bool canLoadToMemory READ canLoadToMemory NOTIFY changed)
    Q_PROPERTY(bool canStart READ canStart NOTIFY changed)
    Q_PROPERTY(QString activeModelName READ activeModelName NOTIFY changed)
    Q_PROPERTY(bool activeModelReady READ activeModelReady NOTIFY changed)
    Q_PROPERTY(QUrl sourceManifestUrl READ sourceManifestUrl NOTIFY changed)
    Q_PROPERTY(QString sequenceName READ sequenceName NOTIFY changed)
    Q_PROPERTY(QUrl sequenceFolderUrl READ sequenceFolderUrl NOTIFY changed)
    Q_PROPERTY(QString sequencePath READ sequencePath NOTIFY changed)
    Q_PROPERTY(qint64 frameCount READ frameCount NOTIFY changed)
    Q_PROPERTY(double recordedFps READ recordedFps NOTIFY changed)
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY changed)
    Q_PROPERTY(QString sequenceValidation READ sequenceValidation NOTIFY changed)
    Q_PROPERTY(qulonglong availableMemoryBytes READ availableMemoryBytes NOTIFY changed)
    Q_PROPERTY(qulonglong bufferBytes READ bufferBytes NOTIFY changed)
    Q_PROPERTY(bool memoryReady READ memoryReady NOTIFY changed)
    Q_PROPERTY(QString loadStatus READ loadStatus NOTIFY changed)
    Q_PROPERTY(double requestedProcessingFps READ requestedProcessingFps WRITE
                   setRequestedProcessingFps NOTIFY changed)
    Q_PROPERTY(double achievedProcessingFps READ achievedProcessingFps NOTIFY changed)
    Q_PROPERTY(qint64 processedFrames READ processedFrames NOTIFY changed)
    Q_PROPERTY(qint64 totalFrames READ totalFrames NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString outputStatus READ outputStatus NOTIFY changed)
    Q_PROPERTY(bool triggerEveryDroplet READ triggerEveryDroplet WRITE
                   setTriggerEveryDroplet NOTIFY changed)
    Q_PROPERTY(QVariantList hitClassModel READ hitClassModel NOTIFY changed)
    Q_PROPERTY(QString selectedHitClassId READ selectedHitClassId WRITE
                   setSelectedHitClassId NOTIFY changed)
    Q_PROPERTY(bool physicalDaqOutputEnabled READ physicalDaqOutputEnabled WRITE
                   setPhysicalDaqOutputEnabled NOTIFY changed)
    Q_PROPERTY(QUrl outputFolderUrl READ outputFolderUrl WRITE setOutputFolderUrl
                   NOTIFY changed)

  public:
    SequenceTestController(
        SequenceTestService& service,
        ActiveModelSnapshotProvider activeModelProvider,
        ResultsRefreshCallback resultsRefresh,
        StorageRootProvider storageRootProvider,
        AvailableMemoryProvider availableMemoryProvider,
        DaqReadinessGate daqReadinessProvider,
        QJsonObject detectorSettings,
        QJsonObject cropSettings,
        QJsonObject timingSettings,
        QString opendssVersion,
        QObject* parent = nullptr);
    ~SequenceTestController() override;

    QString presentation() const;
    QString errorMessage() const;
    bool canLoadToMemory() const;
    bool canStart() const;
    QString activeModelName() const;
    bool activeModelReady() const;
    QUrl sourceManifestUrl() const;
    QString sequenceName() const;
    QUrl sequenceFolderUrl() const;
    QString sequencePath() const;
    qint64 frameCount() const;
    double recordedFps() const;
    QUrl previewUrl() const;
    QString sequenceValidation() const;
    qulonglong availableMemoryBytes() const;
    qulonglong bufferBytes() const;
    bool memoryReady() const;
    QString loadStatus() const;
    double requestedProcessingFps() const;
    void setRequestedProcessingFps(double value);
    double achievedProcessingFps() const;
    qint64 processedFrames() const;
    qint64 totalFrames() const;
    double progress() const;
    QString outputStatus() const;
    bool triggerEveryDroplet() const;
    void setTriggerEveryDroplet(bool value);
    QVariantList hitClassModel() const;
    QString selectedHitClassId() const;
    void setSelectedHitClassId(const QString& value);
    bool physicalDaqOutputEnabled() const;
    void setPhysicalDaqOutputEnabled(bool value);
    QUrl outputFolderUrl() const;
    void setOutputFolderUrl(const QUrl& value);

    Q_INVOKABLE bool selectSequence(const QUrl& sequenceJson);
    Q_INVOKABLE bool loadToMemory();
    Q_INVOKABLE bool start();
    Q_INVOKABLE bool stop();

  public slots:
    void refreshPreflight();

  signals:
    void changed();
    void stopRequestAccepted();

  private:
    bool operationActive() const;
    bool inputLocked() const;
    void clearSelectedSequence();
    void clearLoadedSequence();
    void refreshAvailableMemory();
    void refreshActiveModel();
    void updatePreflight(bool preserveFailure = false);
    void postProgress(quint64 generation, const SequenceTestProgress& progress);
    void finishLoad(quint64 generation,
                    std::shared_ptr<const LoadedSequence> loaded,
                    qulonglong actualBytes,
                    const QByteArray& manifestBytes,
                    const QString& error);
    void finishRun(quint64 generation, bool succeeded, const QString& error);

    SequenceTestService& service_;
    ActiveModelSnapshotProvider activeModelProvider_;
    ResultsRefreshCallback resultsRefresh_;
    AvailableMemoryProvider availableMemoryProvider_;
    DaqReadinessGate daqReadinessProvider_;
    run::HitBoundarySnapshot hitBoundary_;
    const QJsonObject detectorSettings_;
    const QJsonObject cropSettings_;
    const QJsonObject timingSettings_;
    const QString opendssVersion_;

    QString presentation_ = QStringLiteral("empty");
    QString errorMessage_;
    bool canLoadToMemory_ = false;
    bool canStart_ = false;
    QString activeModelName_;
    bool activeModelReady_ = false;
    std::optional<run::ModelSnapshot> activeModel_;
    QUrl sourceManifestUrl_;
    QString sourceManifestPath_;
    QByteArray selectedManifestBytes_;
    QByteArray frozenManifestBytes_;
    QString sequenceId_;
    QString sequenceName_;
    QString experimentType_;
    QString notes_;
    QUrl sequenceFolderUrl_;
    QString sequencePath_;
    qint64 frameCount_ = 0;
    QString frameFilenamePattern_;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    double recordedFps_ = 0.0;
    QJsonObject cameraSettings_;
    QUrl previewUrl_;
    QString sequenceValidation_ = QStringLiteral("Not selected");
    qulonglong availableMemoryBytes_ = 0;
    qulonglong bufferBytes_ = 0;
    bool memoryReady_ = false;
    QString loadStatus_ = QStringLiteral("Not loaded");
    double requestedProcessingFps_ = 0.0;
    double achievedProcessingFps_ = 0.0;
    qint64 processedFrames_ = 0;
    qint64 totalFrames_ = 0;
    QString outputStatus_ = QStringLiteral("Not started");
    bool triggerEveryDroplet_ = false;
    QVariantList hitClassModel_;
    QString selectedHitClassId_;
    bool physicalDaqOutputEnabled_ = false;
    QString physicalDaqWarning_;
    QUrl outputFolderUrl_;
    std::shared_ptr<const LoadedSequence> loadedSequence_;

    std::atomic_bool shuttingDown_{false};
    std::atomic_bool cancelLoad_{false};
    std::atomic_bool stopRequested_{false};
    quint64 selectionGeneration_ = 0;
    quint64 runGeneration_ = 0;
    std::thread loadWorker_;
    std::thread runWorker_;
    std::thread stopWorker_;
};

} // namespace desktop_app::v2::sequence_test
