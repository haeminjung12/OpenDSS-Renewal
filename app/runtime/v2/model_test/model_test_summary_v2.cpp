#include "model_test_summary_v2.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QThread>

#include <cmath>

namespace {

using namespace desktop_app::v2::model_test;

bool fail(QString* error, const QString& message) {
    if (error)
        *error = message;
    return false;
}

template <typename T>
std::optional<T> failed(QString* error, const QString& message) {
    fail(error, message);
    return std::nullopt;
}

bool exactKeys(const QJsonObject& object, const QSet<QString>& required,
               const QSet<QString>& optional = {}) {
    const QStringList keyList = object.keys();
    const QSet<QString> keys(keyList.begin(), keyList.end());
    for (const QString& key : required) {
        if (!keys.contains(key))
            return false;
    }
    return (keys - required - optional).isEmpty();
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

std::optional<ModelTestStatus> parseStatus(const QString& text) {
    if (text == "completed")
        return ModelTestStatus::Completed;
    if (text == "stopped")
        return ModelTestStatus::Stopped;
    if (text == "failed")
        return ModelTestStatus::Failed;
    return std::nullopt;
}

QString deviceText(EffectiveDevice device) {
    if (device == EffectiveDevice::Cpu)
        return QStringLiteral("cpu");
    if (device == EffectiveDevice::Cuda)
        return QStringLiteral("cuda");
    return {};
}

std::optional<EffectiveDevice> parseDevice(const QString& text) {
    if (text == "cpu")
        return EffectiveDevice::Cpu;
    if (text == "cuda")
        return EffectiveDevice::Cuda;
    return std::nullopt;
}

bool validSha(const QString& value) {
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
    return expression.match(value).hasMatch();
}

bool validClasses(const QVector<ModelTestClassSnapshot>& classes) {
    if (classes.size() != 2 && classes.size() != 3)
        return false;
    QSet<QString> ids;
    QSet<QString> names;
    for (const auto& cls : classes) {
        const QString id = cls.id.trimmed();
        const QString name = cls.name.trimmed();
        if (id.isEmpty() || name.isEmpty() || ids.contains(id) ||
            names.contains(name.toCaseFolded())) {
            return false;
        }
        ids.insert(id);
        names.insert(name.toCaseFolded());
    }
    return true;
}

int classIndex(const QVector<ModelTestClassSnapshot>& classes,
               const QString& id) {
    for (int index = 0; index < classes.size(); ++index) {
        if (classes.at(index).id == id)
            return index;
    }
    return -1;
}

bool safeRelativePath(const QString& path) {
    return !path.isEmpty() && !QDir::isAbsolutePath(path) && !path.contains('\\') &&
           QDir::cleanPath(path) == path && path != ".." && !path.startsWith("../");
}

bool validateData(const ModelTestSummaryData& data, bool requireDataset,
                  QString* error) {
    const auto started = QDateTime::fromString(data.startedAt, Qt::ISODate);
    const auto ended = QDateTime::fromString(data.endedAt, Qt::ISODate);
    if (data.testId.trimmed().isEmpty() || statusText(data.status).isEmpty() ||
        !started.isValid() || !ended.isValid() || ended < started ||
        data.stopReason.trimmed().isEmpty() ||
        data.opendssVersion.trimmed().isEmpty() ||
        data.activeModel.id.trimmed().isEmpty() ||
        data.activeModel.name.trimmed().isEmpty() ||
        !validSha(data.activeModel.onnxSha256) ||
        !validSha(data.activeModel.metadataSha256) ||
        !validClasses(data.activeModel.classes) ||
        data.dataset.id.trimmed().isEmpty() ||
        !validClasses(data.dataset.classes) ||
        data.activeModel.classes.size() != data.dataset.classes.size() ||
        deviceText(data.effectiveDevice).isEmpty() || data.eligibleImages < 0 ||
        data.predictionsCsv != "predictions.csv" ||
        (data.effectiveDevice == EffectiveDevice::Cuda &&
         data.fallbackWarning.has_value()) ||
        (data.effectiveDevice == EffectiveDevice::Cpu &&
         (!data.fallbackWarning ||
          data.fallbackWarning->trimmed().isEmpty()))) {
        return fail(error, "Model Test Summary metadata is invalid.");
    }
    const QString normalizedSource =
        QDir::cleanPath(QDir::fromNativeSeparators(data.dataset.sourcePath));
    if (!QDir::isAbsolutePath(normalizedSource) ||
        normalizedSource != data.dataset.sourcePath) {
        return fail(error, "Dataset source_path must be canonical and absolute.");
    }
    if (requireDataset) {
        const QFileInfo source(data.dataset.sourcePath);
        const QString canonical =
            QDir::cleanPath(QDir::fromNativeSeparators(source.canonicalFilePath()));
        if (!source.exists() || !source.isDir() || source.isSymLink() ||
            canonical != data.dataset.sourcePath) {
            return fail(error, "Dataset source_path is not a readable canonical directory.");
        }
    }
    return true;
}

bool requireInteger(const QJsonValue& value, qint64* output) {
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < 0.0 || number > 9007199254740991.0)
        return false;
    *output = static_cast<qint64>(number);
    return true;
}

QJsonArray classesJson(const QVector<ModelTestClassSnapshot>& classes) {
    QJsonArray array;
    for (const auto& cls : classes)
        array.append(QJsonObject{{"id", cls.id}, {"name", cls.name}});
    return array;
}

std::optional<QVector<ModelTestClassSnapshot>>
parseClasses(const QJsonValue& value, QString* error) {
    if (!value.isArray())
        return failed<QVector<ModelTestClassSnapshot>>(error, "classes must be an array.");
    QVector<ModelTestClassSnapshot> classes;
    for (const auto& item : value.toArray()) {
        if (!item.isObject())
            return failed<QVector<ModelTestClassSnapshot>>(error, "Class entry is invalid.");
        const QJsonObject object = item.toObject();
        if (!exactKeys(object, {"id", "name"}) || !object.value("name").isString()) {
            return failed<QVector<ModelTestClassSnapshot>>(error, "Class entry is invalid.");
        }
        if (!object.value("id").isString())
            return failed<QVector<ModelTestClassSnapshot>>(error, "Class ID is invalid.");
        classes.push_back({object.value("id").toString(),
                           object.value("name").toString()});
    }
    if (!validClasses(classes))
        return failed<QVector<ModelTestClassSnapshot>>(
            error, "Classes require unique nonempty IDs and names.");
    return classes;
}

QJsonValue optionalAccuracy(const std::optional<double>& value) {
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonObject summaryJson(const ModelTestSummaryData& data,
                        const ModelTestDerivedResults& derived) {
    QJsonArray perClass;
    for (const auto& item : derived.perClass) {
        perClass.append(QJsonObject{{"class_id", item.classId},
                                    {"support", item.support},
                                    {"correct", item.correct},
                                    {"accuracy", optionalAccuracy(item.accuracy)}});
    }
    QJsonArray confusion;
    for (const auto& row : derived.confusionMatrix) {
        QJsonArray jsonRow;
        for (qint64 count : row)
            jsonRow.append(count);
        confusion.append(jsonRow);
    }
    QJsonObject device{{"policy", ModelTestSummaryV2::DevicePolicy},
                       {"effective", deviceText(data.effectiveDevice)}};
    if (data.fallbackWarning)
        device.insert("fallback_warning", *data.fallbackWarning);
    return QJsonObject{
        {"schema", ModelTestSummaryV2::SchemaVersion},
        {"test_id", data.testId},
        {"status", statusText(data.status)},
        {"timestamps",
         QJsonObject{{"started_at", data.startedAt}, {"ended_at", data.endedAt}}},
        {"stop_reason", data.stopReason},
        {"opendss_version", data.opendssVersion},
        {"active_model",
         QJsonObject{{"id", data.activeModel.id},
                     {"name", data.activeModel.name},
                     {"onnx_sha256", data.activeModel.onnxSha256},
                     {"metadata_sha256", data.activeModel.metadataSha256},
                     {"classes", classesJson(data.activeModel.classes)}}},
        {"dataset",
         QJsonObject{{"id", data.dataset.id},
                     {"source_path", data.dataset.sourcePath},
                     {"classes", classesJson(data.dataset.classes)}}},
        {"device", device},
        {"counts",
         QJsonObject{{"eligible", data.eligibleImages},
                     {"processed", derived.processedImages},
                     {"correct", derived.correctPredictions}}},
        {"overall_accuracy", optionalAccuracy(derived.overallAccuracy)},
        {"per_class", perClass},
        {"confusion_matrix", confusion},
        {"predictions_csv", data.predictionsCsv},
    };
}

bool atomicWrite(const QString& path, const QByteArray& bytes, bool replace,
                 QString* error) {
    if (!replace && QFileInfo::exists(path))
        return fail(error, QString("Refusing to replace existing artifact '%1'.").arg(path));
    constexpr int commitAttempts = 8;
    constexpr unsigned long commitRetryDelayMs = 25;
    const int attempts = replace ? commitAttempts : 1;
    QString commitError;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly))
            return fail(error, QString("Could not open '%1': %2").arg(path, file.errorString()));
        if (file.write(bytes) != bytes.size()) {
            file.cancelWriting();
            return fail(error, QString("Could not completely write '%1'.").arg(path));
        }
        if (file.commit())
            return true;
        commitError = file.errorString();
        if (file.error() != QFileDevice::RenameError || attempt + 1 == attempts)
            return fail(error, QString("Could not publish '%1': %2").arg(path, commitError));
        QThread::msleep(commitRetryDelayMs);
    }
    return fail(error, QString("Could not publish '%1': %2").arg(path, commitError));
}

