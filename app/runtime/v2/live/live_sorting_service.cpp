#include "live_sorting_service.h"

#include "../decision/decision_service.h"
#include "../model/model_load_service.h"
#include "../routing/observed_route_tracker.h"
#include "../run/run_writer_v2.h"
#include "../../crops/crop_service.h"
#include "../../desktop_app/live_frame_dispatcher.h"
#include "../../detection/droplet_detector.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

namespace desktop_app::v2::live {
namespace {

void setError(QString* output, const QString& value) {
    if (output)
        *output = value;
}

QString cleanName(QString value) {
    value = QFileInfo(value.replace('\\', '/')).fileName().trimmed();
    value.replace(QRegularExpression(R"([^\p{L}\p{N} _.-])"), "_");
    value.remove(QRegularExpression(R"(^[ ._]+|[ ._]+$)"));
    return value.isEmpty()
               ? QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
               : value;
}

QString uniqueRunFolder(const QString& root, const QString& requestedName) {
    const QString name = cleanName(requestedName);
    QDir directory(root);
    for (int suffix = 1;; ++suffix) {
        const QString leaf = suffix == 1 ? name : name + "-" + QString::number(suffix);
        const QString path = directory.absoluteFilePath(leaf);
        if (!QFileInfo::exists(path))
            return path;
    }
}

QByteArray pngBytes(const cv::Mat& image, QString* error) {
    QImage view(image.data, image.cols, image.rows, image.step, QImage::Format_Grayscale8);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Could not open the Droplet Crop buffer."));
        return {};
    }
    QImageWriter writer(&buffer, "PNG");
    if (!writer.write(view)) {
        setError(error, writer.errorString());
        return {};
    }
    return bytes;
}

std::optional<QString> metadataModelName(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not read verified model metadata."));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QString name = document.object().value(QStringLiteral("model_name")).toString().trimmed();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        name.isEmpty()) {
        setError(error, QStringLiteral("Verified model metadata has no valid model_name."));
        return std::nullopt;
    }
    return name;
}

std::optional<PreparedLiveModel> prepareProductionModel(ModelLoadService& loader,
                                                        QString* error) {
    QString warning;
    auto adapter = loader.preparePersistedActive(QStringLiteral("cpu"), &warning, error);
    if (!adapter)
        return std::nullopt;
    const auto modelName =
        metadataModelName(QString::fromStdString(adapter->metadataPath()), error);
    if (!modelName)
        return std::nullopt;
    const Metadata& metadata = adapter->metadata();
    if ((metadata.classes.size() != 2 && metadata.classes.size() != 3) ||
        metadata.displayLabels.size() != metadata.classes.size()) {
        setError(error, QStringLiteral("Verified model classes and display labels are invalid."));
        return std::nullopt;
    }
    PreparedLiveModel prepared;
    prepared.snapshot.id = QString::fromStdString(adapter->modelId());
    prepared.snapshot.name = *modelName;
    prepared.snapshot.sha256 = QString::fromStdString(adapter->declaredOnnxSha256());
    for (std::size_t index = 0; index < metadata.classes.size(); ++index) {
        prepared.snapshot.classes.push_back(
            {QString::fromStdString(metadata.classes[index]),
             QString::fromStdString(metadata.displayLabels[index])});
    }
    auto shared = std::shared_ptr<OnnxInferenceAdapter>(std::move(adapter));
    prepared.classify =
        [shared](const cv::Mat& crop,
                 QString* outputError) -> std::optional<LiveInferenceResult> {
        try {
            const ClassificationResult result = shared->classify(crop);
            LiveInferenceResult output;
            output.scores.reserve(static_cast<qsizetype>(result.scores.size()));
            for (const float score : result.scores)
                output.scores.push_back(score);
            return output;
        } catch (const std::exception& exception) {
            setError(outputError,
                     QStringLiteral("Model inference failed: %1").arg(exception.what()));
        } catch (...) {
            setError(outputError, QStringLiteral("Model inference failed."));
        }
        return std::nullopt;
    };
    return prepared;
}

