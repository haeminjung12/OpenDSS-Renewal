#pragma once

#include "live_sorting_service.h"
#include "../camera/camera_device.h"

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace desktop_app::v2 {

class CameraController;

namespace live {

struct LiveControllerFacts {
    QString defaultRunRoot;
    QString opendssVersion;
    QString activeModelName;
    QVector<run::RunClassSnapshot> activeModelClasses;
    bool activeModelLoadable = false;
    QString activeModelId;
    std::function<bool(const QJsonObject&, QString*)> applyCameraProfile;
    std::function<bool(const QJsonObject&, QString*)> applyDaqProfile;
    std::function<bool(const QString&, QString*)> activateModel;
    run::HitBoundarySnapshot hitBoundary;
    QJsonObject detectorSettings;
    QJsonObject cropSettings;
    QJsonObject timingSettings;
    QJsonObject cameraSettings;
    QJsonObject daqSettings;
    double nominalCameraFps = 0.0;
};

using LiveControllerFactsProvider = std::function<LiveControllerFacts()>;
using ResultsRefreshCallback = std::function<void(const QString& runFolder)>;

class LiveSortingController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString presentation READ presentation NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY changed)
    Q_PROPERTY(bool cameraStreaming READ cameraStreaming NOTIFY changed)
    Q_PROPERTY(bool startSortingEnabled READ startSortingEnabled NOTIFY changed)
    Q_PROPERTY(QString disabledReason READ disabledReason NOTIFY changed)
    Q_PROPERTY(bool decisionBoundaryDefined READ decisionBoundaryDefined NOTIFY changed)
    Q_PROPERTY(double decisionBoundaryXRatio READ decisionBoundaryXRatio NOTIFY changed)
    Q_PROPERTY(double decisionBoundaryYRatio READ decisionBoundaryYRatio NOTIFY changed)
    Q_PROPERTY(QString decisionBoundarySide READ decisionBoundarySide NOTIFY changed)
    Q_PROPERTY(QString runName READ runName WRITE setRunName NOTIFY changed)
    Q_PROPERTY(QString experimentType READ experimentType WRITE setExperimentType
                   NOTIFY changed)
    Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY changed)
    Q_PROPERTY(QString duration READ duration WRITE setDuration NOTIFY changed)
    Q_PROPERTY(QString saveLocation READ saveLocation WRITE setSaveLocation
                   NOTIFY changed)
    Q_PROPERTY(QString activeModelText READ activeModelText NOTIFY changed)
    Q_PROPERTY(QStringList hitClassOptions READ hitClassOptions NOTIFY changed)
    Q_PROPERTY(QVariantList hitClassModel READ hitClassModel NOTIFY changed)
    Q_PROPERTY(QString hitClassId READ hitClassId WRITE setHitClassId NOTIFY changed)
    Q_PROPERTY(bool triggerEveryDroplet READ triggerEveryDroplet WRITE
                   setTriggerEveryDroplet NOTIFY changed)
    Q_PROPERTY(bool daqOutputEnabled READ daqOutputEnabled WRITE setDaqOutputEnabled
                   NOTIFY changed)
    Q_PROPERTY(bool recordFullImageSequence READ recordFullImageSequence WRITE
                   setRecordFullImageSequence NOTIFY changed)
    Q_PROPERTY(double elapsedSeconds READ elapsedSeconds NOTIFY changed)
    Q_PROPERTY(qint64 persistedEvents READ persistedEvents NOTIFY changed)
    Q_PROPERTY(QVariantMap integrity READ integrity NOTIFY changed)
    Q_PROPERTY(QString stopReason READ stopReason NOTIFY changed)
    Q_PROPERTY(QString runFolder READ runFolder NOTIFY changed)
    Q_PROPERTY(QString profilePath READ profilePath NOTIFY changed)
    Q_PROPERTY(QString profileStatus READ profileStatus NOTIFY changed)
    Q_PROPERTY(bool canSaveProfile READ canSaveProfile NOTIFY changed)