std::optional<QVector<QStringList>> parseCsvRecords(const QByteArray& bytes,
                                                    QString* error) {
    QVector<QStringList> records;
    QStringList record;
    QString field;
    bool quoted = false;
    bool afterQuote = false;
    const QString text = QString::fromUtf8(bytes);
    for (int index = 0; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        if (quoted) {
            if (ch == '"') {
                if (index + 1 < text.size() && text.at(index + 1) == '"') {
                    field += '"';
                    ++index;
                } else {
                    quoted = false;
                    afterQuote = true;
                }
            } else {
                field += ch;
            }
            continue;
        }
        if (afterQuote && ch != ',' && ch != '\r' && ch != '\n')
            return failed<QVector<QStringList>>(error, "Malformed predictions CSV.");
        if (ch == '"' && field.isEmpty() && !afterQuote) {
            quoted = true;
        } else if (ch == ',') {
            record.push_back(field);
            field.clear();
            afterQuote = false;
        } else if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && index + 1 < text.size() && text.at(index + 1) == '\n')
                ++index;
            record.push_back(field);
            field.clear();
            afterQuote = false;
            records.push_back(record);
            record.clear();
        } else {
            field += ch;
        }
    }
    if (quoted)
        return failed<QVector<QStringList>>(error, "Unterminated quoted CSV field.");
    if (!field.isEmpty() || !record.isEmpty()) {
        record.push_back(field);
        records.push_back(record);
    }
    return records;
}