bool validModel(const PreparedLiveModel& model) {
    if (!model.classify || model.snapshot.id.trimmed().isEmpty() ||
        model.snapshot.name.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$"))
             .match(model.snapshot.sha256)
             .hasMatch() ||
        (model.snapshot.classes.size() != 2 &&
         model.snapshot.classes.size() != 3)) {
        return false;
    }
    for (const auto& cls : model.snapshot.classes) {
        if (cls.id.trimmed().isEmpty() || cls.name.trimmed().isEmpty())
            return false;
    }
    return true;
}

void addRange(run::RunIntegritySeries& series, qint64 first, qint64 last) {
    if (!series.ranges.isEmpty() &&
        first <= series.ranges.last().last + 1) {
        const qint64 oldLast = series.ranges.last().last;
        series.ranges.last().last = (std::max)(oldLast, last);
        series.count += series.ranges.last().last - oldLast;
    } else {
        series.ranges.push_back({first, last});
        series.count += last - first + 1;
    }
}

struct PersistenceItem {
    run::RunEvent event;
    QByteArray cropBytes;
};

struct PendingEvent {
    run::RunEvent event;
    QByteArray cropBytes;
    routing::ObservedRouteTracker route;

    explicit PendingEvent(run::HitBoundarySnapshot boundary)
        : route(std::move(boundary)) {}
};

class ConsumerFault final {};

} // namespace

class LiveSortingService::Impl final {
public:
    Impl(OperationCoordinator& operations, IDropletDetector& detector,
         ModelLoadService* modelLoader, HitPulseCallback pulse,
         LiveModelProvider modelProvider, PersistenceGate persistenceGate)
        : operations(operations),
          detector(detector),
          modelLoader(modelLoader),
          pulse(std::move(pulse)),
          modelProvider(std::move(modelProvider)),
          persistenceGate(std::move(persistenceGate)) {}

    ~Impl() {
        QString ignored;
        finish(run::RunStatus::Interrupted, QStringLiteral("service_destroyed"), &ignored);
    }

