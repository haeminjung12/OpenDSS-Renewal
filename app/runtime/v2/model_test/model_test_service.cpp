#include "model_test_service.h"

#include "model_test_writer.h"
#include "../dataset/dataset_manifest_v2.h"
#include "../model/model_load_service.h"
#include "../operation/operation_coordinator.h"

#include <QDateTime>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>

namespace {

using namespace desktop_app::v2;
using namespace desktop_app::v2::model_test;

constexpr qsizetype kMaximumProcessMessageBytes = 1024 * 1024;

void setError(QString* error, const QString& message) {
    if (error)
        *error = message;
}

QString now() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool validPreparedModel(const PreparedModelTestModel& model) {
    if (!model.classify || model.snapshot.id.trimmed().isEmpty() ||
        model.snapshot.name.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
             .match(model.snapshot.checkpointSha256)
             .hasMatch() ||
        !QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
             .match(model.snapshot.metadataSha256)
             .hasMatch() ||
        (model.snapshot.classes.size() != 2 &&
         model.snapshot.classes.size() != 3)) {
        return false;
    }
    for (const auto& cls : model.snapshot.classes) {
        if (cls.id.trimmed().isEmpty() || cls.name.trimmed().isEmpty())
            return false;
    }
    return model.effectiveDevice == EffectiveDevice::Cuda
               ? !model.fallbackWarning
               : model.fallbackWarning &&
                     !model.fallbackWarning->trimmed().isEmpty();
}

bool writeProcessMessage(QProcess& process, const QJsonObject& message,
                         QString* error) {
    const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) +
                            '\n';
    if (process.write(line) != line.size() || !process.waitForBytesWritten(5000)) {
        setError(error, QStringLiteral("Could not write to the Model Test process."));
        return false;
    }
    return true;
}

std::optional<QJsonObject> readProcessMessage(QProcess& process,
                                              QByteArray& buffer,
                                              const std::atomic_bool& stopRequested,
                                              bool* cancelled,
                                              QString* error) {
    QDeadlineTimer inactivityDeadline(300000);
    if (cancelled)
        *cancelled = false;
    while (true) {
        if (stopRequested.load(std::memory_order_acquire)) {
            if (cancelled)
                *cancelled = true;
            return std::nullopt;
        }
        const qsizetype newline = buffer.indexOf('\n');
        if (newline >= 0) {
            if (newline > kMaximumProcessMessageBytes) {
                setError(
                    error,
                    QStringLiteral(
                        "Model Test process message exceeded the maximum allowed size."));
                return std::nullopt;
            }
            const QByteArray line = buffer.left(newline).trimmed();
            buffer.remove(0, newline + 1);
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError ||
                !document.isObject()) {
                setError(error,
                         QStringLiteral("Model Test process emitted malformed JSONL."));
                return std::nullopt;
            }
            return document.object();
        }
        if (process.bytesAvailable() > 0)
            buffer += process.readAllStandardOutput();
        if (buffer.indexOf('\n') >= 0)
            continue;
        if (buffer.size() > kMaximumProcessMessageBytes) {
            setError(
                error,
                QStringLiteral(
                    "Model Test process message exceeded the maximum allowed size."));
            return std::nullopt;
        }
        if (process.state() == QProcess::NotRunning) {
            buffer += process.readAllStandardOutput();
            if (buffer.indexOf('\n') >= 0)
                continue;
            if (buffer.size() > kMaximumProcessMessageBytes) {
                setError(
                    error,
                    QStringLiteral(
                        "Model Test process message exceeded the maximum allowed size."));
                return std::nullopt;
            }
            setError(error,
                     QStringLiteral("Model Test process exited before completing its protocol: %1")
                         .arg(QString::fromUtf8(process.readAllStandardError()).trimmed()));
            return std::nullopt;
        }
        if (inactivityDeadline.hasExpired()) {
            setError(error,
                     QStringLiteral("Model Test process protocol timed out."));
            return std::nullopt;
        }
        process.waitForReadyRead(100);
    }
}

void stopProcessBounded(QProcess& process) {
    if (process.state() == QProcess::NotRunning) {
        process.waitForFinished(0);
        return;
    }
    process.terminate();
    if (!process.waitForFinished(1000)) {
        process.kill();
        process.waitForFinished(2000);
    }
}

class RunningGuard final {
  public:
    explicit RunningGuard(std::atomic_bool& running) : running_(running) {}
    ~RunningGuard() { running_.store(false, std::memory_order_release); }