std::optional<QVector<ModelTestPrediction>>
loadPredictions(const QString& path, const ModelTestSummaryData& data,
                QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failed<QVector<ModelTestPrediction>>(
            error, QString("Could not read '%1': %2").arg(path, file.errorString()));
    auto records = parseCsvRecords(file.readAll(), error);
    if (!records || records->isEmpty())
        return failed<QVector<ModelTestPrediction>>(error, "Predictions CSV has no header.");
    QStringList expected{"image_path", "true_class_id", "predicted_class_id"};
    for (int index = 0; index < data.activeModel.classes.size(); ++index)
        expected.push_back(QString("score_class_%1").arg(index));
    expected.push_back("correct");
    if (records->takeFirst() != expected)
        return failed<QVector<ModelTestPrediction>>(error, "Predictions CSV header is invalid.");

    QVector<ModelTestPrediction> predictions;
    for (const auto& fields : *records) {
        if (fields.size() != expected.size())
            return failed<QVector<ModelTestPrediction>>(error, "Predictions CSV row width is invalid.");
        ModelTestPrediction prediction;
        prediction.imagePath = fields.at(0);
        prediction.trueClassId = fields.at(1);
        prediction.predictedClassId = fields.at(2);
        for (int index = 0; index < data.activeModel.classes.size(); ++index) {
            bool scoreOk = false;
            const double score = fields.at(3 + index).toDouble(&scoreOk);
            if (!scoreOk || !std::isfinite(score))
                return failed<QVector<ModelTestPrediction>>(error, "Prediction score is invalid.");
            prediction.scores.push_back(score);
        }
        const QString correct = fields.constLast();
        const auto expectedCorrect =
            ModelTestSummaryV2::predictionCorrect(data, prediction, error);
        if ((correct != "true" && correct != "false") ||
            !expectedCorrect || (correct == "true") != *expectedCorrect ||
            !ModelTestSummaryV2::validatePrediction(data, prediction, false, error)) {
            return std::nullopt;
        }
        predictions.push_back(std::move(prediction));
    }
    return predictions;
}