    bool start(const LiveSortingRequest& value, QString* error) {
        setError(error, {});
        {
            std::lock_guard lock(stateMutex);
            if (lifecycle == OperationLifecycle::Starting ||
                lifecycle == OperationLifecycle::Running ||
                lifecycle == OperationLifecycle::Paused ||
                lifecycle == OperationLifecycle::Stopping) {
                setError(error, QStringLiteral("Live Sorting is already active."));
                return false;
            }
            lifecycle = OperationLifecycle::Idle;
        }
        if (!pulse) {
            setError(error, QStringLiteral("Live Sorting requires a Hit pulse callback."));
            return false;
        }
        if (value.triggerMode == run::TriggerMode::ClassBased &&
            (!value.useActiveModel || !value.hitClassId ||
             value.hitClassId->trimmed().isEmpty())) {
            setError(error,
                     QStringLiteral("Class-Based Sorting requires the Active Model and Hit Class."));
            return false;
        }
        if (value.triggerMode == run::TriggerMode::EveryDroplet &&
            value.hitClassId) {
            setError(error, QStringLiteral("Trigger Every Droplet does not use a Hit Class."));
            return false;
        }
        if (value.requestedDurationSeconds &&
            (!std::isfinite(*value.requestedDurationSeconds) ||
             *value.requestedDurationSeconds <= 0.0)) {
            setError(error, QStringLiteral("Duration must be finite and positive."));
            return false;
        }
        const QFileInfo outputRoot(value.outputRoot);
        if (!outputRoot.isDir() || !outputRoot.isWritable()) {
            setError(error, QStringLiteral("The output root must be a writable directory."));
            return false;
        }
        if (!std::isfinite(value.hitBoundary.boundaryY) ||
            value.hitBoundary.boundaryY < 0.0 ||
            value.hitBoundary.imageWidth <= 0 ||
            value.hitBoundary.imageHeight <= 0 ||
            value.hitBoundary.boundaryY >= value.hitBoundary.imageHeight) {
            setError(error, QStringLiteral("Hit boundary snapshot is invalid."));
            return false;
        }

        const bool useModel =
            value.triggerMode == run::TriggerMode::ClassBased || value.useActiveModel;
        ResourceLocks locks = ResourceLock::Camera | ResourceLock::Daq |
                              ResourceLock::Run | ResourceLock::Storage;
        if (useModel)
            locks |= ResourceLock::Model;
        auto acquired = operations.acquire(OperationKind::LiveSorting, locks);
        if (!acquired.acquired()) {
            setError(error, acquired.fault ? acquired.fault->reason
                                          : QStringLiteral("Live Sorting resources are in use."));
            return false;
        }
        lease = std::move(acquired.lease);

        QString localError;
        std::optional<PreparedLiveModel> prepared;
        if (useModel) {
            prepared = modelProvider
                           ? modelProvider(&localError)
                           : (modelLoader ? prepareProductionModel(*modelLoader, &localError)
                                          : std::nullopt);
            if (!prepared || !validModel(*prepared)) {
                lease.transition(OperationLifecycle::Failed);
                lease.release();
                setError(error, localError.isEmpty()
                                    ? QStringLiteral("The Active Model is unavailable or invalid.")
                                    : localError);
                return false;
            }
            if (value.hitClassId &&
                std::none_of(prepared->snapshot.classes.begin(),
                             prepared->snapshot.classes.end(),
                             [&](const run::RunClassSnapshot& cls) {
                                 return cls.id == *value.hitClassId;
                             })) {
                lease.transition(OperationLifecycle::Failed);
                lease.release();
                setError(error,
                         QStringLiteral("Hit Class is not present in the Active Model."));
                return false;
            }
        }

        const QString folder = uniqueRunFolder(value.outputRoot, value.runName);
        run::RunManifestData data;
        data.runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        data.runName = cleanName(value.runName);
        data.operation = run::RunOperation::LiveSorting;
        data.experimentType = value.experimentType;
        data.notes = value.notes;
        data.startedAt =
            QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        data.stopReason = QStringLiteral("operation_in_progress");
        data.opendssVersion = value.opendssVersion;
        data.requestedDurationSeconds = value.requestedDurationSeconds;
        if (prepared)
            data.model = prepared->snapshot;
        data.routing = {value.triggerMode, value.hitClassId, true};
        data.cameraSettings = value.cameraSettings;
        data.detectorSettings = value.detectorSettings;
        data.cropSettings = value.cropSettings;
        data.daqSettings = value.daqSettings;
        data.timingSettings = value.timingSettings;
        data.hitBoundary = value.hitBoundary;
        writer = run::RunWriterV2::start(folder, data, &localError);
        if (!writer) {
            lease.transition(OperationLifecycle::Failed);
            lease.release();
            setError(error, localError);
            return false;
        }

        request = value;
        model = std::move(prepared);
        runFolder = folder;
        detector.reset();
        pending.reset();
        eventNumber = 0;
        persistedEvents.store(0);
        fatal.store(false);
        persistenceStopping = false;
        persistenceWriting = false;
        integrity = {};
        diagnostic.clear();
        haveDelivered = false;
        elapsedBeforeCurrentRun = 0.0;
        activeElapsed.start();
        try {
            persistenceWorker = std::thread([this] { persistenceLoop(); });
            createDispatcher();
        } catch (...) {
            persistenceStopping = true;
            persistenceReady.notify_all();
            if (persistenceWorker.joinable())
                persistenceWorker.join();
            syncWriterIntegrity();
            writer->finalize(run::RunStatus::Failed,
                             QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                             QStringLiteral("worker_start_failed"), 0.0, nullptr);
            writer.reset();
            lease.transition(OperationLifecycle::Failed);
            lease.release();
            setError(error, QStringLiteral("Could not start Live Sorting workers."));
            return false;
        }
        if (!lease.transition(OperationLifecycle::Running)) {
            setError(error, QStringLiteral("Live Sorting could not enter Running state."));
            finish(run::RunStatus::Failed, QStringLiteral("lifecycle_failed"), nullptr);
            return false;
        }
        {
            std::lock_guard lock(stateMutex);
            lifecycle = OperationLifecycle::Running;
        }
        return true;
    }