public:
    LiveSortingController(LiveSortingService& service,
                          CameraController& cameraController,
                          LiveControllerFactsProvider factsProvider,
                          DaqReadinessGate daqReadiness,
                          ResultsRefreshCallback resultsRefresh,
                          QObject* parent = nullptr);
    ~LiveSortingController() override;

    QString presentation() const;
    QString error() const;
    QString diagnostic() const;
    bool cameraStreaming() const;
    bool startSortingEnabled() const;
    QString disabledReason() const;
    bool decisionBoundaryDefined() const;
    double decisionBoundaryXRatio() const;
    double decisionBoundaryYRatio() const;
    QString decisionBoundarySide() const;
    QString runName() const;
    void setRunName(const QString& value);
    QString experimentType() const;
    void setExperimentType(const QString& value);
    QString notes() const;
    void setNotes(const QString& value);
    QString duration() const;
    void setDuration(const QString& value);
    QString saveLocation() const;
    void setSaveLocation(const QString& value);
    QString activeModelText() const;
    QStringList hitClassOptions() const;
    QVariantList hitClassModel() const;
    QString hitClassId() const;
    void setHitClassId(const QString& value);
    bool triggerEveryDroplet() const;
    void setTriggerEveryDroplet(bool value);
    bool daqOutputEnabled() const;
    void setDaqOutputEnabled(bool value);
    bool recordFullImageSequence() const;
    void setRecordFullImageSequence(bool value);
    double elapsedSeconds() const;
    qint64 persistedEvents() const;
    QVariantMap integrity() const;
    QString stopReason() const;
    QString runFolder() const;
    QString profilePath() const;
    QString profileStatus() const;
    bool canSaveProfile() const;

    Q_INVOKABLE bool startCamera();
    Q_INVOKABLE bool stopCamera();
    Q_INVOKABLE bool startSorting();
    Q_INVOKABLE bool setDecisionBoundary(double xRatio, double yRatio);
    Q_INVOKABLE void resetDecisionBoundary();
    Q_INVOKABLE void setDecisionBoundarySide(const QString& side);
    Q_INVOKABLE bool pauseSorting();
    Q_INVOKABLE bool resumeSorting();
    Q_INVOKABLE bool stopSorting();
    Q_INVOKABLE bool primaryAction();
    Q_INVOKABLE bool secondaryAction();
    Q_INVOKABLE void startNewRun();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool openProfile(const QUrl& fileUrl);
    Q_INVOKABLE bool saveProfile();
    Q_INVOKABLE bool saveProfileAs(const QUrl& fileUrl);

signals:
    void changed();

private:
    enum class ServiceAction {
        Pause,
        Resume,
        Stop,
        PollDuration,
    };

    QString preflightError() const;
    std::optional<double> requestedDuration(QString* error) const;
    void acceptFrame(CameraFrame frame);
    bool requestServiceAction(ServiceAction action);
    void serviceActionLoop();
    void completeServiceAction(ServiceAction action, bool succeeded,
                               const QString& error,
                               const LiveSortingSnapshot& snapshot);
    void updateSnapshot();
    void projectSnapshot(const LiveSortingSnapshot& snapshot);
    void setActionError(const QString& value);
    QJsonObject profileDocument() const;
    bool writeProfile(const QString& path);

    LiveSortingService& service_;
    CameraController& cameraController_;
    LiveControllerFactsProvider factsProvider_;
    DaqReadinessGate daqReadiness_;
    ResultsRefreshCallback resultsRefresh_;
    LiveControllerFacts facts_;
    LiveSortingSnapshot snapshot_;
    QTimer pollTimer_;
    QString runName_;
    QString experimentType_;
    QString notes_;
    QString duration_;
    QString saveLocation_;
    QString hitClassId_;
    QString actionError_;
    QString profilePath_;
    QString profileStatus_;
    double decisionBoundaryXRatio_ = 0.0;
    double decisionBoundaryYRatio_ = 0.0;
    bool decisionBoundaryDefined_ = false;
    std::optional<QJsonObject> profileDetectorSettings_;
    std::optional<QJsonObject> profileCropSettings_;
    std::optional<QJsonObject> profileTimingSettings_;
    bool triggerEveryDroplet_ = true;
    bool daqOutputEnabled_ = false;
    bool recordFullImageSequence_ = false;
    bool outcomeCleared_ = false;
    bool resultsNotified_ = false;
    bool actionInProgress_ = false;
    bool pollInProgress_ = false;
    quint64 lastDeliveryId_ = 0;
    qint64 lastTimestampNs_ = 0;
    qint64 droppedFrames_ = 0;
    std::mutex actionMutex_;
    std::condition_variable actionReady_;
    std::optional<ServiceAction> pendingAction_;
    bool actionWorkerStopping_ = false;
    std::thread actionWorker_;
};

} // namespace live
} // namespace desktop_app::v2