bool sameOptionalDouble(const QJsonValue& value, const std::optional<double>& expected) {
    if (!expected)
        return value.isNull();
    return value.isDouble() && std::isfinite(value.toDouble()) &&
           value.toDouble() == *expected;
}

bool validStoredAccuracy(const QJsonValue& value, bool hasSupport) {
    if (!hasSupport)
        return value.isNull();
    return value.isDouble() && std::isfinite(value.toDouble()) &&
           value.toDouble() >= 0.0 && value.toDouble() <= 1.0;
}

} // namespace

namespace desktop_app::v2::model_test {

bool ModelTestSummaryV2::validateInitial(const ModelTestSummaryData& data,
                                         QString* error) {
    if (error)
        error->clear();
    return validateData(data, true, error);
}

bool ModelTestSummaryV2::validatePrediction(
    const ModelTestSummaryData& data, const ModelTestPrediction& prediction,
    bool requireReadableSource, QString* error) {
    if (error)
        error->clear();
    const int count = data.activeModel.classes.size();
    const int trueIndex = classIndex(data.dataset.classes,
                                     prediction.trueClassId);
    const int predictedIndex = classIndex(data.activeModel.classes,
                                          prediction.predictedClassId);
    if (!safeRelativePath(prediction.imagePath) ||
        trueIndex < 0 || predictedIndex < 0 ||
        prediction.scores.size() != count) {
        return fail(error, "Prediction path, class IDs, or score count is invalid.");
    }
    int argmax = 0;
    for (int index = 0; index < prediction.scores.size(); ++index) {
        if (!std::isfinite(prediction.scores.at(index)))
            return fail(error, "Class Scores must be finite.");
        if (prediction.scores.at(index) > prediction.scores.at(argmax))
            argmax = index;
    }
    if (predictedIndex != argmax)
        return fail(error, "predicted_class_id must be the first argmax Class Score.");
    if (!requireReadableSource)
        return true;

    const QString root = data.dataset.sourcePath;
    QString current = root;
    const QStringList parts = prediction.imagePath.split('/');
    for (const QString& part : parts) {
        current = QDir(current).filePath(part);
        const QFileInfo info(current);
        if (!info.exists() || info.isSymLink())
            return fail(error, "Prediction image path is missing or linked.");
    }
    const QFileInfo image(current);
    const QString canonicalRoot =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath()));
    const QString canonicalImage =
        QDir::cleanPath(QDir::fromNativeSeparators(image.canonicalFilePath()));
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    if (!image.isFile() || !image.isReadable() || canonicalImage.isEmpty() ||
        (canonicalImage != canonicalRoot &&
         !canonicalImage.startsWith(canonicalRoot + '/', sensitivity))) {
        return fail(error, "Prediction image path escapes the Dataset or is unreadable.");
    }
    return true;
}

std::optional<bool> ModelTestSummaryV2::predictionCorrect(
    const ModelTestSummaryData& data, const ModelTestPrediction& prediction,
    QString* error) {
    if (error)
        error->clear();
    const int trueIndex = classIndex(data.dataset.classes,
                                     prediction.trueClassId);
    const int predictedIndex = classIndex(data.activeModel.classes,
                                          prediction.predictedClassId);
    if (trueIndex < 0 || predictedIndex < 0)
        return failed<bool>(error, "Prediction class ID is not present in its snapshot.");
    return trueIndex == predictedIndex;
}