    bool offerFrame(const QImage& image, const FrameMeta& meta, double fps) {
        if (fatal.load(std::memory_order_acquire)) {
            finish(run::RunStatus::Failed, QStringLiteral("processing_fault"), nullptr);
            return false;
        }
        std::lock_guard lock(stateMutex);
        if (lifecycle != OperationLifecycle::Running || !dispatcher)
            return false;
        const qint64 sourceIndex = frameIndex(meta);
        if (haveDelivered && meta.delivered > lastDelivered + 1) {
            const qint64 gapCount = meta.delivered - lastDelivered - 1;
            const qint64 firstMissing =
                sourceIndex > gapCount ? sourceIndex - gapCount
                                       : lastDelivered + 1;
            const qint64 lastMissing =
                sourceIndex > gapCount ? sourceIndex - 1
                                       : meta.delivered - 1;
            addRange(integrity.sourceFrameGaps, firstMissing, lastMissing);
            qWarning().noquote()
                << "Live Sorting source frame gap:"
                << firstMissing << "-" << lastMissing;
        }
        if (meta.delivered > 0) {
            lastDelivered = meta.delivered;
            haveDelivered = true;
        }
        LiveFrameDispatcher::Membership membership;
        membership.liveLogging = true;
        const auto result = dispatcher->offer(image, meta, fps, membership);
        if (!result.accepted) {
            if (sourceIndex > 0) {
                addRange(integrity.queueRejections, sourceIndex, sourceIndex);
                qWarning().noquote()
                    << "Live Sorting frame handoff rejected at source frame"
                    << sourceIndex;
            }
            diagnostic = QStringLiteral("A Live frame was rejected by the bounded handoff queue.");
        }
        return result.accepted;
    }

    bool pause(QString* error) {
        setError(error, {});
        std::unique_ptr<LiveFrameDispatcher> toDrain;
        {
            std::lock_guard lock(stateMutex);
            if (lifecycle != OperationLifecycle::Running) {
                setError(error, QStringLiteral("Live Sorting is not running."));
                return false;
            }
            lifecycle = OperationLifecycle::Paused;
            elapsedBeforeCurrentRun +=
                static_cast<double>(activeElapsed.nsecsElapsed()) / 1'000'000'000.0;
            toDrain = std::move(dispatcher);
        }
        if (!lease.transition(OperationLifecycle::Paused)) {
            setError(error, QStringLiteral("Live Sorting could not enter Paused state."));
            return false;
        }
        toDrain->stopAndDrain();
        finalizePending();
        drainPersistence();
        syncWriterIntegrity();
        QString localError;
        if (!writer->flush(&localError)) {
            recordConsumerFailure(positiveFrameIndex());
            diagnostic = localError;
            qWarning().noquote() << "Live Sorting pause flush failed:" << localError;
        }
        if (fatal.load(std::memory_order_acquire)) {
            setError(error, diagnostic);
            return finish(run::RunStatus::Failed,
                          QStringLiteral("processing_fault"), error);
        }
        return true;
    }

    bool resume(QString* error) {
        setError(error, {});
        {
            std::lock_guard lock(stateMutex);
            if (lifecycle != OperationLifecycle::Paused) {
                setError(error, QStringLiteral("Live Sorting is not paused."));
                return false;
            }
            createDispatcher();
            activeElapsed.restart();
            haveDelivered = false;
            lifecycle = OperationLifecycle::Running;
        }
        if (!lease.transition(OperationLifecycle::Running)) {
            setError(error, QStringLiteral("Live Sorting could not resume."));
            return false;
        }
        return true;
    }

