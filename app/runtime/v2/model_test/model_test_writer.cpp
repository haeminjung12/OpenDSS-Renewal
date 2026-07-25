#include "model_test_writer.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>

namespace {

using namespace desktop_app::v2::model_test;

const QByteArray HeaderPrefix =
    "image_path,true_class_id,predicted_class_id,score_class_0,score_class_1";

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

QString csvField(QString value) {
    if (value.contains('"'))
        value.replace("\"", "\"\"");
    if (value.contains(',') || value.contains('"') || value.contains('\r') ||
        value.contains('\n')) {
        return '"' + value + '"';
    }
    return value;
}

QByteArray csvHeader(int classCount) {
    QByteArray header = HeaderPrefix;
    if (classCount == 3)
        header += ",score_class_2";
    header += ",correct\n";
    return header;
}

QByteArray csvRow(const ModelTestSummaryData& data,
                  const ModelTestPrediction& prediction) {
    QStringList fields{prediction.imagePath, prediction.trueClassId,
                       prediction.predictedClassId};
    for (double score : prediction.scores)
        fields.push_back(QString::number(score, 'g', 17));
    fields.push_back(
        ModelTestSummaryV2::predictionCorrect(data, prediction).value_or(false)
            ? "true"
            : "false");
    std::transform(fields.begin(), fields.end(), fields.begin(), csvField);
    return (fields.join(',') + '\n').toUtf8();
}

bool atomicWrite(const QString& path, const QByteArray& bytes, QString* error) {
    if (QFileInfo::exists(path))
        return fail(error, "Refusing to replace an existing final predictions CSV.");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QString("Could not open '%1': %2").arg(path, file.errorString()));
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return fail(error, "Could not completely write final predictions CSV.");
    }
    if (!file.commit())
        return fail(error, QString("Could not publish predictions CSV: %1").arg(file.errorString()));
    return true;
}

} // namespace

