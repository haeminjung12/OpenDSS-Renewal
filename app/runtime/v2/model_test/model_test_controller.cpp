#include "model_test_controller.h"

#include "model_test_summary_v2.h"
#include "../model/model_load_service.h"
#include "../operation/operation_coordinator.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QVariantList>

#include <utility>

namespace {

using namespace desktop_app::v2::model_test;

QString localPath(const QUrl& url, const QString& label, QString* error) {
    if (!url.isValid() || !url.isLocalFile() || url.hasQuery() ||
        url.hasFragment()) {
        *error = label + QStringLiteral(" must be a local URL.");
        return {};
    }
    const QString path = url.toLocalFile();
    if (path.trimmed().isEmpty())
        *error = label + QStringLiteral(" must contain a local path.");
    return path;
}

QString statusText(ModelTestStatus status) {
    switch (status) {
    case ModelTestStatus::Completed:
        return QStringLiteral("completed");
    case ModelTestStatus::Stopped:
        return QStringLiteral("stopped");
    case ModelTestStatus::Failed:
        return QStringLiteral("failed");
    }
    return {};
}

QVariantMap resultMap(const ModelTestSummaryV2& summary) {
    const auto& data = summary.data();
    const auto& derived = summary.derivedResults();
    QVariantList perClass;
    for (const auto& metrics : derived.perClass) {
        QVariantMap item{{QStringLiteral("classId"), metrics.classId},
                         {QStringLiteral("support"), metrics.support},
                         {QStringLiteral("correct"), metrics.correct}};
        if (metrics.accuracy)
            item.insert(QStringLiteral("accuracy"), *metrics.accuracy);
        perClass.push_back(item);
    }

    QVariantList confusionMatrix;
    for (const auto& sourceRow : derived.confusionMatrix) {
        QVariantList row;
        for (qint64 value : sourceRow)
            row.push_back(value);
        confusionMatrix.push_back(row);
    }

    QVariantMap result{
        {QStringLiteral("status"), statusText(data.status)},
        {QStringLiteral("activeModelId"), data.activeModel.id},
        {QStringLiteral("activeModelName"), data.activeModel.name},
        {QStringLiteral("datasetId"), data.dataset.id},
        {QStringLiteral("effectiveDevice"),
         data.effectiveDevice == EffectiveDevice::Cuda ? QStringLiteral("GPU")
                                                       : QStringLiteral("CPU")},
        {QStringLiteral("eligibleImages"), data.eligibleImages},
        {QStringLiteral("processedImages"), derived.processedImages},
        {QStringLiteral("correctPredictions"), derived.correctPredictions},
        {QStringLiteral("perClass"), perClass},
        {QStringLiteral("confusionMatrix"), confusionMatrix}};
    if (data.fallbackWarning)
        result.insert(QStringLiteral("fallbackWarning"), *data.fallbackWarning);
    if (derived.overallAccuracy)
        result.insert(QStringLiteral("overallAccuracy"),
                      *derived.overallAccuracy);
    return result;
}

} // namespace

