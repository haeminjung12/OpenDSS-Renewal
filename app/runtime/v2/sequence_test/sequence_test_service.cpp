#include "sequence_test_service.h"

#include "../decision/decision_service.h"
#include "../model/model_load_service.h"
#include "../operation/operation_coordinator.h"
#include "../routing/observed_route_tracker.h"
#include "../run/run_writer_v2.h"
#include "../sequence/sequence_manifest_v2.h"
#include "../../crops/crop_service.h"
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
#include <QTemporaryFile>
#include <QUuid>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace desktop_app::v2::sequence_test {
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
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || name.isEmpty()) {
        setError(error, QStringLiteral("Verified model metadata has no valid model_name."));
        return std::nullopt;
    }
    return name;
}

std::optional<sequence::SequenceManifestV2>
loadFrozenManifest(const QByteArray& bytes, QString* error) {
    if (bytes.isEmpty()) {
        setError(error,
                 QStringLiteral("Frozen Sequence manifest bytes are missing."));
        return std::nullopt;
    }
    QTemporaryFile file;
    if (!file.open() || file.write(bytes) != bytes.size() || !file.flush()) {
        setError(error,
                 QStringLiteral("Could not validate the frozen Sequence manifest."));
        return std::nullopt;
    }
    const QString path = file.fileName();
    file.close();
    return sequence::SequenceManifestV2::load(path, error);
}

bool writeExactBytes(const QString& path,
                     const QByteArray& bytes,
                     QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        setError(error,
                 QStringLiteral("Could not create the Run Sequence manifest snapshot."));
        return false;
    }
    qint64 written = 0;
    while (written < bytes.size()) {
        const qint64 count =
            file.write(bytes.constData() + written, bytes.size() - written);
        if (count <= 0) {
            setError(error,
                     QStringLiteral("Could not write the Run Sequence manifest snapshot."));
            return false;
        }
        written += count;
    }
    if (!file.flush()) {
        setError(error,
                 QStringLiteral("Could not finalize the Run Sequence manifest snapshot."));
        return false;
    }
    return true;
}