namespace desktop_app::v2::model_test {

std::optional<ModelTestWriter>
ModelTestWriter::start(const QString& outputFolder,
                       ModelTestSummaryData initialData, QString* error) {
    if (error)
        error->clear();
    if (QFileInfo::exists(outputFolder))
        return fail(error, "Model Test output folder already exists."), std::nullopt;

    initialData.status = ModelTestStatus::Stopped;
    initialData.endedAt = initialData.startedAt;
    initialData.stopReason = QStringLiteral("operation_in_progress");
    initialData.predictionsCsv = QStringLiteral("predictions.csv");
    if (!ModelTestSummaryV2::validateInitial(initialData, error))
        return std::nullopt;
    if (!QDir().mkpath(outputFolder))
        return fail(error, "Could not create Model Test output folder."), std::nullopt;
    const QFileInfo folderInfo(outputFolder);
    const QString requestedPath = QDir::cleanPath(
        QDir::fromNativeSeparators(folderInfo.absoluteFilePath()));
    const QString canonicalPath = QDir::cleanPath(
        QDir::fromNativeSeparators(folderInfo.canonicalFilePath()));
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    if (folderInfo.isSymLink() || canonicalPath.isEmpty() ||
        requestedPath.compare(canonicalPath, sensitivity) != 0) {
        QDir().rmdir(outputFolder);
        return fail(error, "Model Test output folder is not canonical."), std::nullopt;
    }

    auto partial = std::make_unique<QFile>(
        QDir(outputFolder).filePath(QStringLiteral("predictions.partial.csv")));
    if (!partial->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDir().rmdir(outputFolder);
        return fail(error, "Could not create predictions.partial.csv."), std::nullopt;
    }
    const QByteArray header = csvHeader(initialData.activeModel.classes.size());
    if (partial->write(header) != header.size() || !partial->flush()) {
        partial->close();
        QFile::remove(partial->fileName());
        QDir().rmdir(outputFolder);
        return fail(error, "Could not initialize predictions.partial.csv."), std::nullopt;
    }
    if (!ModelTestSummaryV2::savePartial(
            QDir(outputFolder).filePath(QStringLiteral("model_test_summary.partial.json")),
            initialData, {}, error)) {
        partial->close();
        QFile::remove(partial->fileName());
        QDir().rmdir(outputFolder);
        return std::nullopt;
    }
    return ModelTestWriter(outputFolder, std::move(initialData), std::move(partial));
}

ModelTestWriter::ModelTestWriter(QString outputFolder, ModelTestSummaryData data,
                                 std::unique_ptr<QFile> partialCsv)
    : outputFolder_(std::move(outputFolder)), data_(std::move(data)),
      partialCsv_(std::move(partialCsv)) {}

ModelTestWriter::ModelTestWriter(ModelTestWriter&&) noexcept = default;
ModelTestWriter& ModelTestWriter::operator=(ModelTestWriter&&) noexcept = default;
ModelTestWriter::~ModelTestWriter() = default;

bool ModelTestWriter::appendPrediction(const ModelTestPrediction& prediction,
                                       QString* error) {
    if (error)
        error->clear();
    if (finalized_ || !partialCsv_ || !partialCsv_->isOpen())
        return fail(error, "Model Test writer is not active.");
    if (predictions_.size() >= data_.eligibleImages)
        return fail(error, "All eligible images have already been processed.");
    if (!ModelTestSummaryV2::validatePrediction(data_, prediction, true, error))
        return false;
    if (std::any_of(predictions_.begin(), predictions_.end(),
                    [&](const ModelTestPrediction& old) {
                        return old.imagePath == prediction.imagePath;
                    })) {
        return fail(error, "Prediction image paths must be unique.");
    }

    const qint64 priorOffset = partialCsv_->pos();
    const QByteArray row = csvRow(data_, prediction);
    bool written = false;
    if (failNextAppendForTest_) {
        failNextAppendForTest_ = false;
        partialCsv_->write(row.left(row.size() / 2));
        partialCsv_->flush();
    } else {
        written = partialCsv_->write(row) == row.size() && partialCsv_->flush();
    }
    if (!written) {
        const bool rolledBack = partialCsv_->resize(priorOffset) &&
                                partialCsv_->seek(priorOffset) &&
                                partialCsv_->flush();
        return fail(error, rolledBack ? "Could not append prediction."
                                      : "Prediction append and rollback both failed.");
    }

    predictions_.push_back(prediction);
    if (!ModelTestSummaryV2::savePartial(
            QDir(outputFolder_)
                .filePath(QStringLiteral("model_test_summary.partial.json")),
            data_, predictions_, error)) {
        predictions_.removeLast();
        const bool rolledBack = partialCsv_->resize(priorOffset) &&
                                partialCsv_->seek(priorOffset) &&
                                partialCsv_->flush();
        return fail(error, rolledBack
                               ? "Could not update recoverable Model Test Summary."
                               : "Partial summary update and CSV rollback both failed.");
    }
    return true;
}

bool ModelTestWriter::flush(QString* error) {
    if (error)
        error->clear();
    if (finalized_)
        return true;
    if (!partialCsv_ || !partialCsv_->isOpen() || !partialCsv_->flush())
        return fail(error, "Could not flush predictions.partial.csv.");
    return ModelTestSummaryV2::savePartial(
        QDir(outputFolder_)
            .filePath(QStringLiteral("model_test_summary.partial.json")),
        data_, predictions_, error);
}

bool ModelTestWriter::finalize(ModelTestStatus status, const QString& endedAt,
                               const QString& stopReason, QString* error) {
    if (error)
        error->clear();
    if (finalized_)
        return fail(error, "Model Test has already been finalized.");
    if (status != ModelTestStatus::Completed &&
        status != ModelTestStatus::Stopped && status != ModelTestStatus::Failed) {
        return fail(error, "Final Model Test status is invalid.");
    }
    const auto started = QDateTime::fromString(data_.startedAt, Qt::ISODate);
    const auto ended = QDateTime::fromString(endedAt, Qt::ISODate);
    if (!ended.isValid() || ended < started || stopReason.trimmed().isEmpty() ||
        (status == ModelTestStatus::Completed &&
         predictions_.size() != data_.eligibleImages)) {
        return fail(error, "Final Model Test timing, reason, or processed count is invalid.");
    }
    if (partialCsv_ && !partialCsv_->isOpen() &&
        !partialCsv_->open(QIODevice::ReadWrite | QIODevice::Append)) {
        return fail(error, "Could not reopen predictions.partial.csv for finalization retry.");
    }
    if (!flush(error))
        return false;
    partialCsv_->close();

    const QString partialPath =
        QDir(outputFolder_).filePath(QStringLiteral("predictions.partial.csv"));
    const QString finalPath =
        QDir(outputFolder_).filePath(QStringLiteral("predictions.csv"));
    QFile partial(partialPath);
    if (!partial.open(QIODevice::ReadOnly))
        return fail(error, "Could not reopen predictions.partial.csv.");
    const QByteArray bytes = partial.readAll();
    partial.close();
    if (QFileInfo::exists(finalPath)) {
        QFile existing(finalPath);
        if (!existing.open(QIODevice::ReadOnly) || existing.readAll() != bytes)
            return fail(error, "Existing predictions.csv does not match recoverable predictions.");
    } else if (!atomicWrite(finalPath, bytes, error)) {
        return false;
    }

    data_.status = status;
    data_.endedAt = endedAt;
    data_.stopReason = stopReason;
    const QString partialSummary =
        QDir(outputFolder_)
            .filePath(QStringLiteral("model_test_summary.partial.json"));
    if (!ModelTestSummaryV2::savePartial(partialSummary, data_, predictions_, error))
        return false;
    if (failNextFinalSummaryForTest_) {
        failNextFinalSummaryForTest_ = false;
        return fail(error, "Injected final Model Test Summary failure.");
    }
    const QString finalSummary =
        QDir(outputFolder_).filePath(QStringLiteral("model_test_summary.json"));
    if (!ModelTestSummaryV2::save(finalSummary, data_, predictions_, error))
        return false;

    finalized_ = true;
    if (status != ModelTestStatus::Failed) {
        if (failPartialCsvCleanupForTest_) {
            failPartialCsvCleanupForTest_ = false;
        } else {
            QFile::remove(partialPath);
        }
        QFile::remove(partialSummary);
    }
    return true;
}

} // namespace desktop_app::v2::model_test