  private:
    std::atomic_bool& running_;
};

} // namespace

namespace desktop_app::v2::model_test {

ModelTestService::ModelTestService(OperationCoordinator& operations,
                                   ModelLoadService* modelLoader,
                                   ModelTestModelProvider modelProvider,
                                   ModelTestProgress progress,
                                   QString pythonExecutable,
                                   QString workingDirectory)
    : operations_(operations), modelLoader_(modelLoader),
      modelProvider_(std::move(modelProvider)), progress_(std::move(progress)),
      pythonExecutable_(std::move(pythonExecutable)),
      workingDirectory_(std::move(workingDirectory)) {}

void ModelTestService::requestStop() noexcept {
    stopRequested_.store(true, std::memory_order_release);
}

bool ModelTestService::run(const ModelTestRequest& request, QString* error) {
    setError(error, {});
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true,
                                          std::memory_order_acq_rel)) {
        setError(error, QStringLiteral("This Model Test is already running."));
        return false;
    }
    RunningGuard guard(running_);
    stopRequested_.store(false, std::memory_order_release);

    OperationLease lease;
    ModelLease modelLease;
    std::optional<ModelTestWriter> writer;
    QString localError;
    const auto failAfterStart = [&](const QString& message,
                                    const QString& reason) {
        if (writer) {
            try {
                writer->finalize(ModelTestStatus::Failed, now(), reason, nullptr);
            } catch (...) {
            }
        }
        if (lease.isValid())
            lease.transition(OperationLifecycle::Failed);
        setError(error, message);
        return false;
    };

    try {
        if (request.datasetJsonPath.trimmed().isEmpty() ||
            request.outputFolder.trimmed().isEmpty() ||
            request.opendssVersion.trimmed().isEmpty()) {
            setError(error, QStringLiteral("Model Test request is incomplete."));
            return false;
        }
        if (QFileInfo::exists(request.outputFolder)) {
            setError(error,
                     QStringLiteral("Model Test output folder already exists."));
            return false;
        }

        auto acquired = operations_.acquireWithDataset(
            OperationKind::ModelTest,
            ResourceLock::Model | ResourceLock::Storage,
            request.datasetJsonPath, DatasetAccess::Read);
        if (!acquired.acquired()) {
            setError(error, acquired.fault
                                ? acquired.fault->reason
                                : QStringLiteral("Model Test resources are in use."));
            return false;
        }
        lease = std::move(acquired.lease);
        if (!lease.transition(OperationLifecycle::Running)) {
            setError(error,
                     QStringLiteral("Model Test could not enter Running state."));
            return false;
        }

        std::optional<PreparedModelTestModel> model;
        std::optional<PersistedActiveCheckpointInspection> checkpoint;
        if (modelProvider_) {
            model = modelProvider_(&localError);
            if (!model || !validPreparedModel(*model)) {
                lease.transition(OperationLifecycle::Failed);
                setError(error,
                         localError.isEmpty()
                             ? QStringLiteral("The Active Model is unavailable or invalid.")
                             : localError);
                return false;
            }
        } else {
            if (!modelLoader_) {
                lease.transition(OperationLifecycle::Failed);
                setError(error,
                         QStringLiteral("The Active Model loader is unavailable."));
                return false;
            }
            checkpoint =
                modelLoader_->inspectAndMigratePersistedActiveCheckpoint();
            if (!checkpoint->loadable) {
                lease.transition(OperationLifecycle::Failed);
                setError(error, checkpoint->error);
                return false;
            }
            auto acquiredModel = operations_.acquireModel(
                QFileInfo(checkpoint->checkpointPath).absolutePath(),
                ModelAccess::Read);
            if (!acquiredModel.acquired()) {
                lease.transition(OperationLifecycle::Failed);
                setError(error,
                         acquiredModel.fault
                             ? acquiredModel.fault->reason
                             : QStringLiteral(
                                   "The Active Model Package is in use."));
                return false;
            }
            modelLease = std::move(acquiredModel.lease);
        }

        auto dataset = dataset::DatasetManifestV2::load(
            request.datasetJsonPath, &localError);
        if (!dataset) {
            lease.transition(OperationLifecycle::Failed);
            setError(error, localError);
            return false;
        }
        const auto samples = dataset->trainingSamples(&localError);
        if (!localError.isEmpty()) {
            lease.transition(OperationLifecycle::Failed);
            setError(error, localError);
            return false;
        }
        if (samples.isEmpty()) {
            lease.transition(OperationLifecycle::Failed);
            setError(error,
                     QStringLiteral("The Dataset has no eligible labeled crops."));
            return false;
        }
        const qsizetype modelClassCount =
            model ? model->snapshot.classes.size() : checkpoint->classes.size();
        if (modelClassCount != dataset->classes().size()) {
            lease.transition(OperationLifecycle::Failed);
            setError(
                error,
                QStringLiteral("The Active Model has %1 output classes, but the "
                               "Dataset defines %2 classes.")
                    .arg(modelClassCount)
                    .arg(dataset->classes().size()));
            return false;
        }

        const QString datasetRoot =
            QDir::cleanPath(QDir::fromNativeSeparators(
                QFileInfo(request.datasetJsonPath).canonicalPath()));
        ModelTestSummaryData summary;
        summary.testId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        summary.startedAt = now();
        summary.opendssVersion = request.opendssVersion;
        if (model) {
            summary.activeModel = model->snapshot;
        } else {
            summary.activeModel.id = checkpoint->id;
            summary.activeModel.name = checkpoint->displayName;
            summary.activeModel.checkpointSha256 =
                checkpoint->checkpointSha256;
            summary.activeModel.metadataSha256 = checkpoint->metadataSha256;
            for (const auto& cls : checkpoint->classes)
                summary.activeModel.classes.push_back({cls.id, cls.displayLabel});
        }
        summary.dataset.id = dataset->datasetId();
        summary.dataset.sourcePath = datasetRoot;
        for (const auto& cls : dataset->classes())
            summary.dataset.classes.push_back({cls.id, cls.name});
        summary.eligibleImages = samples.size();

        qint64 processed = 0;
        bool stopped = false;
        if (model) {
            summary.effectiveDevice = model->effectiveDevice;
            summary.fallbackWarning = model->fallbackWarning;
            writer = ModelTestWriter::start(request.outputFolder, summary,
                                            &localError);
            if (!writer) {
                lease.transition(OperationLifecycle::Failed);
                setError(error, localError);
                return false;
            }
            if (progress_)
                progress_(0, samples.size());
            for (const auto& sample : samples) {
                if (stopRequested_.load(std::memory_order_acquire))
                    break;
                QImageReader reader(sample.cropPath);
                QImage image = reader.read();
                if (image.isNull())
                    return failAfterStart(
                        QStringLiteral("Eligible crop '%1' could not be decoded.")
                            .arg(sample.cropPath),
                        QStringLiteral("corrupt_image"));
                image = image.convertToFormat(QImage::Format_Grayscale8);
                cv::Mat crop(image.height(), image.width(), CV_8UC1, image.bits(),
                             image.bytesPerLine());
                auto result = model->classify(crop, &localError);
                if (!result)
                    return failAfterStart(localError,
                                          QStringLiteral("classification_failed"));
                if (result->scores.size() != model->snapshot.classes.size() ||
                    std::any_of(result->scores.cbegin(), result->scores.cend(),
                                [](double score) { return !std::isfinite(score); })) {
                    return failAfterStart(
                        QStringLiteral("Active Model returned invalid class scores."),
                        QStringLiteral("invalid_class_scores"));
                }
                int bestIndex = 0;
                for (int index = 1; index < result->scores.size(); ++index) {
                    if (result->scores.at(index) >
                        result->scores.at(bestIndex))
                        bestIndex = index;
                }
                const QString relativePath = QDir::fromNativeSeparators(
                    QDir(datasetRoot).relativeFilePath(sample.cropPath));
                if (!writer->appendPrediction(
                        {relativePath, sample.classId,
                         model->snapshot.classes.at(bestIndex).id,
                         result->scores},
                        &localError)) {
                    return failAfterStart(
                        localError, QStringLiteral("artifact_write_failed"));
                }
                ++processed;
                if (progress_)
                    progress_(processed, samples.size());
            }
            stopped = stopRequested_.load(std::memory_order_acquire);
        } else {
            if (!QFileInfo(pythonExecutable_).isFile() ||
                !QFileInfo(workingDirectory_).isDir()) {
                lease.transition(OperationLifecycle::Failed);
                setError(error,
                         QStringLiteral("The installed Model Test Python runtime is unavailable."));
                return false;
            }
            QProcess process;
            QProcessEnvironment environment =
                QProcessEnvironment::systemEnvironment();
            environment.remove(QStringLiteral("PYTHONPATH"));
            environment.remove(QStringLiteral("PYTHONHOME"));
            environment.insert(QStringLiteral("PYTHONNOUSERSITE"),
                               QStringLiteral("1"));
            process.setProcessEnvironment(environment);
            process.setWorkingDirectory(workingDirectory_);
            process.setProgram(pythonExecutable_);
            process.setArguments(
                {QStringLiteral("-I"), QStringLiteral("-m"),
                 QStringLiteral("droplet_trainer"),
                 QStringLiteral("model-test-process"),
                 QStringLiteral("--checkpoint"), checkpoint->checkpointPath});
            process.setProcessChannelMode(QProcess::SeparateChannels);
            process.start();
            if (!process.waitForStarted(10000)) {
                lease.transition(OperationLifecycle::Failed);
                setError(error,
                         QStringLiteral("Model Test Python process could not start: ") +
                             process.errorString());
                return false;
            }

            QJsonArray classIds;
            for (const auto& cls : dataset->classes())
                classIds.append(cls.id);
            QJsonArray processItems;
            for (qsizetype index = 0; index < samples.size(); ++index) {
                const auto& sample = samples.at(index);
                processItems.append(
                    QJsonObject{{"sequence", index},
                                {"record_id", sample.recordId},
                                {"image_path", sample.cropPath},
                                {"true_class_id", sample.classId}});
            }
            if (!writeProcessMessage(
                    process,
                    QJsonObject{{"schema", "opendss.model_test.request.v1"},
                                {"class_ids", classIds},
                                {"items", processItems}},
                    &localError)) {
                stopProcessBounded(process);
                lease.transition(OperationLifecycle::Failed);
                setError(error, localError);
                return false;
            }

            QByteArray outputBuffer;
            bool processReadCancelled = false;
            const auto ready =
                readProcessMessage(process, outputBuffer, stopRequested_,
                                   &processReadCancelled, &localError);
            const QString device =
                ready ? ready->value("device").toString() : QString{};
            if (!ready || ready->value("event").toString() != "ready" ||
                ready->value("checkpoint_sha256").toString().toLower() !=
                    checkpoint->checkpointSha256 ||
                ready->value("total").toInteger(-1) != samples.size() ||
                (device != "cuda" && device != "cpu")) {
                stopProcessBounded(process);
                lease.transition(processReadCancelled
                                     ? OperationLifecycle::Interrupted
                                     : OperationLifecycle::Failed);
                setError(error,
                         processReadCancelled
                             ? QStringLiteral("Model Test stopped before evaluation began.")
                         : localError.isEmpty()
                             ? QStringLiteral("Model Test Python readiness is invalid.")
                             : localError);
                return false;
            }
            summary.effectiveDevice =
                device == "cuda" ? EffectiveDevice::Cuda : EffectiveDevice::Cpu;
            if (summary.effectiveDevice == EffectiveDevice::Cpu) {
                summary.fallbackWarning = QStringLiteral(
                    "CUDA was unavailable or unusable; automatic Model Test used CPU.");
            }
            writer = ModelTestWriter::start(request.outputFolder, summary,
                                            &localError);
            if (!writer) {
                stopProcessBounded(process);
                lease.transition(OperationLifecycle::Failed);
                setError(error, localError);
                return false;
            }
            if (progress_)
                progress_(0, samples.size());
            if (!writeProcessMessage(process, QJsonObject{{"command", "start"}},
                                     &localError)) {
                stopProcessBounded(process);
                return failAfterStart(
                    localError, QStringLiteral("process_protocol_failed"));
            }
            const auto failProcessAfterStart =
                [&](const QString& message, const QString& reason) {
                    stopProcessBounded(process);
                    return failAfterStart(message, reason);
                };

            qint64 expectedBatch = 0;
            bool processFinished = false;
            bool processTerminatedForStop = false;
            while (!processFinished) {
                processReadCancelled = false;
                const auto message =
                    readProcessMessage(process, outputBuffer, stopRequested_,
                                       &processReadCancelled, &localError);
                if (!message) {
                    if (processReadCancelled) {
                        stopped = true;
                        stopProcessBounded(process);
                        processTerminatedForStop = true;
                        break;
                    }
                    return failProcessAfterStart(
                        localError, QStringLiteral("process_protocol_failed"));
                }
                const QString event = message->value("event").toString();
                if (event == "run_failed") {
                    return failProcessAfterStart(
                        message->value("error").toString(),
                        QStringLiteral("classification_failed"));
                }
                if (event == "run_finished") {
                    stopped = message->value("status").toString() == "stopped";
                    if ((!stopped &&
                         message->value("status").toString() != "completed") ||
                        message->value("processed").toInteger(-1) != processed ||
                        message->value("total").toInteger(-1) != samples.size()) {
                        return failProcessAfterStart(
                            QStringLiteral("Model Test completion protocol is invalid."),
                            QStringLiteral("process_protocol_failed"));
                    }
                    processFinished = true;
                    continue;
                }
                if (event != "batch_ready" ||
                    message->value("batch_index").toInteger(-1) != expectedBatch ||
                    !message->value("facts").isArray()) {
                    return failProcessAfterStart(
                        QStringLiteral("Model Test batch protocol is invalid."),
                        QStringLiteral("process_protocol_failed"));
                }
                const QJsonArray facts = message->value("facts").toArray();
                if (facts.isEmpty() || processed + facts.size() > samples.size())
                    return failProcessAfterStart(
                        QStringLiteral("Model Test batch size is invalid."),
                        QStringLiteral("process_protocol_failed"));
                QVector<ModelTestPrediction> predictions;
                predictions.reserve(facts.size());
                for (qsizetype index = 0; index < facts.size(); ++index) {
                    const QJsonObject fact = facts.at(index).toObject();
                    const auto& sample = samples.at(processed + index);
                    if (fact.value("sequence").toInteger(-1) != processed + index ||
                        fact.value("record_id").toString() != sample.recordId ||
                        QFileInfo(fact.value("image_path").toString()).absoluteFilePath() !=
                            QFileInfo(sample.cropPath).absoluteFilePath() ||
                        fact.value("true_class_id").toString() != sample.classId ||
                        !fact.value("class_scores").isArray()) {
                        return failProcessAfterStart(
                            QStringLiteral("Model Test per-image protocol is invalid."),
                            QStringLiteral("process_protocol_failed"));
                    }
                    QVector<double> scores;
                    for (const auto& score : fact.value("class_scores").toArray())
                        scores.push_back(score.toDouble(
                            std::numeric_limits<double>::quiet_NaN()));
                    predictions.push_back(
                        {QDir::fromNativeSeparators(
                             QDir(datasetRoot).relativeFilePath(sample.cropPath)),
                         sample.classId,
                         fact.value("predicted_class_id").toString(), scores});
                }
                if (!writer->appendBatch(predictions, &localError))
                    return failProcessAfterStart(
                        localError, QStringLiteral("artifact_write_failed"));
                processed += facts.size();
                if (progress_)
                    progress_(processed, samples.size());
                stopped = stopRequested_.load(std::memory_order_acquire);
                if (!writeProcessMessage(
                        process,
                        QJsonObject{{"command", "committed"},
                                    {"batch_index", expectedBatch},
                                    {"stop", stopped}},
                        &localError)) {
                    return failProcessAfterStart(
                        localError, QStringLiteral("process_protocol_failed"));
                }
                ++expectedBatch;
            }
            if (process.state() != QProcess::NotRunning &&
                !process.waitForFinished(5000)) {
                return failProcessAfterStart(
                    QStringLiteral("Model Test Python process did not exit."),
                    QStringLiteral("process_protocol_failed"));
            }
            if (!processTerminatedForStop &&
                (process.exitStatus() != QProcess::NormalExit ||
                 process.exitCode() != 0)) {
                return failProcessAfterStart(
                    QStringLiteral("Model Test Python process failed: ") +
                        QString::fromUtf8(process.readAllStandardError()).trimmed(),
                    QStringLiteral("classification_failed"));
            }
        }

        const ModelTestStatus status =
            stopped ? ModelTestStatus::Stopped : ModelTestStatus::Completed;
        const QString reason =
            stopped ? QStringLiteral("user")
                    : QStringLiteral("end_of_dataset");
        if (!writer->finalize(status, now(), reason, &localError))
            return failAfterStart(localError,
                                  QStringLiteral("artifact_finalize_failed"));
        lease.transition(stopped ? OperationLifecycle::Interrupted
                                 : OperationLifecycle::Completed);
        return true;
    } catch (const std::exception& exception) {
        return failAfterStart(
            QStringLiteral("Model Test failed: %1").arg(exception.what()),
            QStringLiteral("processing_exception"));
    } catch (...) {
        return failAfterStart(
            QStringLiteral("Model Test failed with an unknown exception."),
            QStringLiteral("processing_exception"));
    }
}

} // namespace desktop_app::v2::model_test