    bool stop(QString* error) {
        return finish(run::RunStatus::Stopped, QStringLiteral("user"), error);
    }

    bool pollDuration(QString* error) {
        setError(error, {});
        if (fatal.load(std::memory_order_acquire))
            return finish(run::RunStatus::Failed,
                          QStringLiteral("processing_fault"), error);
        std::optional<double> duration;
        double elapsed = 0.0;
        {
            std::lock_guard lock(stateMutex);
            if (lifecycle != OperationLifecycle::Running &&
                lifecycle != OperationLifecycle::Paused)
                return false;
            duration = request.requestedDurationSeconds;
            elapsed = elapsedSecondsLocked();
        }
        if (!duration || elapsed < *duration)
            return false;
        return finish(run::RunStatus::Completed, QStringLiteral("duration"), error);
    }

    LiveSortingSnapshot snapshot() const {
        std::lock_guard lock(stateMutex);
        return {lifecycle, runFolder, elapsedSecondsLocked(),
                persistedEvents.load(std::memory_order_acquire), integrity, diagnostic};
    }

private:
    static qint64 frameIndex(const FrameMeta& meta) {
        return meta.frameIndex > 0 ? meta.frameIndex : meta.delivered;
    }

    qint64 positiveFrameIndex() const {
        std::lock_guard lock(stateMutex);
        return lastDelivered > 0 ? lastDelivered : 1;
    }

    double elapsedSecondsLocked() const {
        if (lifecycle == OperationLifecycle::Running && activeElapsed.isValid()) {
            return elapsedBeforeCurrentRun +
                   static_cast<double>(activeElapsed.nsecsElapsed()) /
                       1'000'000'000.0;
        }
        return elapsedBeforeCurrentRun;
    }

    void createDispatcher() {
        dispatcher = std::make_unique<LiveFrameDispatcher>(
            [this](const QImage& image, const FrameMeta& meta, double,
                   std::uint64_t, LiveFrameDispatcher::Membership) {
                try {
                    consumeFrame(image, meta);
                } catch (const ConsumerFault&) {
                    throw;
                } catch (const std::exception& exception) {
                    consumerFault(frameIndex(meta),
                                  QStringLiteral("Live frame processing failed: %1")
                                      .arg(exception.what()));
                } catch (...) {
                    consumerFault(frameIndex(meta),
                                  QStringLiteral("Live frame processing failed."));
                }
            });
    }

    [[noreturn]] void consumerFault(qint64 sourceIndex, const QString& reason) {
        recordConsumerFailure(sourceIndex > 0 ? sourceIndex : 1);
        {
            std::lock_guard lock(stateMutex);
            diagnostic = reason;
        }
        fatal.store(true, std::memory_order_release);
        qWarning().noquote() << reason << "source frame" << sourceIndex;
        throw ConsumerFault{};
    }

