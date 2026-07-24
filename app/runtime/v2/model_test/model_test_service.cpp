#include "model_test_service.h"

#include "model_test_writer.h"
#include "../dataset/dataset_manifest_v2.h"
#include "../model/model_load_service.h"
#include "../operation/operation_coordinator.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>

namespace {

using namespace desktop_app::v2;
using namespace desktop_app::v2::model_test;

void setError(QString* error, const QString& message) {
    if (error)
        *error = message;
}

QString now() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

std::optional<QString> modelName(const QString& metadataPath, QString* error) {
    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not read verified model metadata."));
        return std::nullopt;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QString name =
        document.object().value(QStringLiteral("model_name")).toString().trimmed();
    if (!document.isObject() || name.isEmpty()) {
        setError(error,
                 QStringLiteral("Verified model metadata has no valid model_name."));
        return std::nullopt;
    }
    return name;
}

std::optional<PreparedModelTestModel>
prepareProductionModel(ModelLoadService& loader, QString* error) {
    QString warning;
    auto adapter =
        loader.preparePersistedActive(QStringLiteral("auto"), &warning, error);
    if (!adapter)
        return std::nullopt;
    const auto name =
        modelName(QString::fromStdString(adapter->metadataPath()), error);
    if (!name)
        return std::nullopt;

    const Metadata& metadata = adapter->metadata();
    if ((metadata.classes.size() != 2 && metadata.classes.size() != 3) ||
        metadata.displayLabels.size() != metadata.classes.size()) {
        setError(error,
                 QStringLiteral("Verified model classes and display labels are invalid."));
        return std::nullopt;
    }

    PreparedModelTestModel prepared;
    prepared.snapshot.id = QString::fromStdString(adapter->modelId());
    prepared.snapshot.name = *name;
    prepared.snapshot.onnxSha256 =
        QString::fromStdString(adapter->declaredOnnxSha256()).toLower();
    prepared.snapshot.metadataSha256 =
        QString::fromStdString(adapter->metadataSha256()).toLower();
    for (std::size_t index = 0; index < metadata.classes.size(); ++index) {
        prepared.snapshot.classes.push_back(
            {QString::fromStdString(metadata.classes[index]),
             QString::fromStdString(metadata.displayLabels[index])});
    }
    prepared.effectiveDevice =
        adapter->executionProvider() == "CUDA" ? EffectiveDevice::Cuda
                                                : EffectiveDevice::Cpu;
    if (prepared.effectiveDevice == EffectiveDevice::Cpu) {
        prepared.fallbackWarning =
            warning.trimmed().isEmpty()
                ? QStringLiteral("CUDA was unavailable or unusable; Auto mode used CPU.")
                : warning;
    }

    auto shared = std::shared_ptr<OnnxInferenceAdapter>(std::move(adapter));
    prepared.classify =
        [shared](const cv::Mat& crop,
                 QString* outputError) -> std::optional<ModelTestInferenceResult> {
        try {
            const ClassificationResult result = shared->classify(crop);
            ModelTestInferenceResult output;
            output.scores.reserve(static_cast<qsizetype>(result.scores.size()));
            for (float score : result.scores)
                output.scores.push_back(score);
            return output;
        } catch (const std::exception& exception) {
            setError(outputError,
                     QStringLiteral("Model inference failed: %1")
                         .arg(exception.what()));
        } catch (...) {
            setError(outputError, QStringLiteral("Model inference failed."));
        }
        return std::nullopt;
    };
    return prepared;
}

bool validPreparedModel(const PreparedModelTestModel& model) {
    if (!model.classify || model.snapshot.id.trimmed().isEmpty() ||
        model.snapshot.name.trimmed().isEmpty() ||
        !QRegularExpression(QStringLiteral("^[0-9a-f]{64}$"))
             .match(model.snapshot.onnxSha256)
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
                                   ModelTestProgress progress)
    : operations_(operations), modelLoader_(modelLoader),
      modelProvider_(std::move(modelProvider)), progress_(std::move(progress)) {}

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

        auto model =
            modelProvider_ ? modelProvider_(&localError)
                           : (modelLoader_
                                  ? prepareProductionModel(*modelLoader_,
                                                           &localError)
                                  : std::nullopt);
        if (!model || !validPreparedModel(*model)) {
            lease.transition(OperationLifecycle::Failed);
            setError(error,
                     localError.isEmpty()
                         ? QStringLiteral("The Active Model is unavailable or invalid.")
                         : localError);
            return false;
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
        if (model->snapshot.classes.size() != dataset->classes().size()) {
            lease.transition(OperationLifecycle::Failed);
            setError(
                error,
                QStringLiteral("The Active Model has %1 output classes, but the "
                               "Dataset defines %2 classes.")
                    .arg(model->snapshot.classes.size())
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
        summary.activeModel = model->snapshot;
        summary.dataset.id = dataset->datasetId();
        summary.dataset.sourcePath = datasetRoot;
        for (const auto& cls : dataset->classes())
            summary.dataset.classes.push_back({cls.id, cls.name});
        summary.effectiveDevice = model->effectiveDevice;
        summary.fallbackWarning = model->fallbackWarning;
        summary.eligibleImages = samples.size();

        writer = ModelTestWriter::start(request.outputFolder, summary,
                                        &localError);
        if (!writer) {
            lease.transition(OperationLifecycle::Failed);
            setError(error, localError);
            return false;
        }
        if (progress_)
            progress_(0, samples.size());

        qint64 processed = 0;
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
                return failAfterStart(
                    localError.isEmpty()
                        ? QStringLiteral("Model inference failed.")
                        : localError,
                    QStringLiteral("classification_failed"));
            if (result->scores.size() != model->snapshot.classes.size() ||
                std::any_of(result->scores.cbegin(), result->scores.cend(),
                            [](double score) {
                                return !std::isfinite(score);
                            })) {
                return failAfterStart(
                    QStringLiteral(
                        "Model inference returned an invalid Class Score vector."),
                    QStringLiteral("invalid_class_scores"));
            }
            int bestIndex = 0;
            for (int index = 1; index < result->scores.size(); ++index) {
                if (result->scores.at(index) >
                    result->scores.at(bestIndex)) {
                    bestIndex = index;
                }
            }
            const QString relativePath = QDir::fromNativeSeparators(
                QDir(datasetRoot).relativeFilePath(sample.cropPath));
            ModelTestPrediction prediction{
                relativePath, sample.classId,
                model->snapshot.classes.at(bestIndex).id, result->scores};
            if (!writer->appendPrediction(prediction, &localError))
                return failAfterStart(localError,
                                      QStringLiteral("artifact_write_failed"));
            ++processed;
            if (progress_)
                progress_(processed, samples.size());
        }

        const bool stopped =
            stopRequested_.load(std::memory_order_acquire);
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
