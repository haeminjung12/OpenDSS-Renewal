#include "live_sorting_controller.h"

#include "../camera/camera_controller.h"
#include "../camera/frame_conversion.h"
#include "../../desktop_app/frame_types.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>
#include <utility>

namespace desktop_app::v2::live {
namespace {

bool activeLifecycle(OperationLifecycle lifecycle) {
    return lifecycle == OperationLifecycle::Starting ||
           lifecycle == OperationLifecycle::Running ||
           lifecycle == OperationLifecycle::Paused ||
           lifecycle == OperationLifecycle::Stopping;
}

QVariantMap integritySeries(const run::RunIntegritySeries& series) {
    QVariantList ranges;
    for (const auto& range : series.ranges) {
        ranges.push_back(
            QVariantMap{{QStringLiteral("first"), range.first},
                        {QStringLiteral("last"), range.last}});
    }
    return {{QStringLiteral("count"), series.count},
            {QStringLiteral("ranges"), ranges}};
}

bool validBoundary(const run::HitBoundarySnapshot& boundary) {
    return std::isfinite(boundary.boundaryY) && boundary.boundaryY >= 0.0 &&
           boundary.imageWidth > 0 && boundary.imageHeight > 0 &&
           boundary.boundaryY < boundary.imageHeight;
}

} // namespace

LiveSortingController::LiveSortingController(
    LiveSortingService& service, CameraController& cameraController,
    LiveControllerFactsProvider factsProvider, DaqReadinessGate daqReadiness,
    ResultsRefreshCallback resultsRefresh, QObject* parent)
    : QObject(parent), service_(service), cameraController_(cameraController),
      factsProvider_(std::move(factsProvider)),
      daqReadiness_(std::move(daqReadiness)),
      resultsRefresh_(std::move(resultsRefresh)) {
    pollTimer_.setInterval(100);
    connect(&pollTimer_, &QTimer::timeout, this, [this] {
        updateSnapshot();
        if (activeLifecycle(snapshot_.lifecycle))
            requestServiceAction(ServiceAction::PollDuration);
    });
    connect(&cameraController_, &CameraController::frameReady, this,
            &LiveSortingController::acceptFrame, Qt::QueuedConnection);
    connect(&cameraController_, &CameraController::stateChanged, this,
            &LiveSortingController::refresh);
    connect(&cameraController_, &CameraController::errorChanged, this,
            &LiveSortingController::refresh);
    refresh();
    actionWorker_ = std::thread([this] { serviceActionLoop(); });
    pollTimer_.start();
}

LiveSortingController::~LiveSortingController() {
    pollTimer_.stop();
    {
        std::lock_guard lock(actionMutex_);
        actionWorkerStopping_ = true;
        pendingAction_.reset();
        actionReady_.notify_all();
    }
    if (actionWorker_.joinable())
        actionWorker_.join();
}

QString LiveSortingController::presentation() const {
    if (outcomeCleared_ && !activeLifecycle(snapshot_.lifecycle))
        return cameraController_.cameraStatus() == QStringLiteral("Unavailable")
                   ? QStringLiteral("unavailable")
                   : QStringLiteral("ready");
    switch (snapshot_.lifecycle) {
    case OperationLifecycle::Starting:
    case OperationLifecycle::Running:
    case OperationLifecycle::Stopping:
        return QStringLiteral("running");
    case OperationLifecycle::Paused:
        return QStringLiteral("paused");
    case OperationLifecycle::Completed:
        return QStringLiteral("completed");
    case OperationLifecycle::Interrupted:
    case OperationLifecycle::Failed:
        return QStringLiteral("error");
    case OperationLifecycle::Idle:
        return cameraController_.cameraStatus() == QStringLiteral("Unavailable")
                   ? QStringLiteral("unavailable")
                   : QStringLiteral("ready");
    }
    return QStringLiteral("error");
}

QString LiveSortingController::error() const {
    if (!actionError_.isEmpty())
        return actionError_;
    if (snapshot_.lifecycle == OperationLifecycle::Failed ||
        snapshot_.lifecycle == OperationLifecycle::Interrupted) {
        return snapshot_.diagnostic.isEmpty()
                   ? QStringLiteral("Live Sorting ended: %1.")
                         .arg(snapshot_.stopReason)
                   : snapshot_.diagnostic;
    }
    return {};
}

QString LiveSortingController::diagnostic() const {
    return snapshot_.diagnostic;
}

bool LiveSortingController::cameraStreaming() const {
    return cameraController_.streaming();
}

bool LiveSortingController::startSortingEnabled() const {
    return !activeLifecycle(snapshot_.lifecycle) &&
           preflightError().isEmpty();
}

QString LiveSortingController::runName() const { return runName_; }

void LiveSortingController::setRunName(const QString& value) {
    if (runName_ == value)
        return;
    runName_ = value;
    setActionError({});
    emit changed();
}

QString LiveSortingController::experimentType() const {
    return experimentType_;
}

void LiveSortingController::setExperimentType(const QString& value) {
    if (experimentType_ == value)
        return;
    experimentType_ = value;
    emit changed();
}

QString LiveSortingController::notes() const { return notes_; }

void LiveSortingController::setNotes(const QString& value) {
    if (notes_ == value)
        return;
    notes_ = value;
    emit changed();
}

QString LiveSortingController::duration() const { return duration_; }

void LiveSortingController::setDuration(const QString& value) {
    if (duration_ == value)
        return;
    duration_ = value;
    setActionError({});
    emit changed();
}

QString LiveSortingController::saveLocation() const { return saveLocation_; }

void LiveSortingController::setSaveLocation(const QString& value) {
    if (saveLocation_ == value)
        return;
    saveLocation_ = value;
    setActionError({});
    emit changed();
}

QString LiveSortingController::activeModelText() const {
    return facts_.activeModelName.trimmed().isEmpty()
               ? QStringLiteral("No Active Model")
               : facts_.activeModelName;
}

QStringList LiveSortingController::hitClassOptions() const {
    QStringList values;
    for (const auto& cls : facts_.activeModelClasses)
        values.push_back(cls.name);
    return values;
}

QString LiveSortingController::hitClassId() const { return hitClassId_; }

void LiveSortingController::setHitClassId(const QString& value) {
    if (hitClassId_ == value)
        return;
    hitClassId_ = value;
    setActionError({});
    emit changed();
}

bool LiveSortingController::triggerEveryDroplet() const {
    return triggerEveryDroplet_;
}

void LiveSortingController::setTriggerEveryDroplet(bool value) {
    if (triggerEveryDroplet_ == value)
        return;
    triggerEveryDroplet_ = value;
    setActionError({});
    emit changed();
}

bool LiveSortingController::daqOutputEnabled() const {
    return daqOutputEnabled_;
}

void LiveSortingController::setDaqOutputEnabled(bool value) {
    if (daqOutputEnabled_ == value)
        return;
    daqOutputEnabled_ = value;
    setActionError({});
    emit changed();
}

bool LiveSortingController::recordFullImageSequence() const {
    return recordFullImageSequence_;
}

void LiveSortingController::setRecordFullImageSequence(bool value) {
    if (recordFullImageSequence_ == value)
        return;
    recordFullImageSequence_ = value;
    emit changed();
}

double LiveSortingController::elapsedSeconds() const {
    return snapshot_.elapsedSeconds;
}

qint64 LiveSortingController::persistedEvents() const {
    return snapshot_.persistedEvents;
}

QVariantMap LiveSortingController::integrity() const {
    return {{QStringLiteral("sourceFrameGaps"),
             integritySeries(snapshot_.integrity.sourceFrameGaps)},
            {QStringLiteral("queueRejections"),
             integritySeries(snapshot_.integrity.queueRejections)},
            {QStringLiteral("consumerFailures"),
             integritySeries(snapshot_.integrity.consumerFailures)}};
}

QString LiveSortingController::stopReason() const {
    return snapshot_.stopReason;
}

QString LiveSortingController::runFolder() const { return snapshot_.runFolder; }

bool LiveSortingController::startCamera() {
    setActionError({});
    if (!cameraController_.start()) {
        setActionError(cameraController_.error());
        return false;
    }
    return true;
}

bool LiveSortingController::stopCamera() {
    setActionError({});
    if (!cameraController_.stop()) {
        setActionError(cameraController_.error());
        return false;
    }
    return true;
}

bool LiveSortingController::startSorting() {
    refresh();
    const QString blocker = preflightError();
    if (!blocker.isEmpty()) {
        setActionError(blocker);
        return false;
    }
    QString durationError;
    const auto requested = requestedDuration(&durationError);
    if (!durationError.isEmpty()) {
        setActionError(durationError);
        return false;
    }

    LiveSortingRequest request;
    request.outputRoot = QDir::cleanPath(saveLocation_);
    request.runName = runName_;
    request.experimentType = experimentType_;
    request.notes = notes_;
    request.triggerMode = triggerEveryDroplet_
                              ? run::TriggerMode::EveryDroplet
                              : run::TriggerMode::ClassBased;
    if (!triggerEveryDroplet_)
        request.hitClassId = hitClassId_;
    request.hitBoundary = facts_.hitBoundary;
    request.requestedDurationSeconds = requested;
    request.useActiveModel =
        !triggerEveryDroplet_ || facts_.activeModelLoadable;
    request.opendssVersion = facts_.opendssVersion;
    request.detectorSettings = facts_.detectorSettings;
    request.cropSettings = facts_.cropSettings;
    request.timingSettings = facts_.timingSettings;
    request.cameraSettings = facts_.cameraSettings;
    request.daqSettings = facts_.daqSettings;
    request.daqOutputEnabled = daqOutputEnabled_;
    request.recordFullImageSequence = recordFullImageSequence_;

    QString serviceError;
    if (!service_.start(request, &serviceError)) {
        setActionError(serviceError);
        updateSnapshot();
        return false;
    }
    actionError_.clear();
    outcomeCleared_ = false;
    resultsNotified_ = false;
    lastDeliveryId_ = 0;
    lastTimestampNs_ = 0;
    droppedFrames_ = 0;
    updateSnapshot();
    return true;
}

bool LiveSortingController::pauseSorting() {
    if (snapshot_.lifecycle != OperationLifecycle::Running)
        return false;
    return requestServiceAction(ServiceAction::Pause);
}

bool LiveSortingController::resumeSorting() {
    if (snapshot_.lifecycle != OperationLifecycle::Paused)
        return false;
    return requestServiceAction(ServiceAction::Resume);
}

bool LiveSortingController::stopSorting() {
    if (!activeLifecycle(snapshot_.lifecycle))
        return false;
    return requestServiceAction(ServiceAction::Stop);
}

bool LiveSortingController::primaryAction() {
    const QString state = presentation();
    if (state == QStringLiteral("running"))
        return pauseSorting();
    if (state == QStringLiteral("paused"))
        return resumeSorting();
    if (state == QStringLiteral("completed")) {
        startNewRun();
        return true;
    }
    if (state != QStringLiteral("ready"))
        return false;
    return cameraStreaming() ? stopCamera() : startCamera();
}

bool LiveSortingController::secondaryAction() {
    const QString state = presentation();
    if (state == QStringLiteral("running") ||
        state == QStringLiteral("paused")) {
        return stopSorting();
    }
    if (state == QStringLiteral("completed")) {
        if (!resultsRefresh_)
            return false;
        resultsRefresh_(snapshot_.runFolder);
        resultsNotified_ = true;
        return true;
    }
    return state == QStringLiteral("ready") && startSorting();
}

void LiveSortingController::startNewRun() {
    if (activeLifecycle(snapshot_.lifecycle))
        return;
    outcomeCleared_ = true;
    actionError_.clear();
    emit changed();
}

void LiveSortingController::refresh() {
    if (factsProvider_)
        facts_ = factsProvider_();
    if (saveLocation_.trimmed().isEmpty())
        saveLocation_ = QDir::cleanPath(facts_.defaultRunRoot);
    updateSnapshot();
}

QString LiveSortingController::preflightError() const {
    if (!cameraController_.streaming())
        return QStringLiteral("Start Camera.");
    if (facts_.opendssVersion.trimmed().isEmpty())
        return QStringLiteral("OpenDSS version is unavailable.");
    const QFileInfo output(QDir::cleanPath(saveLocation_));
    if (!output.isDir() || !output.isWritable())
        return QStringLiteral("The save location must be a writable directory.");
    QString durationError;
    requestedDuration(&durationError);
    if (!durationError.isEmpty())
        return durationError;
    if (!validBoundary(facts_.hitBoundary))
        return QStringLiteral("Hit boundary calibration is required.");
    if (!triggerEveryDroplet_) {
        if (!facts_.activeModelLoadable)
            return QStringLiteral("Class-Based Sorting requires an Active Model.");
        if (hitClassId_.trimmed().isEmpty())
            return QStringLiteral("Select a Hit Class.");
        bool found = false;
        for (const auto& cls : facts_.activeModelClasses)
            found = found || cls.id == hitClassId_;
        if (!found)
            return QStringLiteral("The selected Hit Class is unavailable.");
    }
    if (daqOutputEnabled_) {
        QString reason;
        if (!daqReadiness_ || !daqReadiness_(&reason)) {
            return reason.trimmed().isEmpty()
                       ? QStringLiteral("DAQ is not ready.")
                       : reason;
        }
    }
    return {};
}

std::optional<double>
LiveSortingController::requestedDuration(QString* error) const {
    if (error)
        error->clear();
    const QString value = duration_.trimmed();
    if (value.isEmpty())
        return std::nullopt;
    bool converted = false;
    const double seconds = value.toDouble(&converted);
    if (!converted || !std::isfinite(seconds) || seconds <= 0.0) {
        if (error)
            *error = QStringLiteral("Duration must be a positive number of seconds.");
        return std::nullopt;
    }
    return seconds;
}

void LiveSortingController::acceptFrame(CameraFrame frame) {
    if (service_.snapshot().lifecycle != OperationLifecycle::Running)
        return;
    QString conversionError;
    QImage image = convertCameraFrame(frame, &conversionError);

    double fps = facts_.nominalCameraFps;
    if (lastTimestampNs_ > 0 &&
        frame.monotonicTimestampNs > lastTimestampNs_) {
        fps = 1'000'000'000.0 /
              static_cast<double>(frame.monotonicTimestampNs -
                                  lastTimestampNs_);
    }
    if (!std::isfinite(fps) || fps <= 0.0)
        fps = 0.0;
    if (lastDeliveryId_ > 0 && frame.deliveryId > lastDeliveryId_ + 1)
        droppedFrames_ += static_cast<qint64>(frame.deliveryId - lastDeliveryId_ - 1);

    FrameMeta meta;
    meta.width = frame.width;
    meta.height = frame.height;
    meta.bits = frame.bitDepth;
    meta.binning = 1.0;
    meta.frameIndex = static_cast<qint64>(frame.deliveryId);
    meta.delivered = static_cast<qint64>(frame.deliveryId);
    meta.dropped = droppedFrames_;
    meta.internalFps = fps;
    lastDeliveryId_ = frame.deliveryId;
    lastTimestampNs_ = frame.monotonicTimestampNs;

    if (image.isNull()) {
        setActionError(conversionError);
        service_.offerFrame({}, meta, fps);
    } else {
        service_.offerFrame(image, meta, fps);
    }
    updateSnapshot();
}

bool LiveSortingController::requestServiceAction(ServiceAction action) {
    {
        std::lock_guard lock(actionMutex_);
        if (actionWorkerStopping_ || actionInProgress_ || pendingAction_)
            return false;
        pendingAction_ = action;
        actionInProgress_ = true;
        actionReady_.notify_one();
    }
    actionError_.clear();
    emit changed();
    return true;
}

void LiveSortingController::serviceActionLoop() {
    for (;;) {
        ServiceAction action = ServiceAction::PollDuration;
        {
            std::unique_lock lock(actionMutex_);
            actionReady_.wait(
                lock, [this] { return actionWorkerStopping_ || pendingAction_; });
            if (actionWorkerStopping_) {
                pendingAction_.reset();
                lock.unlock();
                if (activeLifecycle(service_.snapshot().lifecycle)) {
                    QString ignored;
                    service_.stop(&ignored);
                }
                return;
            }
            action = *pendingAction_;
            pendingAction_.reset();
        }

        QString serviceError;
        bool succeeded = false;
        switch (action) {
        case ServiceAction::Pause:
            succeeded = service_.pause(&serviceError);
            break;
        case ServiceAction::Resume:
            succeeded = service_.resume(&serviceError);
            break;
        case ServiceAction::Stop:
            succeeded = service_.stop(&serviceError);
            break;
        case ServiceAction::PollDuration:
            service_.pollDuration(&serviceError);
            succeeded = serviceError.isEmpty();
            break;
        }
        const LiveSortingSnapshot completedSnapshot = service_.snapshot();
        std::lock_guard lock(actionMutex_);
        if (!actionWorkerStopping_) {
            QMetaObject::invokeMethod(
                this,
                [this, action, succeeded, serviceError, completedSnapshot] {
                    completeServiceAction(action, succeeded, serviceError,
                                          completedSnapshot);
                },
                Qt::QueuedConnection);
        }
    }
}

void LiveSortingController::completeServiceAction(
    ServiceAction action, bool succeeded, const QString& error,
    const LiveSortingSnapshot& completedSnapshot) {
    actionInProgress_ = false;
    if (!succeeded)
        actionError_ = error;
    else if (action != ServiceAction::PollDuration)
        actionError_.clear();
    if (succeeded && action == ServiceAction::Resume) {
        lastDeliveryId_ = 0;
        lastTimestampNs_ = 0;
    }
    projectSnapshot(completedSnapshot);
}

void LiveSortingController::updateSnapshot() {
    projectSnapshot(service_.snapshot());
}

void LiveSortingController::projectSnapshot(
    const LiveSortingSnapshot& completedSnapshot) {
    const OperationLifecycle previous = snapshot_.lifecycle;
    snapshot_ = completedSnapshot;
    if ((snapshot_.lifecycle == OperationLifecycle::Completed ||
         snapshot_.lifecycle == OperationLifecycle::Interrupted ||
         snapshot_.lifecycle == OperationLifecycle::Failed) &&
        activeLifecycle(previous) && !resultsNotified_) {
        if (resultsRefresh_)
            resultsRefresh_(snapshot_.runFolder);
        resultsNotified_ = true;
    }
    emit changed();
}

void LiveSortingController::setActionError(const QString& value) {
    if (actionError_ == value)
        return;
    actionError_ = value;
    emit changed();
}

} // namespace desktop_app::v2::live