namespace desktop_app::v2::model_test {

ModelTestController::ModelTestController(OperationCoordinator& operations,
                                         ModelLoadService& modelLoader,
                                         QString opendssVersion,
                                         QObject* parent)
    : QObject(parent), opendssVersion_(std::move(opendssVersion)),
      service_(operations, &modelLoader, {},
               [this](qint64 processed, qint64 eligible) {
                   if (stopRequested_.load(std::memory_order_acquire))
                       service_.requestStop();
                   postProgress(processed, eligible);
               }) {}

ModelTestController::~ModelTestController() {
    stopRequested_.store(true, std::memory_order_release);
    service_.requestStop();
    if (worker_.joinable())
        worker_.join();
}

QUrl ModelTestController::datasetManifestUrl() const {
    return datasetManifestUrl_;
}

void ModelTestController::setDatasetManifestUrl(const QUrl& url) {
    if (active() || datasetManifestUrl_ == url)
        return;
    datasetManifestUrl_ = url;
    clearOutcome();
    updateReadyPresentation();
    emit changed();
}

QUrl ModelTestController::outputFolderUrl() const {
    return outputFolderUrl_;
}

void ModelTestController::setOutputFolderUrl(const QUrl& url) {
    if (active() || outputFolderUrl_ == url)
        return;
    outputFolderUrl_ = url;
    clearOutcome();
    updateReadyPresentation();
    emit changed();
}

QString ModelTestController::presentation() const {
    return presentation_;
}

QString ModelTestController::errorMessage() const {
    return errorMessage_;
}

qint64 ModelTestController::processedImages() const {
    return processedImages_;
}

qint64 ModelTestController::eligibleImages() const {
    return eligibleImages_;
}

double ModelTestController::progress() const {
    return eligibleImages_ > 0
               ? static_cast<double>(processedImages_) /
                     static_cast<double>(eligibleImages_)
               : 0.0;
}

QVariantMap ModelTestController::resultSummary() const {
    return resultSummary_;
}

QUrl ModelTestController::summaryUrl() const {
    return summaryUrl_;
}

QUrl ModelTestController::predictionsCsvUrl() const {
    return predictionsCsvUrl_;
}

QUrl ModelTestController::artifactOutputFolderUrl() const {
    return artifactOutputFolderUrl_;
}

bool ModelTestController::start() {
    if (active())
        return false;
    if (worker_.joinable())
        worker_.join();

    clearOutcome();
    QString validationError;
    const QString datasetPath =
        localPath(datasetManifestUrl_, QStringLiteral("Dataset manifest"),
                  &validationError);
    const QFileInfo datasetInfo(datasetPath);
    if (validationError.isEmpty() &&
        (!datasetInfo.isFile() || !datasetInfo.isReadable()))
        validationError = QStringLiteral("Dataset manifest is not a readable file.");
    const QString outputPath =
        validationError.isEmpty()
            ? localPath(outputFolderUrl_, QStringLiteral("Output folder"),
                        &validationError)
            : QString();
    if (validationError.isEmpty() && QFileInfo::exists(outputPath))
        validationError = QStringLiteral("Output folder already exists.");
    if (validationError.isEmpty() && opendssVersion_.trimmed().isEmpty())
        validationError = QStringLiteral("OpenDSS version is unavailable.");
    if (!validationError.isEmpty()) {
        presentation_ = QStringLiteral("error");
        errorMessage_ = validationError;
        emit changed();
        return false;
    }

    presentation_ = QStringLiteral("starting");
    stopRequested_.store(false, std::memory_order_release);
    emit changed();
    try {
        worker_ = std::thread(
            [this, datasetPath = QDir::cleanPath(datasetPath),
             outputPath = QDir::cleanPath(outputPath)] {
                QString error;
                const bool succeeded = service_.run(
                    {datasetPath, outputPath, opendssVersion_}, &error);
                QMetaObject::invokeMethod(
                    this,
                    [this, succeeded, error, outputPath] {
                        finishRun(succeeded, error, outputPath);
                    },
                    Qt::QueuedConnection);
            });
    } catch (const std::exception& exception) {
        presentation_ = QStringLiteral("error");
        errorMessage_ =
            QStringLiteral("Model Test could not start: %1").arg(exception.what());
        emit changed();
        return false;
    }
    return true;
}

bool ModelTestController::stop() {
    if (!active())
        return false;
    stopRequested_.store(true, std::memory_order_release);
    service_.requestStop();
    presentation_ = QStringLiteral("stopping");
    emit changed();
    return true;
}

bool ModelTestController::active() const {
    return presentation_ == QStringLiteral("starting") ||
           presentation_ == QStringLiteral("running") ||
           presentation_ == QStringLiteral("stopping");
}

void ModelTestController::clearOutcome() {
    errorMessage_.clear();
    processedImages_ = 0;
    eligibleImages_ = 0;
    resultSummary_.clear();
    summaryUrl_.clear();
    predictionsCsvUrl_.clear();
    artifactOutputFolderUrl_.clear();
}

void ModelTestController::updateReadyPresentation() {
    presentation_ =
        datasetManifestUrl_.isEmpty() || outputFolderUrl_.isEmpty()
            ? QStringLiteral("empty")
            : QStringLiteral("ready");
}

void ModelTestController::postProgress(qint64 processed, qint64 eligible) {
    QMetaObject::invokeMethod(
        this,
        [this, processed, eligible] {
            processedImages_ = processed;
            eligibleImages_ = eligible;
            if (presentation_ == QStringLiteral("starting"))
                presentation_ = QStringLiteral("running");
            emit changed();
        },
        Qt::QueuedConnection);
}

void ModelTestController::finishRun(bool succeeded,
                                    const QString& serviceError,
                                    const QString& outputPath) {
    stopRequested_.store(false, std::memory_order_release);
    const QString summaryPath =
        QDir(outputPath).filePath(QStringLiteral("model_test_summary.json"));
    QString summaryError;
    auto summary = ModelTestSummaryV2::load(summaryPath, &summaryError);
    if (summary) {
        const auto& data = summary->data();
        const auto& derived = summary->derivedResults();
        processedImages_ = derived.processedImages;
        eligibleImages_ = data.eligibleImages;
        resultSummary_ = resultMap(*summary);
        summaryUrl_ = QUrl::fromLocalFile(summaryPath);
        artifactOutputFolderUrl_ = QUrl::fromLocalFile(outputPath);
        const QString predictionsPath =
            QDir(outputPath).filePath(data.predictionsCsv);
        if (QFileInfo(predictionsPath).isFile())
            predictionsCsvUrl_ = QUrl::fromLocalFile(predictionsPath);
        presentation_ =
            data.status == ModelTestStatus::Completed
                ? QStringLiteral("completed")
                : data.status == ModelTestStatus::Stopped
                      ? QStringLiteral("interrupted")
                      : QStringLiteral("error");
    } else {
        presentation_ = QStringLiteral("error");
    }

    if (!succeeded)
        errorMessage_ = serviceError;
    else if (!summary)
        errorMessage_ = summaryError;
    emit changed();
}

} // namespace desktop_app::v2::model_test