    void consumeFrame(const QImage& supplied, const FrameMeta& meta) {
        if (fatal.load(std::memory_order_acquire))
            throw ConsumerFault{};
        if (supplied.isNull())
            consumerFault(frameIndex(meta), QStringLiteral("Live frame is empty."));
        QImage image = supplied.convertToFormat(QImage::Format_Grayscale8);
        if (image.width() != request.hitBoundary.imageWidth ||
            image.height() != request.hitBoundary.imageHeight) {
            consumerFault(frameIndex(meta),
                          QStringLiteral("Live frame dimensions do not match the Run."));
        }
        cv::Mat frame(image.height(), image.width(), CV_8UC1, image.bits(),
                      image.bytesPerLine());
        const DropletDetectionFrame detection = detector.processFrame(frame);
        QString localError;

        if (detection.eventEntered) {
            finalizePending();
            desktop_app::DatasetCrop crop;
            if (!desktop_app::CropService::makeDatasetCrop(
                    frame, detection.bbox, &crop, &localError)) {
                consumerFault(frameIndex(meta), localError);
            }
            pending.emplace(request.hitBoundary);
            ++eventNumber;
            pending->event.eventId =
                QStringLiteral("event_%1").arg(eventNumber, 6, 10, QLatin1Char('0'));
            pending->event.detectionTimestamp =
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            pending->event.sourceFrameIndex = frameIndex(meta);
            pending->event.cropPath =
                QStringLiteral("crops/droplet_%1.png")
                    .arg(eventNumber, 6, 10, QLatin1Char('0'));
            pending->cropBytes = pngBytes(crop.image, &localError);
            if (pending->cropBytes.isEmpty())
                consumerFault(frameIndex(meta), localError);

            if (model) {
                QElapsedTimer inferenceTimer;
                inferenceTimer.start();
                auto result = model->classify(crop.image, &localError);
                const double inferenceMs =
                    static_cast<double>(inferenceTimer.nsecsElapsed()) / 1'000'000.0;
                if (!result ||
                    result->scores.size() != model->snapshot.classes.size() ||
                    std::any_of(result->scores.begin(), result->scores.end(),
                                [](double score) { return !std::isfinite(score); })) {
                    pending.reset();
                    consumerFault(frameIndex(meta),
                                  localError.isEmpty()
                                      ? QStringLiteral("Model inference result is invalid.")
                                      : localError);
                }
                int bestIndex = 0;
                for (int index = 1; index < result->scores.size(); ++index) {
                    if (result->scores.at(index) >
                        result->scores.at(bestIndex))
                        bestIndex = index;
                }
                pending->event.predictedClassId =
                    model->snapshot.classes.at(bestIndex).id;
                pending->event.scores = result->scores;
                pending->event.inferenceTimeMs = inferenceMs;
            }
            const auto decision = decision::DecisionService::decide(
                request.triggerMode, pending->event.predictedClassId,
                request.hitClassId, &localError);
            if (!decision) {
                pending.reset();
                consumerFault(frameIndex(meta), localError);
            }
            pending->event.decision = *decision;
            if (*decision == run::Route::Waste) {
                pending->event.daqPulseStatus = run::DaqPulseStatus::NotRequested;
            } else {
                const auto pulseStatus = pulse(&localError);
                if (pulseStatus != run::DaqPulseStatus::Issued &&
                    pulseStatus != run::DaqPulseStatus::SuppressedNotIssued &&
                    pulseStatus != run::DaqPulseStatus::Failed) {
                    pending.reset();
                    consumerFault(frameIndex(meta),
                                  QStringLiteral("Hit pulse callback returned an invalid status."));
                }
                pending->event.daqPulseStatus = pulseStatus;
                if (pulseStatus == run::DaqPulseStatus::Failed) {
                    {
                        std::lock_guard lock(stateMutex);
                        diagnostic =
                            localError.isEmpty()
                                ? QStringLiteral("The Live Hit pulse failed.")
                                : localError;
                    }
                    fatal.store(true, std::memory_order_release);
                }
            }
        } else if (!detection.detected) {
            finalizePending();
        }
        if (pending && detection.detected)
            pending->route.addSample(detection.centroid.y);
        if (fatal.load(std::memory_order_acquire))
            throw ConsumerFault{};
    }

    void finalizePending() {
        if (!pending)
            return;
        pending->event.observedRoute = pending->route.finalize();
        enqueue({std::move(pending->event), std::move(pending->cropBytes)});
        pending.reset();
    }

    bool enqueue(PersistenceItem item) {
        std::lock_guard lock(persistenceMutex);
        if (persistenceQueue.size() == PersistenceCapacity) {
            recordQueueRejectionLocked(item.event.sourceFrameIndex);
            qWarning().noquote()
                << "Live Sorting persistence queue rejected source frame"
                << item.event.sourceFrameIndex;
            return false;
        }
        persistenceQueue.push_back(std::move(item));
        persistenceReady.notify_one();
        return true;
    }

