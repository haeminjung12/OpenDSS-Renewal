#include "../v2/model_test/model_test_summary_v2.h"
#include "../v2/model_test/model_test_writer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace desktop_app::v2::model_test;

namespace desktop_app::v2::model_test {
struct ModelTestWriterTestAccess {
    static void failNextAppend(ModelTestWriter& writer) {
        writer.failNextAppendForTest_ = true;
    }
    static void failNextFinalSummary(ModelTestWriter& writer) {
        writer.failNextFinalSummaryForTest_ = true;
    }
    static void failPartialCsvCleanup(ModelTestWriter& writer) {
        writer.failPartialCsvCleanupForTest_ = true;
    }
};
} // namespace desktop_app::v2::model_test

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

void writeFile(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open fixture file");
    require(file.write(bytes) == bytes.size(), "write fixture file");
}

QByteArray readFile(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read file");
    return file.readAll();
}

#ifdef Q_OS_WIN
HANDLE exclusiveReadLock(const QString& path) {
    const QString nativePath = QDir::toNativeSeparators(path);
    return CreateFileW(reinterpret_cast<LPCWSTR>(nativePath.utf16()), GENERIC_READ,
                       0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool waitForLock(const std::atomic_bool& acquired) {
    for (int attempt = 0; attempt < 1000; ++attempt) {
        if (acquired.load())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return acquired.load();
}
#endif

ModelTestSummaryData data(QTemporaryDir& temporary, int classCount,
                          qint64 eligible) {
    const QString dataset = QDir(temporary.path()).filePath("dataset");
    QDir().mkpath(QDir(dataset).filePath("crops"));
    writeFile(QDir(dataset).filePath("crops/a.png"), "source-a");
    writeFile(QDir(dataset).filePath("crops/b,comma.png"), "source-b");
    writeFile(QDir(dataset).filePath("crops/c.png"), "source-c");
    ModelTestSummaryData value;
    value.testId = "writer-test";
    value.startedAt = "2026-07-24T12:00:00Z";
    value.opendssVersion = "2.0";
    value.activeModel.id = "model";
    value.activeModel.name = "Active";
    value.activeModel.onnxSha256 = QString(64, 'c');
    value.activeModel.metadataSha256 = QString(64, 'd');
    for (int index = 0; index < classCount; ++index)
        value.activeModel.classes.push_back(
            {QString("model-%1").arg(index), QString("Model %1").arg(index)});
    value.dataset.id = "dataset";
    value.dataset.sourcePath =
        QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(dataset).canonicalFilePath()));
    for (int index = 0; index < classCount; ++index)
        value.dataset.classes.push_back(
            {QString("dataset-%1").arg(index), QString("Dataset %1").arg(index)});
    value.effectiveDevice = EffectiveDevice::Cuda;
    value.eligibleImages = eligible;
    return value;
}

void testCompletedEscapingRecoveryAndSourceUntouched() {
    stage = "completed";
    QTemporaryDir temporary;
    QString error;
    auto summary = data(temporary, 3, 2);
    const QString source =
        QDir(summary.dataset.sourcePath).filePath("crops/b,comma.png");
    const QByteArray before = QCryptographicHash::hash(
        readFile(source), QCryptographicHash::Sha256);
    const QString root = QDir(temporary.path()).filePath("output");
    auto writer = ModelTestWriter::start(root, summary, &error);
    require(writer.has_value(), qPrintable(error));
    const QString partialSummary =
        QDir(root).filePath("model_test_summary.partial.json");
    const QByteArray staleSummary = readFile(partialSummary);
    require(writer->appendPrediction(
                {"crops/a.png", "dataset-0", "model-0",
                 {0.9, 0.05, 0.05}}, &error),
            qPrintable(error));
    require(writer->appendPrediction(
                {"crops/b,comma.png", "dataset-1", "model-2",
                 {0.1, 0.2, 0.7}}, &error),
            qPrintable(error));
    writeFile(partialSummary, staleSummary);
    auto recovered = ModelTestSummaryV2::load(partialSummary, &error);
    require(recovered.has_value() &&
                recovered->derivedResults().processedImages == 2 &&
                recovered->derivedResults().correctPredictions == 1,
            qPrintable(error));
    QJsonObject malformed =
        QJsonDocument::fromJson(staleSummary).object();
    QJsonObject malformedCounts = malformed.value("counts").toObject();
    malformedCounts.insert("processed", QStringLiteral("zero"));
    malformed.insert("counts", malformedCounts);
    writeFile(partialSummary, QJsonDocument(malformed).toJson());
    require(!ModelTestSummaryV2::load(partialSummary, &error),
            "partial count type remains strict");
    malformed = QJsonDocument::fromJson(staleSummary).object();
    QJsonArray malformedPerClass = malformed.value("per_class").toArray();
    QJsonObject malformedMetric = malformedPerClass.at(0).toObject();
    malformedMetric.insert("accuracy", QStringLiteral("not-a-number"));
    malformedPerClass[0] = malformedMetric;
    malformed.insert("per_class", malformedPerClass);
    writeFile(partialSummary, QJsonDocument(malformed).toJson());
    require(!ModelTestSummaryV2::load(partialSummary, &error),
            "partial accuracy type remains strict");
    writeFile(partialSummary, staleSummary);
    require(writer->finalize(ModelTestStatus::Completed,
                             "2026-07-24T12:01:00Z",
                             "all_eligible_images_processed", &error),
            qPrintable(error));
    require(QFileInfo::exists(QDir(root).filePath("predictions.csv")) &&
                QFileInfo::exists(QDir(root).filePath("model_test_summary.json")) &&
                !QFileInfo::exists(QDir(root).filePath("predictions.partial.csv")) &&
                !QFileInfo::exists(partialSummary),
            "completed publication and recovery cleanup");
    const QByteArray csv = readFile(QDir(root).filePath("predictions.csv"));
    require(csv.startsWith(
                "image_path,true_class_id,predicted_class_id,score_class_0,"
                "score_class_1,score_class_2,correct\n") &&
                csv.contains("\"crops/b,comma.png\""),
            "three-class header and CSV escaping");
    auto finalSummary = ModelTestSummaryV2::load(
        QDir(root).filePath("model_test_summary.json"), &error);
    require(finalSummary.has_value() &&
                finalSummary->derivedResults().perClass.at(2).support == 0,
            qPrintable(error));
    const QByteArray after = QCryptographicHash::hash(
        readFile(source), QCryptographicHash::Sha256);
    require(before == after, "source crop remains untouched");
}

void testRollbackAndFailedRecovery() {
    stage = "rollback";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("output");
    auto writer = ModelTestWriter::start(root, data(temporary, 2, 1), &error);
    require(writer.has_value(), qPrintable(error));
    const QByteArray initialCsv =
        readFile(QDir(root).filePath("predictions.partial.csv"));
    ModelTestWriterTestAccess::failNextAppend(*writer);
    require(!writer->appendPrediction(
                {"crops/a.png", "dataset-0", "model-0", {0.6, 0.4}},
                &error),
            "injected partial append failure");
    require(readFile(QDir(root).filePath("predictions.partial.csv")) == initialCsv &&
                writer->predictions().isEmpty(),
            "partial append rolls back exactly");
    require(writer->appendPrediction(
                {"crops/a.png", "dataset-0", "model-0", {0.6, 0.4}},
                &error),
            qPrintable(error));
    require(writer->finalize(ModelTestStatus::Failed,
                             "2026-07-24T12:00:10Z",
                             "inference_error", &error),
            qPrintable(error));
    require(QFileInfo::exists(QDir(root).filePath("predictions.partial.csv")) &&
                QFileInfo::exists(
                    QDir(root).filePath("model_test_summary.partial.json")) &&
                QFileInfo::exists(QDir(root).filePath("predictions.csv")) &&
                QFileInfo::exists(
                    QDir(root).filePath("model_test_summary.json")),
            "failed test retains factual recovery and final artifacts");
}

void testPartialSummaryCommitRetryAndRollback() {
    stage = "partial-summary-commit";
#ifdef Q_OS_WIN
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("retry-output");
    auto writer = ModelTestWriter::start(root, data(temporary, 2, 1), &error);
    require(writer.has_value(), qPrintable(error));
    const QString partialSummary =
        QDir(root).filePath("model_test_summary.partial.json");
    std::atomic_bool acquired = false;
    std::atomic_bool lockFailed = false;
    std::thread unlocker([&] {
        const HANDLE handle = exclusiveReadLock(partialSummary);
        if (handle == INVALID_HANDLE_VALUE) {
            lockFailed = true;
            acquired = true;
            return;
        }
        acquired = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        CloseHandle(handle);
    });
    const bool lockReady = waitForLock(acquired);
    if (!lockReady || lockFailed) {
        unlocker.join();
        require(false, "acquire an exclusive partial summary lock");
    }
    QElapsedTimer elapsed;
    elapsed.start();
    const bool appended = writer->appendPrediction(
        {"crops/a.png", "dataset-0", "model-0", {0.6, 0.4}}, &error);
    const qint64 elapsedMs = elapsed.elapsed();
    unlocker.join();
    require(appended, qPrintable(error));
    require(elapsedMs >= 25,
            "partial summary sharing conflict was retried before publication");
    auto recovered = ModelTestSummaryV2::load(partialSummary, &error);
    require(recovered.has_value() &&
                recovered->derivedResults().processedImages == 1 &&
                readFile(QDir(root).filePath("predictions.partial.csv")).count('\n') == 2,
            qPrintable(error));

    const QString failedRoot = QDir(temporary.path()).filePath("failed-output");
    auto failedWriter =
        ModelTestWriter::start(failedRoot, data(temporary, 2, 1), &error);
    require(failedWriter.has_value(), qPrintable(error));
    const QString failedSummary =
        QDir(failedRoot).filePath("model_test_summary.partial.json");
    const QByteArray initialCsv =
        readFile(QDir(failedRoot).filePath("predictions.partial.csv"));
    const HANDLE handle = exclusiveReadLock(failedSummary);
    require(handle != INVALID_HANDLE_VALUE,
            "acquire a persistent exclusive partial summary lock");
    const bool failedAppend = failedWriter->appendPrediction(
        {"crops/a.png", "dataset-0", "model-0", {0.6, 0.4}}, &error);
    CloseHandle(handle);
    require(!failedAppend &&
                failedWriter->predictions().isEmpty() &&
                readFile(QDir(failedRoot).filePath("predictions.partial.csv")) == initialCsv,
            "permanent partial summary failure rolls back the prediction CSV");
    recovered = ModelTestSummaryV2::load(failedSummary, &error);
    require(recovered.has_value() &&
                recovered->derivedResults().processedImages == 0,
            qPrintable(error));
#endif
}

void testFinalSummaryLastAndRetry() {
    stage = "final-summary-last";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("output");
    auto writer = ModelTestWriter::start(root, data(temporary, 2, 1), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendPrediction(
                {"crops/a.png", "dataset-0", "model-0", {1.0, 0.0}},
                &error),
            qPrintable(error));
    ModelTestWriterTestAccess::failNextFinalSummary(*writer);
    require(!writer->finalize(ModelTestStatus::Completed,
                              "2026-07-24T12:00:05Z",
                              "all_eligible_images_processed", &error),
            "injected summary publication failure");
    require(QFileInfo::exists(QDir(root).filePath("predictions.csv")) &&
                QFileInfo::exists(
                    QDir(root).filePath("model_test_summary.partial.json")),
            "predictions publish before summary and recovery remains");
    require(writer->finalize(ModelTestStatus::Completed,
                             "2026-07-24T12:00:05Z",
                             "all_eligible_images_processed", &error),
            qPrintable(error));
}

void testUniqueFolderAndStoppedTwoClass() {
    stage = "stopped";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("output");
    QDir().mkpath(root);
    require(!ModelTestWriter::start(root, data(temporary, 2, 1), &error),
            "existing output folder rejected");
    require(QDir().rmdir(root), "remove empty existing folder");
    auto writer = ModelTestWriter::start(root, data(temporary, 2, 1), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->finalize(ModelTestStatus::Stopped,
                             "2026-07-24T12:00:01Z",
                             "user_stopped", &error),
            qPrintable(error));
    const QByteArray csv = readFile(QDir(root).filePath("predictions.csv"));
    require(csv ==
                "image_path,true_class_id,predicted_class_id,score_class_0,"
                "score_class_1,correct\n",
            "two-class CSV omits score_class_2");
}

void testCanonicalSuccessSurvivesCleanupFailure() {
    stage = "cleanup";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("output");
    auto writer = ModelTestWriter::start(root, data(temporary, 2, 1), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendPrediction(
                {"crops/a.png", "dataset-0", "model-0", {0.8, 0.2}},
                &error),
            qPrintable(error));
    ModelTestWriterTestAccess::failPartialCsvCleanup(*writer);
    require(writer->finalize(ModelTestStatus::Completed,
                             "2026-07-24T12:00:05Z",
                             "all_eligible_images_processed", &error),
            qPrintable(error));
    const QString finalCsv = QDir(root).filePath("predictions.csv");
    const QString finalSummary =
        QDir(root).filePath("model_test_summary.json");
    const QString stalePartial =
        QDir(root).filePath("predictions.partial.csv");
    require(QFileInfo::exists(stalePartial) &&
                !QFileInfo::exists(
                    QDir(root).filePath("model_test_summary.partial.json")),
            "partial cleanup attempts are independent");
    require(ModelTestSummaryV2::load(finalSummary, &error).has_value(),
            qPrintable(error));
    const QByteArray canonicalCsv = readFile(finalCsv);
    const QByteArray canonicalSummary = readFile(finalSummary);
    const QByteArray partialCsv = readFile(stalePartial);
    require(!writer->finalize(ModelTestStatus::Completed,
                              "2026-07-24T12:00:05Z",
                              "all_eligible_images_processed", &error),
            "finalized writer refuses retry");
    require(readFile(finalCsv) == canonicalCsv &&
                readFile(finalSummary) == canonicalSummary &&
                readFile(stalePartial) == partialCsv,
            "retry cannot rewrite canonical or truncate stale partial");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testCompletedEscapingRecoveryAndSourceUntouched();
    testRollbackAndFailedRecovery();
    testPartialSummaryCommitRetryAndRollback();
    testFinalSummaryLastAndRetry();
    testUniqueFolderAndStoppedTwoClass();
    testCanonicalSuccessSurvivesCleanupFailure();
    return 0;
}