std::optional<ModelTestDerivedResults>
ModelTestSummaryV2::derive(const ModelTestSummaryData& data,
                           const QVector<ModelTestPrediction>& predictions,
                           QString* error) {
    if (error)
        error->clear();
    if (!validateData(data, false, error))
        return std::nullopt;
    if (predictions.size() > data.eligibleImages)
        return failed<ModelTestDerivedResults>(error, "Processed count exceeds eligible count.");
    if (data.status == ModelTestStatus::Completed &&
        predictions.size() != data.eligibleImages) {
        return failed<ModelTestDerivedResults>(
            error, "Completed Model Test must process every eligible image.");
    }

    ModelTestDerivedResults result;
    const int count = data.activeModel.classes.size();
    result.perClass.resize(count);
    result.confusionMatrix.resize(count);
    for (int index = 0; index < count; ++index) {
        result.perClass[index].classId = data.dataset.classes.at(index).id;
        result.confusionMatrix[index].fill(0, count);
    }
    QSet<QString> paths;
    for (const auto& prediction : predictions) {
        if (!validatePrediction(data, prediction, false, error))
            return std::nullopt;
        if (paths.contains(prediction.imagePath))
            return failed<ModelTestDerivedResults>(error, "Prediction image paths must be unique.");
        paths.insert(prediction.imagePath);
        ++result.processedImages;
        const int trueIndex = classIndex(data.dataset.classes,
                                         prediction.trueClassId);
        const int predictedIndex = classIndex(data.activeModel.classes,
                                              prediction.predictedClassId);
        auto& metrics = result.perClass[trueIndex];
        ++metrics.support;
        ++result.confusionMatrix[trueIndex][predictedIndex];
        const auto correct = predictionCorrect(data, prediction, error);
        if (!correct)
            return std::nullopt;
        if (*correct) {
            ++result.correctPredictions;
            ++metrics.correct;
        }
    }
    if (result.processedImages > 0) {
        result.overallAccuracy =
            static_cast<double>(result.correctPredictions) / result.processedImages;
    }
    for (auto& metrics : result.perClass) {
        if (metrics.support > 0)
            metrics.accuracy = static_cast<double>(metrics.correct) / metrics.support;
    }
    return result;
}

bool ModelTestSummaryV2::save(const QString& path,
                              const ModelTestSummaryData& data,
                              const QVector<ModelTestPrediction>& predictions,
                              QString* error) {
    if (error)
        error->clear();
    if (!validateData(data, true, error))
        return false;
    for (const auto& prediction : predictions) {
        if (!validatePrediction(data, prediction, true, error))
            return false;
    }
    auto derived = derive(data, predictions, error);
    if (!derived)
        return false;
    return atomicWrite(path, QJsonDocument(summaryJson(data, *derived)).toJson(),
                       false, error);
}

bool ModelTestSummaryV2::savePartial(
    const QString& path, const ModelTestSummaryData& data,
    const QVector<ModelTestPrediction>& predictions, QString* error) {
    if (error)
        error->clear();
    if (!validateData(data, true, error))
        return false;
    for (const auto& prediction : predictions) {
        if (!validatePrediction(data, prediction, true, error))
            return false;
    }
    return savePartialWithValidatedSources(path, data, predictions, error);
}

bool ModelTestSummaryV2::savePartialWithValidatedSources(
    const QString& path, const ModelTestSummaryData& data,
    const QVector<ModelTestPrediction>& predictions, QString* error) {
    if (error)
        error->clear();
    if (!validateData(data, true, error))
        return false;
    auto derived = derive(data, predictions, error);
    if (!derived)
        return false;
    return atomicWrite(path, QJsonDocument(summaryJson(data, *derived)).toJson(),
                       true, error);
}