    void recordQueueRejectionLocked(qint64 sourceIndex) {
        std::lock_guard stateLock(stateMutex);
        addRange(integrity.queueRejections, sourceIndex, sourceIndex);
        diagnostic =
            QStringLiteral("A completed Live event was rejected by the persistence queue.");
    }

    void recordConsumerFailure(qint64 sourceIndex) {
        std::lock_guard lock(stateMutex);
        addRange(integrity.consumerFailures, sourceIndex, sourceIndex);
    }

    void syncWriterIntegrity() {
        if (!writer)
            return;
        std::lock_guard lock(stateMutex);
        const_cast<run::RunManifestData&>(writer->data()).integrity = integrity;
    }

    void persistenceLoop() {
        for (;;) {
            PersistenceItem item;
            {
                std::unique_lock lock(persistenceMutex);
                persistenceReady.wait(lock, [this] {
                    return persistenceStopping || !persistenceQueue.empty();
                });
                if (persistenceQueue.empty() && persistenceStopping)
                    break;
                item = std::move(persistenceQueue.front());
                persistenceQueue.pop_front();
                persistenceWriting = true;
            }

            QString localError;
            bool accepted = !persistenceGate || persistenceGate(&localError);
            if (accepted) {
                syncWriterIntegrity();
                accepted =
                    writer->appendEvent(item.event, item.cropBytes, &localError);
            }
            if (!accepted) {
                recordConsumerFailure(item.event.sourceFrameIndex);
                {
                    std::lock_guard lock(stateMutex);
                    diagnostic =
                        localError.isEmpty()
                            ? QStringLiteral("A completed Live event could not be persisted.")
                            : localError;
                }
                qWarning().noquote()
                    << "Live Sorting event persistence loss at source frame"
                    << item.event.sourceFrameIndex << ":" << localError;
            } else {
                persistedEvents.fetch_add(1, std::memory_order_release);
            }
            {
                std::lock_guard lock(persistenceMutex);
                persistenceWriting = false;
                persistenceDrained.notify_all();
            }
        }
        std::lock_guard lock(persistenceMutex);
        persistenceDrained.notify_all();
    }

    void drainPersistence() {
        std::unique_lock lock(persistenceMutex);
        persistenceDrained.wait(lock, [this] {
            return persistenceQueue.empty() && !persistenceWriting;
        });
    }

    bool finish(run::RunStatus requestedStatus, const QString& reason,
                QString* error) {
        setError(error, {});
        std::unique_ptr<LiveFrameDispatcher> toDrain;
        {
            std::lock_guard lock(stateMutex);
            if (lifecycle == OperationLifecycle::Idle ||
                lifecycle == OperationLifecycle::Completed ||
                lifecycle == OperationLifecycle::Interrupted ||
                lifecycle == OperationLifecycle::Failed) {
                return false;
            }
            if (lifecycle == OperationLifecycle::Running) {
                elapsedBeforeCurrentRun +=
                    static_cast<double>(activeElapsed.nsecsElapsed()) /
                    1'000'000'000.0;
            }
            lifecycle = OperationLifecycle::Stopping;
            toDrain = std::move(dispatcher);
        }
        lease.transition(OperationLifecycle::Stopping);
        if (toDrain)
            toDrain->stopAndDrain();
        finalizePending();
        drainPersistence();
        {
            std::lock_guard lock(persistenceMutex);
            persistenceStopping = true;
            persistenceReady.notify_all();
        }
        if (persistenceWorker.joinable())
            persistenceWorker.join();

        run::RunStatus status = requestedStatus;
        QString stopReason = reason;
        {
            std::lock_guard lock(stateMutex);
            if (fatal.load(std::memory_order_acquire) ||
                integrity.queueRejections.count > 0 ||
                integrity.consumerFailures.count > 0) {
                status = run::RunStatus::Failed;
                stopReason = QStringLiteral("event_integrity_loss");
            } else if (integrity.sourceFrameGaps.count > 0) {
                status = run::RunStatus::Interrupted;
                stopReason = QStringLiteral("source_frame_gap");
            }
        }
        syncWriterIntegrity();
        QString localError;
        const bool finalized =
            writer && writer->finalize(
                          status,
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                          stopReason, 0.0, &localError);
        if (!finalized) {
            status = run::RunStatus::Failed;
            if (localError.isEmpty())
                localError = QStringLiteral("Live Run finalization failed.");
        }
        writer.reset();

        OperationLifecycle finalLifecycle = OperationLifecycle::Failed;
        if (finalized) {
            if (status == run::RunStatus::Completed ||
                status == run::RunStatus::Stopped)
                finalLifecycle = OperationLifecycle::Completed;
            else if (status == run::RunStatus::Interrupted)
                finalLifecycle = OperationLifecycle::Interrupted;
        }
        lease.transition(finalLifecycle);
        lease.release();
        {
            std::lock_guard lock(stateMutex);
            lifecycle = finalLifecycle;
            if (!localError.isEmpty())
                diagnostic = localError;
        }
        if (!finalized)
            setError(error, localError);
        return finalized;
    }