std::optional<PreparedModel> prepareProductionModel(ModelLoadService& loader, QString* error) {
    QString warning;
    auto adapter = loader.preparePersistedActive(QStringLiteral("cpu"), &warning, error);
    if (!adapter)
        return std::nullopt;
    const auto modelName = metadataModelName(
        QString::fromStdString(adapter->metadataPath()), error);
    if (!modelName)
        return std::nullopt;
    const Metadata& metadata = adapter->metadata();
    if ((metadata.classes.size() != 2 && metadata.classes.size() != 3) ||
        metadata.displayLabels.size() != metadata.classes.size()) {
        setError(error, QStringLiteral("Verified model classes and display labels are invalid."));
        return std::nullopt;
    }
    PreparedModel prepared;
    prepared.snapshot.id = QString::fromStdString(adapter->modelId());
    prepared.snapshot.name = *modelName;
    prepared.snapshot.sha256 = QString::fromStdString(adapter->declaredOnnxSha256());
    for (std::size_t index = 0; index < metadata.classes.size(); ++index) {
        prepared.snapshot.classes.push_back(
            {QString::fromStdString(metadata.classes[index]),
             QString::fromStdString(metadata.displayLabels[index])});
    }
    auto shared = std::shared_ptr<OnnxInferenceAdapter>(std::move(adapter));
    prepared.classify = [shared](const cv::Mat& crop,
                                 QString* outputError) -> std::optional<ModelInferenceResult> {
        try {
            const ClassificationResult result = shared->classify(crop);
            ModelInferenceResult output;
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

bool validModel(const PreparedModel& model) {
    if (!model.classify || model.snapshot.id.trimmed().isEmpty() ||
        model.snapshot.name.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$"))
             .match(model.snapshot.sha256)
             .hasMatch() ||
        (model.snapshot.classes.size() != 2 && model.snapshot.classes.size() != 3)) {
        return false;
    }
    for (const auto& cls : model.snapshot.classes) {
        if (cls.id.trimmed().isEmpty() || cls.name.trimmed().isEmpty())
            return false;
    }
    return true;
}

bool validateLoadedSequence(const LoadedSequence& loaded,
                            const sequence::SequenceManifestData& manifest,
                            const QString& sequenceJson,
                            QString* error) {
    if (QFileInfo(loaded.sourceSequenceJson).canonicalFilePath() !=
        QFileInfo(sequenceJson).canonicalFilePath()) {
        setError(error, QStringLiteral("Loaded Sequence source does not match the selected Sequence."));
        return false;
    }
    if (loaded.sequenceId != manifest.sequenceId) {
        setError(error, QStringLiteral("Loaded Sequence identity does not match sequence.json."));
        return false;
    }
    if (static_cast<qint64>(loaded.frames.size()) != manifest.frameCount) {
        setError(error, QStringLiteral("Loaded Sequence frame count does not match sequence.json."));
        return false;
    }
    for (qsizetype index = 0; index < loaded.frames.size(); ++index) {
        const auto& frame = loaded.frames.at(index);
        if (frame.sourceFrameIndex != index + 1) {
            setError(error, QStringLiteral("Loaded Sequence frame order does not match sequence.json."));
            return false;
        }
        if (frame.image.isNull() || frame.image.width() != manifest.imageWidth ||
            frame.image.height() != manifest.imageHeight) {
            setError(error, QStringLiteral("Loaded Sequence frame dimensions do not match sequence.json."));
            return false;
        }
    }
    return true;
}

void reportProgress(const ProgressCallback& callback,
                    const SequenceTestProgress& progress) {
    if (!callback)
        return;
    try {
        callback(progress);
    } catch (const std::exception& exception) {
        qWarning().noquote()
            << "Sequence Test progress observer failed:" << exception.what();
    } catch (...) {
        qWarning().noquote() << "Sequence Test progress observer failed.";
    }
}

struct PendingEvent {
    run::RunEvent event;
    QByteArray cropBytes;
    routing::ObservedRouteTracker route;

    explicit PendingEvent(run::HitBoundarySnapshot boundary)
        : route(std::move(boundary)) {}
};

class RunningGuard final {
public:
    RunningGuard(std::mutex& mutex, bool& running, bool& acceptingStop)
        : mutex_(mutex), running_(running), acceptingStop_(acceptingStop) {}
    ~RunningGuard() {
        std::lock_guard lock(mutex_);
        acceptingStop_ = false;
        running_ = false;
    }

private:
    std::mutex& mutex_;
    bool& running_;
    bool& acceptingStop_;
};

} // namespace

SequenceTestService::SequenceTestService(OperationCoordinator& operations,
                                         IDropletDetector& detector,
                                         ModelLoadService* modelLoader,
                                         ModelProvider modelProvider,
                                         HitPulseCallback hitPulse,
                                         DaqReadinessGate daqReadinessGate)
    : operations_(operations),
      detector_(detector),
      modelLoader_(modelLoader),
      modelProvider_(std::move(modelProvider)),
      hitPulse_(std::move(hitPulse)),
      daqReadinessGate_(std::move(daqReadinessGate)) {}

void SequenceTestService::requestStop() noexcept {
    std::unique_lock lock(controlMutex_);
    // Calls outside the active run's stop-accepting interval are no-ops.
    if (!running_ || !acceptingStop_)
        return;
    stopRequested_ = true;
    stopChanged_.notify_all();
    // A reserved pulse precedes this stop. Wait for it unless the callback
    // requested the stop itself; no later pulse can reserve after the flag is set.
    if (pulseInFlight_ && pulseThread_ != std::this_thread::get_id())
        pulseFinished_.wait(lock, [&] { return !pulseInFlight_; });
}

bool SequenceTestService::run(const SequenceTestRequest& request, QString* error,
                              QString* completedRunFolder) {
    if (completedRunFolder)
        completedRunFolder->clear();
    setError(error, {});
    {
        std::lock_guard lock(controlMutex_);
        if (running_) {
            setError(error, QStringLiteral("This Sequence Test is already running."));
            return false;
        }
        running_ = true;
        acceptingStop_ = true;
        stopRequested_ = false;
    }
    RunningGuard runningGuard(controlMutex_, running_, acceptingStop_);
    const auto stopRequested = [&] {
        std::lock_guard lock(controlMutex_);
        return stopRequested_;
    };
    std::optional<run::RunWriterV2> writer;
    OperationLease lease;
    QElapsedTimer elapsed;
    qint64 processedFrames = 0;
    double lastProgressAchievedFps = 0.0;
    const auto failFromException = [&](const QString& message) {
        if (writer) {
            try {
                writer->finalize(
                    run::RunStatus::Failed,
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                    QStringLiteral("processing_exception"),
                    lastProgressAchievedFps, nullptr);
            } catch (...) {
            }
        }
        if (lease.isValid())
            lease.transition(OperationLifecycle::Failed);
        setError(error, message);
        return false;
    };

    try {
    if (!std::isfinite(request.requestedProcessingFps) ||
        request.requestedProcessingFps <= 0.0) {
        setError(error, QStringLiteral("Requested Processing FPS must be finite and positive."));
        return false;
    }
    const bool modelRequired = request.triggerMode == run::TriggerMode::ClassBased;
    const bool useModel = modelRequired || request.useActiveModel;
    if (modelRequired && !request.useActiveModel) {
        setError(error, QStringLiteral("Class-Based Sorting requires the Active Model."));
        return false;
    }
    if (request.triggerMode == run::TriggerMode::ClassBased &&
        (!request.hitClassId || request.hitClassId->trimmed().isEmpty())) {
        setError(error, QStringLiteral("Class-Based Sorting requires a Hit Class."));
        return false;
    }
    if (request.triggerMode == run::TriggerMode::EveryDroplet && request.hitClassId) {
        setError(error, QStringLiteral("Trigger Every Droplet does not use a Hit Class."));
        return false;
    }

    const QFileInfo outputRoot(request.outputRoot);
    if (!outputRoot.isDir() || !outputRoot.isWritable()) {
        setError(error, QStringLiteral("The output root must be a writable directory."));
        return false;
    }

    ResourceLocks locks =
        ResourceLock::Sequence | ResourceLock::Run | ResourceLock::Storage;
    if (request.physicalDaqOutputEnabled)
        locks |= ResourceLock::Daq;
    if (useModel)
        locks |= ResourceLock::Model;
    auto acquired = operations_.acquire(OperationKind::SequenceTest, locks);
    if (!acquired.acquired()) {
        setError(error, acquired.fault ? acquired.fault->reason
                                      : QStringLiteral("Sequence Test resources are in use."));
        return false;
    }
    lease = std::move(acquired.lease);

    QString localError;
    if (request.physicalDaqOutputEnabled) {
        bool daqReady = false;
        try {
            daqReady = daqReadinessGate_ && daqReadinessGate_(&localError);
        } catch (const std::exception& exception) {
            localError =
                QStringLiteral("DAQ readiness check failed: %1").arg(exception.what());
        } catch (...) {
            localError = QStringLiteral("DAQ readiness check failed.");
        }
        if (!daqReady || !hitPulse_) {
            lease.transition(OperationLifecycle::Failed);
            setError(error,
                     !daqReady
                         ? (localError.isEmpty() ? QStringLiteral("DAQ is not ready.")
                                                 : localError)
                         : QStringLiteral("DAQ Hit output is not configured."));
            return false;
        }
    }
    if (!lease.transition(OperationLifecycle::Running)) {
        setError(error, QStringLiteral("Sequence Test could not enter Running state."));
        return false;
    }

    auto sequence =
        loadFrozenManifest(request.frozenManifestBytes, &localError);
    if (!sequence) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, localError);
        return false;
    }
    const auto& sequenceData = sequence->data();
    if (!request.loadedSequence) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, QStringLiteral("Sequence is not loaded to memory."));
        return false;
    }
    if (!validateLoadedSequence(*request.loadedSequence, sequenceData,
                                request.sequenceJson, &localError)) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, localError);
        return false;
    }
    if (request.hitBoundary.imageWidth != sequenceData.imageWidth ||
        request.hitBoundary.imageHeight != sequenceData.imageHeight ||
        !std::isfinite(request.hitBoundary.boundaryY) ||
        request.hitBoundary.boundaryY < 0.0 ||
        request.hitBoundary.boundaryY >= request.hitBoundary.imageHeight) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, QStringLiteral("Hit boundary dimensions do not match the Sequence."));
        return false;
    }

    std::optional<PreparedModel> model;
    if (useModel) {
        model = modelProvider_
                    ? modelProvider_(&localError)
                    : (modelLoader_ ? prepareProductionModel(*modelLoader_, &localError)
                                    : std::nullopt);
        if (!model || !validModel(*model)) {
            lease.transition(OperationLifecycle::Failed);
            setError(error, localError.isEmpty()
                                ? QStringLiteral("The Active Model is unavailable or invalid.")
                                : localError);
            return false;
        }
        if (request.hitClassId &&
            std::none_of(model->snapshot.classes.begin(), model->snapshot.classes.end(),
                         [&](const run::RunClassSnapshot& cls) {
                             return cls.id == *request.hitClassId;
                         })) {
            lease.transition(OperationLifecycle::Failed);
            setError(error, QStringLiteral("Hit Class is not present in the Active Model."));
            return false;
        }
    }

    const QString runFolder = uniqueRunFolder(request.outputRoot, request.runName);
    if (completedRunFolder)
        *completedRunFolder = runFolder;
    const QString startedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    run::RunManifestData data;
    data.runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    data.runName = cleanName(request.runName);
    data.operation = run::RunOperation::SequenceTest;
    data.experimentType = request.experimentType;
    data.notes = request.notes;
    data.startedAt = startedAt;
    data.stopReason = QStringLiteral("operation_in_progress");
    data.opendssVersion = request.opendssVersion;
    data.sourceSequence = {sequenceData.sequenceId, sequenceData.name,
                           QStringLiteral("source/sequence.json")};
    if (model)
        data.model = model->snapshot;
    data.routing = {request.triggerMode, request.hitClassId,
                    request.physicalDaqOutputEnabled};
    data.cameraSettings = request.cameraSettings;
    data.detectorSettings = request.detectorSettings;
    data.cropSettings = request.cropSettings;
    data.daqSettings = request.daqSettings;
    data.timingSettings = request.timingSettings;
    data.hitBoundary = request.hitBoundary;
    data.requestedProcessingFps = request.requestedProcessingFps;

    writer = run::RunWriterV2::start(runFolder, data, &localError);
    if (!writer) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, localError);
        return false;
    }
    const QString sourceFolder =
        QDir(runFolder).filePath(QStringLiteral("source"));
    const QString sourceManifest =
        QDir(sourceFolder).filePath(QStringLiteral("sequence.json"));
    if (!QDir().mkpath(sourceFolder) ||
        !writeExactBytes(sourceManifest, request.frozenManifestBytes,
                         &localError)) {
        writer->finalize(run::RunStatus::Failed,
                         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                         QStringLiteral("source_snapshot_failed"), 0.0, nullptr);
        lease.transition(OperationLifecycle::Failed);
        setError(error,
                 localError.isEmpty()
                     ? QStringLiteral("Could not snapshot the source Sequence manifest.")
                     : localError);
        return false;
    }

    detector_.reset();
    elapsed.start();
    const auto scheduleStart = std::chrono::steady_clock::now();
    qint64 eventNumber = 0;
    QString failureReason = QStringLiteral("processing_failed");
    std::optional<PendingEvent> pending;

    const auto finalizePending = [&]() -> bool {
        if (!pending)
            return true;
        pending->event.observedRoute = pending->route.finalize();
        const auto decision = decision::DecisionService::decide(
            request.triggerMode, pending->event.predictedClassId,
            request.hitClassId, &localError);
        if (!decision)
            return false;
        pending->event.decision = *decision;

        bool pulseFailed = false;
        QString pulseError;
        if (pending->event.decision == run::Route::Waste) {
            pending->event.daqPulseStatus = run::DaqPulseStatus::NotRequested;
        } else if (!request.physicalDaqOutputEnabled) {
            pending->event.daqPulseStatus =
                run::DaqPulseStatus::SuppressedNotIssued;
        } else {
            localError.clear();
            bool dispatchPulse = false;
            {
                std::lock_guard lock(controlMutex_);
                if (!stopRequested_) {
                    pulseInFlight_ = true;
                    pulseThread_ = std::this_thread::get_id();
                    dispatchPulse = true;
                }
            }
            if (!dispatchPulse) {
                localError =
                    QStringLiteral("Stop was requested before DAQ Hit output dispatch.");
                pending->event.daqPulseStatus =
                    run::DaqPulseStatus::SuppressedNotIssued;
                qWarning().noquote()
                    << "Sequence Test DAQ Hit output suppressed:" << localError;
            } else {
                run::DaqPulseStatus pulseStatus = run::DaqPulseStatus::Failed;
                try {
                    pulseStatus = hitPulse_(true, &localError);
                } catch (const std::exception& exception) {
                    localError =
                        QStringLiteral("DAQ Hit output failed: %1").arg(exception.what());
                } catch (...) {
                    localError = QStringLiteral("DAQ Hit output failed.");
                }
                {
                    std::lock_guard lock(controlMutex_);
                    pulseInFlight_ = false;
                    pulseThread_ = {};
                }
                pulseFinished_.notify_all();
                if (pulseStatus != run::DaqPulseStatus::Issued &&
                    pulseStatus != run::DaqPulseStatus::SuppressedNotIssued &&
                    pulseStatus != run::DaqPulseStatus::Failed) {
                    pulseStatus = run::DaqPulseStatus::Failed;
                    localError =
                        QStringLiteral("DAQ Hit output returned an invalid status.");
                } else if (pulseStatus == run::DaqPulseStatus::SuppressedNotIssued &&
                           localError.trimmed().isEmpty()) {
                    pulseStatus = run::DaqPulseStatus::Failed;
                    localError =
                        QStringLiteral("DAQ Hit output was suppressed without a reason.");
                }
                pending->event.daqPulseStatus = pulseStatus;
                if (pulseStatus == run::DaqPulseStatus::SuppressedNotIssued) {
                    qWarning().noquote()
                        << "Sequence Test DAQ Hit output suppressed:" << localError;
                } else if (pulseStatus == run::DaqPulseStatus::Failed) {
                    pulseError =
                        localError.isEmpty()
                            ? QStringLiteral("The Sequence Test DAQ Hit output failed.")
                            : localError;
                    failureReason = QStringLiteral("daq_pulse_failed");
                    pulseFailed = true;
                }
            }
        }

        if (!writer->appendEvent(pending->event, pending->cropBytes, &localError)) {
            if (pulseFailed && localError != pulseError)
                localError = pulseError + QStringLiteral(" ") + localError;
            return false;
        }
        pending.reset();
        if (pulseFailed) {
            localError = pulseError;
            return false;
        }
        return true;
    };

    bool processingOk = true;
    for (qint64 frameIndex = 1; frameIndex <= sequenceData.frameCount; ++frameIndex) {
        const auto deadline =
            scheduleStart +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(
                    static_cast<double>(frameIndex) /
                    request.requestedProcessingFps));
        {
            std::unique_lock lock(controlMutex_);
            if (stopChanged_.wait_until(lock, deadline,
                                        [&] { return stopRequested_; }))
                break;
        }
        QImage image =
            request.loadedSequence->frames.at(frameIndex - 1).image;
        image = image.convertToFormat(QImage::Format_Grayscale8);
        cv::Mat frame(image.height(), image.width(), CV_8UC1, image.bits(),
                      image.bytesPerLine());
        const DropletDetectionFrame detection = detector_.processFrame(frame);
        if (stopRequested())
            break;

        if (detection.eventEntered) {
            if (!finalizePending()) {
                processingOk = false;
                break;
            }
            desktop_app::DatasetCrop crop;
            if (!desktop_app::CropService::makeDatasetCrop(frame, detection.bbox, &crop,
                                                           &localError)) {
                processingOk = false;
                break;
            }
            pending.emplace(request.hitBoundary);
            ++eventNumber;
            pending->event.eventId =
                QStringLiteral("event_%1").arg(eventNumber, 6, 10, QLatin1Char('0'));
            pending->event.detectionTimestamp =
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            pending->event.sourceFrameIndex = frameIndex;
            pending->event.cropPath =
                QStringLiteral("crops/droplet_%1.png")
                    .arg(eventNumber, 6, 10, QLatin1Char('0'));
            pending->cropBytes = pngBytes(crop.image, &localError);
            if (pending->cropBytes.isEmpty()) {
                processingOk = false;
                break;
            }
            if (model) {
                QElapsedTimer inferenceTimer;
                inferenceTimer.start();
                auto result = model->classify(crop.image, &localError);
                const double inferenceMs =
                    static_cast<double>(inferenceTimer.nsecsElapsed()) / 1'000'000.0;
                if (!result || result->scores.size() != model->snapshot.classes.size() ||
                    std::any_of(result->scores.begin(), result->scores.end(),
                                [](double score) { return !std::isfinite(score); })) {
                    if (localError.isEmpty())
                        localError = QStringLiteral("Model inference result is invalid.");
                    processingOk = false;
                    break;
                }
                int bestIndex = 0;
                for (int index = 1; index < result->scores.size(); ++index) {
                    if (result->scores[index] > result->scores[bestIndex])
                        bestIndex = index;
                }
                pending->event.predictedClassId =
                    model->snapshot.classes.at(bestIndex).id;
                pending->event.scores = result->scores;
                pending->event.inferenceTimeMs = inferenceMs;
            }
        } else if (!detection.detected && !finalizePending()) {
            processingOk = false;
            break;
        }
        if (pending && detection.detected)
            pending->route.addSample(detection.centroid.y);
        ++processedFrames;
        const double progressSeconds =
            static_cast<double>(elapsed.nsecsElapsed()) / 1'000'000'000.0;
        lastProgressAchievedFps =
            progressSeconds > 0.0 ? processedFrames / progressSeconds : 0.0;
        reportProgress(
            request.progressCallback,
            {processedFrames, sequenceData.frameCount, progressSeconds,
             lastProgressAchievedFps});
    }
    if (processingOk && !finalizePending())
        processingOk = false;

    bool stopped = false;
    {
        std::lock_guard lock(controlMutex_);
        stopped = stopRequested_;
        acceptingStop_ = false;
    }
    qWarning().noquote() << "Sequence Test frame summary:"
                         << "processed" << processedFrames
                         << "total" << sequenceData.frameCount;

    if (!processingOk || (!stopped && processedFrames == 0)) {
        if (localError.isEmpty())
            localError = QStringLiteral("The Sequence contained no readable frames.");
        writer->finalize(run::RunStatus::Failed,
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                          failureReason, lastProgressAchievedFps, nullptr);
        lease.transition(OperationLifecycle::Failed);
        setError(error, localError);
        return false;
    }
    const run::RunStatus finalStatus =
        stopped ? run::RunStatus::Interrupted : run::RunStatus::Completed;
    const QString stopReason =
        stopped ? QStringLiteral("user") : QStringLiteral("end_of_sequence");
    if (!writer->finalize(finalStatus,
                          QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
                          stopReason, lastProgressAchievedFps, &localError)) {
        lease.transition(OperationLifecycle::Failed);
        setError(error, localError);
        return false;
    }
    lease.transition(stopped ? OperationLifecycle::Interrupted
                             : OperationLifecycle::Completed);
    return true;
    } catch (const std::exception& exception) {
        return failFromException(
            QStringLiteral("Sequence Test failed: %1").arg(exception.what()));
    } catch (...) {
        return failFromException(
            QStringLiteral("Sequence Test failed with an unknown exception."));
    }
}

} // namespace desktop_app::v2::sequence_test