std::optional<ModelTestSummaryV2>
ModelTestSummaryV2::load(const QString& path, QString* error) {
    if (error)
        error->clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failed<ModelTestSummaryV2>(
            error, QString("Could not read '%1': %2").arg(path, file.errorString()));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return failed<ModelTestSummaryV2>(error, "Model Test Summary JSON is invalid.");
    const QJsonObject root = document.object();
    const QSet<QString> rootKeys{
        "schema",          "test_id",          "status",
        "timestamps",      "stop_reason",      "opendss_version",
        "active_model",    "dataset",          "device",
        "counts",          "overall_accuracy", "per_class",
        "confusion_matrix", "predictions_csv"};
    if (!exactKeys(root, rootKeys) ||
        root.value("schema").toString() != SchemaVersion ||
        !root.value("test_id").isString() || !root.value("status").isString() ||
        !root.value("timestamps").isObject() ||
        !root.value("stop_reason").isString() ||
        !root.value("opendss_version").isString() ||
        !root.value("active_model").isObject() ||
        !root.value("dataset").isObject() || !root.value("device").isObject() ||
        !root.value("counts").isObject() || !root.value("per_class").isArray() ||
        !root.value("confusion_matrix").isArray() ||
        root.value("predictions_csv").toString() != "predictions.csv") {
        return failed<ModelTestSummaryV2>(error, "Model Test Summary schema is invalid.");
    }

    ModelTestSummaryV2 result;
    auto status = parseStatus(root.value("status").toString());
    if (!status)
        return failed<ModelTestSummaryV2>(error, "Model Test status is invalid.");
    result.data_.testId = root.value("test_id").toString();
    result.data_.status = *status;
    result.data_.stopReason = root.value("stop_reason").toString();
    result.data_.opendssVersion = root.value("opendss_version").toString();

    const QJsonObject timestamps = root.value("timestamps").toObject();
    if (!exactKeys(timestamps, {"started_at", "ended_at"}) ||
        !timestamps.value("started_at").isString() ||
        !timestamps.value("ended_at").isString()) {
        return failed<ModelTestSummaryV2>(error, "Model Test timestamps are invalid.");
    }
    result.data_.startedAt = timestamps.value("started_at").toString();
    result.data_.endedAt = timestamps.value("ended_at").toString();

    const QJsonObject model = root.value("active_model").toObject();
    if (!exactKeys(model, {"id", "name", "onnx_sha256", "metadata_sha256", "classes"}) ||
        !model.value("id").isString() || !model.value("name").isString() ||
        !model.value("onnx_sha256").isString() ||
        !model.value("metadata_sha256").isString()) {
        return failed<ModelTestSummaryV2>(error, "Active Model snapshot is invalid.");
    }
    auto modelClasses = parseClasses(model.value("classes"), error);
    if (!modelClasses)
        return std::nullopt;
    result.data_.activeModel = {
        model.value("id").toString(), model.value("name").toString(),
        model.value("onnx_sha256").toString(),
        model.value("metadata_sha256").toString(), *modelClasses};

    const QJsonObject dataset = root.value("dataset").toObject();
    if (!exactKeys(dataset, {"id", "source_path", "classes"}) ||
        !dataset.value("id").isString() || !dataset.value("source_path").isString()) {
        return failed<ModelTestSummaryV2>(error, "Dataset snapshot is invalid.");
    }
    auto datasetClasses = parseClasses(dataset.value("classes"), error);
    if (!datasetClasses)
        return std::nullopt;
    result.data_.dataset = {dataset.value("id").toString(),
                            dataset.value("source_path").toString(), *datasetClasses};

    const QJsonObject device = root.value("device").toObject();
    if (!exactKeys(device, {"policy", "effective"}, {"fallback_warning"}) ||
        device.value("policy").toString() != DevicePolicy ||
        !device.value("effective").isString() ||
        (device.contains("fallback_warning") &&
         !device.value("fallback_warning").isString())) {
        return failed<ModelTestSummaryV2>(error, "Device snapshot is invalid.");
    }
    auto effective = parseDevice(device.value("effective").toString());
    if (!effective)
        return failed<ModelTestSummaryV2>(error, "Effective device is invalid.");
    result.data_.effectiveDevice = *effective;
    if (device.contains("fallback_warning"))
        result.data_.fallbackWarning = device.value("fallback_warning").toString();

    const QJsonObject counts = root.value("counts").toObject();
    qint64 storedEligible = 0;
    qint64 storedProcessed = 0;
    qint64 storedCorrect = 0;
    if (!exactKeys(counts, {"eligible", "processed", "correct"}) ||
        !requireInteger(counts.value("eligible"), &storedEligible) ||
        !requireInteger(counts.value("processed"), &storedProcessed) ||
        !requireInteger(counts.value("correct"), &storedCorrect) ||
        storedProcessed > storedEligible || storedCorrect > storedProcessed) {
        return failed<ModelTestSummaryV2>(error, "Model Test counts are invalid.");
    }
    result.data_.eligibleImages = storedEligible;
    if (!validateData(result.data_, false, error))
        return std::nullopt;

    const bool partial = QFileInfo(path).fileName().endsWith(".partial.json");
    const QString csvPath = QDir(QFileInfo(path).absolutePath())
                                .filePath(partial ? "predictions.partial.csv"
                                                  : "predictions.csv");
    auto predictions = loadPredictions(csvPath, result.data_, error);
    if (!predictions)
        return std::nullopt;
    auto derived = derive(result.data_, *predictions, error);
    if (!derived)
        return std::nullopt;
    if (!validStoredAccuracy(root.value("overall_accuracy"),
                             storedProcessed > 0) ||
        (!partial &&
         (storedProcessed != derived->processedImages ||
          storedCorrect != derived->correctPredictions ||
          !sameOptionalDouble(root.value("overall_accuracy"),
                              derived->overallAccuracy)))) {
        return failed<ModelTestSummaryV2>(error, "Stored Model Test metrics disagree with predictions CSV.");
    }

    const QJsonArray perClass = root.value("per_class").toArray();
    if (perClass.size() != derived->perClass.size())
        return failed<ModelTestSummaryV2>(error, "Per-class metrics size is invalid.");
    for (int index = 0; index < perClass.size(); ++index) {
        if (!perClass.at(index).isObject())
            return failed<ModelTestSummaryV2>(error, "Per-class metric is invalid.");
        const QJsonObject item = perClass.at(index).toObject();
        qint64 support = 0;
        qint64 correct = 0;
        const auto& expected = derived->perClass.at(index);
        if (!exactKeys(item, {"class_id", "support", "correct", "accuracy"}) ||
            !item.value("class_id").isString() ||
            !requireInteger(item.value("support"), &support) ||
            !requireInteger(item.value("correct"), &correct) ||
            item.value("class_id").toString() != expected.classId ||
            correct > support ||
            !validStoredAccuracy(item.value("accuracy"), support > 0) ||
            (!partial && (support != expected.support ||
                          correct != expected.correct ||
                          !sameOptionalDouble(item.value("accuracy"),
                                              expected.accuracy)))) {
            return failed<ModelTestSummaryV2>(error, "Per-class metrics disagree with predictions CSV.");
        }
    }
    const QJsonArray confusion = root.value("confusion_matrix").toArray();
    if (confusion.size() != derived->confusionMatrix.size())
        return failed<ModelTestSummaryV2>(error, "Confusion matrix size is invalid.");
    for (int row = 0; row < confusion.size(); ++row) {
        if (!confusion.at(row).isArray() ||
            confusion.at(row).toArray().size() != derived->confusionMatrix.size()) {
            return failed<ModelTestSummaryV2>(error, "Confusion matrix row is invalid.");
        }
        for (int column = 0; column < confusion.at(row).toArray().size(); ++column) {
            qint64 count = 0;
            if (!requireInteger(confusion.at(row).toArray().at(column), &count) ||
                (!partial &&
                 count != derived->confusionMatrix.at(row).at(column))) {
                return failed<ModelTestSummaryV2>(error, "Confusion matrix disagrees with predictions CSV.");
            }
        }
    }
    result.predictions_ = std::move(*predictions);
    result.derived_ = std::move(*derived);
    return result;
}

} // namespace desktop_app::v2::model_test