    static constexpr std::size_t PersistenceCapacity = 16;

    OperationCoordinator& operations;
    IDropletDetector& detector;
    ModelLoadService* modelLoader = nullptr;
    HitPulseCallback pulse;
    LiveModelProvider modelProvider;
    PersistenceGate persistenceGate;

    mutable std::mutex stateMutex;
    OperationLifecycle lifecycle = OperationLifecycle::Idle;
    OperationLease lease;
    LiveSortingRequest request;
    std::optional<PreparedLiveModel> model;
    QString runFolder;
    QString diagnostic;
    run::RunIntegrity integrity;
    bool haveDelivered = false;
    qint64 lastDelivered = 0;
    qint64 eventNumber = 0;
    double elapsedBeforeCurrentRun = 0.0;
    QElapsedTimer activeElapsed;
    std::unique_ptr<LiveFrameDispatcher> dispatcher;
    std::optional<PendingEvent> pending;
    std::optional<run::RunWriterV2> writer;
    std::atomic_bool fatal{false};
    std::atomic<qint64> persistedEvents{0};

    std::mutex persistenceMutex;
    std::condition_variable persistenceReady;
    std::condition_variable persistenceDrained;
    std::deque<PersistenceItem> persistenceQueue;
    bool persistenceStopping = false;
    bool persistenceWriting = false;
    std::thread persistenceWorker;
};

LiveSortingService::LiveSortingService(OperationCoordinator& operations,
                                       IDropletDetector& detector,
                                       ModelLoadService* modelLoader,
                                       HitPulseCallback pulse,
                                       LiveModelProvider modelProvider,
                                       PersistenceGate persistenceGate)
    : impl_(std::make_unique<Impl>(operations, detector, modelLoader,
                                  std::move(pulse), std::move(modelProvider),
                                  std::move(persistenceGate))) {}

LiveSortingService::~LiveSortingService() = default;

bool LiveSortingService::start(const LiveSortingRequest& request, QString* error) {
    return impl_->start(request, error);
}

bool LiveSortingService::offerFrame(const QImage& image, const FrameMeta& meta,
                                    double fps) {
    return impl_->offerFrame(image, meta, fps);
}

bool LiveSortingService::pause(QString* error) {
    return impl_->pause(error);
}

bool LiveSortingService::resume(QString* error) {
    return impl_->resume(error);
}

bool LiveSortingService::stop(QString* error) {
    return impl_->stop(error);
}

bool LiveSortingService::pollDuration(QString* error) {
    return impl_->pollDuration(error);
}

LiveSortingSnapshot LiveSortingService::snapshot() const {
    return impl_->snapshot();
}

} // namespace desktop_app::v2::live
