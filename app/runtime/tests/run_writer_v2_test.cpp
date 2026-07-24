#include "../v2/run/run_manifest_v2.h"
#include "../v2/run/run_writer_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace desktop_app::v2::run;

namespace desktop_app::v2::run {
struct RunWriterV2TestAccess {
    static void failNextAppend(RunWriterV2& writer) {
        writer.failNextCsvAppendForTest_ = true;
    }
};
} // namespace desktop_app::v2::run

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

RunManifestData data() {
    RunManifestData value;
    value.runId = "run";
    value.runName = "Run";
    value.startedAt = "2026-07-24T12:00:00Z";
    value.endedAt = "2026-07-24T12:00:01Z";
    value.stopReason = "user";
    value.opendssVersion = "2";
    value.sourceSequence = {"seq", "Sequence", "source/sequence.json"};
    value.routing.triggerMode = TriggerMode::EveryDroplet;
    value.requestedProcessingFps = 10.0;
    value.achievedProcessingFps = 10.0;
    value.cameraSettings = {{"exposure_us", 100}};
    value.hitBoundary = {180.0, HitSide::PositiveY, 1200, 360};
    return value;
}

RunEvent event() {
    RunEvent value;
    value.eventId = "event,\"quoted\"";
    value.detectionTimestamp = "2026-07-24T12:00:00Z";
    value.sourceFrameIndex = 7;
    value.cropPath = "crops/crop,7.png";
    value.decision = Route::Hit;
    value.observedRoute = Route::Unresolved;
    value.daqPulseStatus = DaqPulseStatus::SuppressedNotIssued;
    return value;
}

void testCompletedAndEscaping() {
    stage = "completed";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("completed");
    auto writer = RunWriterV2::start(root, data(), &error);
    require(writer.has_value(), qPrintable(error));
    const QString partialSummary = QDir(root).filePath("run_summary.partial.json");
    auto atStart = RunManifestV2::load(partialSummary, &error);
    require(atStart.has_value() && atStart->data().events.isEmpty() &&
                atStart->data().status == RunStatus::Interrupted,
            qPrintable(error));
    QFile oldPartialFile(partialSummary);
    require(oldPartialFile.open(QIODevice::ReadOnly), "read starting partial summary");
    const QByteArray oldPartialSummary = oldPartialFile.readAll();
    oldPartialFile.close();
    const QByteArray cropBytes("preserved-source-bytes\0tail", 27);
    require(writer->appendEvent(event(), cropBytes, &error), qPrintable(error));
    auto afterAppend = RunManifestV2::load(partialSummary, &error);
    require(afterAppend.has_value() && afterAppend->data().events.size() == 1,
            qPrintable(error));
    require(oldPartialFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "restore stale partial summary");
    require(oldPartialFile.write(oldPartialSummary) == oldPartialSummary.size(),
            "write stale partial summary");
    oldPartialFile.close();
    auto crashWindow = RunManifestV2::load(partialSummary, &error);
    require(crashWindow.has_value() && crashWindow->data().events.size() == 1 &&
                crashWindow->derivedCounts().total == 1,
            "flushed partial CSV recovers across stale-summary crash window");
    require(writer->flush(&error), qPrintable(error));
    require(QFileInfo::exists(QDir(root).filePath("events.partial.csv")),
            "partial exists while active");
    require(writer->finalize(RunStatus::Completed, "2026-07-24T12:00:01Z",
                             "user", 10.0, &error),
            qPrintable(error));
    require(!QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
                !QFileInfo::exists(partialSummary) &&
                QFileInfo::exists(QDir(root).filePath("events.csv")),
            "clean finalization renames partial");
    QFile crop(QDir(root).filePath(event().cropPath));
    require(crop.open(QIODevice::ReadOnly) && crop.readAll() == cropBytes,
            "crop bytes preserved exactly");
    auto loaded = RunManifestV2::load(QDir(root).filePath("run_summary.json"),
                                      &error);
    require(loaded.has_value(), qPrintable(error));
    require(loaded->data().events.at(0).eventId == event().eventId &&
                loaded->data().events.at(0).cropPath == event().cropPath,
            "quoted CSV fields reopen exactly");
}

void testInterruptedAndFailedRecovery() {
    stage = "interrupted";
    for (RunStatus status : {RunStatus::Interrupted, RunStatus::Failed}) {
        QTemporaryDir temporary;
        QString error;
        const QString root = QDir(temporary.path()).filePath(
            status == RunStatus::Interrupted ? "interrupted" : "failed");
        auto writer = RunWriterV2::start(root, data(), &error);
        require(writer.has_value(), qPrintable(error));
        RunEvent value = event();
        value.eventId = status == RunStatus::Interrupted ? "interrupted" : "failed";
        value.cropPath = QString("crops/%1.png").arg(value.eventId);
        require(writer->appendEvent(value, "crop", &error), qPrintable(error));
        require(writer->finalize(status, "2026-07-24T12:00:01Z",
                                 status == RunStatus::Interrupted
                                     ? "camera_disconnected"
                                     : "writer_error",
                                 10.0, &error),
                qPrintable(error));
        require(QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
                    QFileInfo::exists(QDir(root).filePath("run_summary.partial.json")) &&
                    QFileInfo::exists(QDir(root).filePath("events.csv")),
                "incomplete Run preserves partial and readable canonical log");
        auto loaded = RunManifestV2::load(QDir(root).filePath("run_summary.json"),
                                          &error);
        require(loaded.has_value() && loaded->data().status == status,
                qPrintable(error));
    }
}

void testActivePartialSurvivesDestruction() {
    stage = "active";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("active");
    {
        auto writer = RunWriterV2::start(root, data(), &error);
        require(writer.has_value(), qPrintable(error));
        require(writer->appendEvent(event(), "crop", &error), qPrintable(error));
    }
    require(QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
                QFileInfo::exists(QDir(root).filePath("run_summary.partial.json")) &&
                !QFileInfo::exists(QDir(root).filePath("run_summary.json")),
            "unfinalized writer leaves recoverable partial state");
    auto recovered = RunManifestV2::load(
        QDir(root).filePath("run_summary.partial.json"), &error);
    require(recovered.has_value() && recovered->data().events.size() == 1,
            qPrintable(error));
}

void testCsvRollbackAndEarlyFailure() {
    stage = "rollback";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("rollback");
    auto writer = RunWriterV2::start(root, data(), &error);
    require(writer.has_value(), qPrintable(error));
    RunWriterV2TestAccess::failNextAppend(*writer);
    require(!writer->appendEvent(event(), "crop", &error),
            "injected CSV failure");
    require(!QFileInfo::exists(QDir(root).filePath(event().cropPath)),
            "failed append removes crop");
    auto recovered = RunManifestV2::load(
        QDir(root).filePath("run_summary.partial.json"), &error);
    require(recovered.has_value() && recovered->data().events.isEmpty(),
            qPrintable(error));
    require(writer->appendEvent(event(), "crop", &error),
            "writer remains usable after rollback");
    require(writer->finalize(RunStatus::Failed, "2026-07-24T12:00:01Z",
                             "early_failure", 0.0, &error),
            qPrintable(error));
    auto failed = RunManifestV2::load(QDir(root).filePath("run_summary.json"),
                                      &error);
    require(failed.has_value() && failed->data().achievedProcessingFps == 0.0,
            qPrintable(error));
}

void testLinkedCropDirectoryRejectedWhenSupported() {
    stage = "linked";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("linked");
    auto writer = RunWriterV2::start(root, data(), &error);
    require(writer.has_value(), qPrintable(error));
    const QString crops = QDir(root).filePath("crops");
    const QString outside = QDir(temporary.path()).filePath("outside");
    QDir().mkpath(outside);
    require(QDir().rmdir(crops), "remove empty crops directory");
    if (QFile::link(outside, crops) && QFileInfo(crops).isSymLink())
        require(!writer->appendEvent(event(), "crop", &error),
                "linked crops directory rejected");
}

void testStoppedCleanup() {
    stage = "stopped";
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("stopped");
    auto writer = RunWriterV2::start(root, data(), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendEvent(event(), "crop", &error), qPrintable(error));
    require(writer->finalize(RunStatus::Stopped, "2026-07-24T12:00:01Z",
                             "user", 10.0, &error),
            qPrintable(error));
    require(!QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
                !QFileInfo::exists(
                    QDir(root).filePath("run_summary.partial.json")),
            "Stopped Run removes clean partials");
    auto loaded = RunManifestV2::load(
        QDir(root).filePath("run_summary.json"), &error);
    require(loaded.has_value() && loaded->data().status == RunStatus::Stopped,
            qPrintable(error));
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testCompletedAndEscaping();
    testInterruptedAndFailedRecovery();
    testActivePartialSurvivesDestruction();
    testCsvRollbackAndEarlyFailure();
    testLinkedCropDirectoryRejectedWhenSupported();
    testStoppedCleanup();
    return 0;
}
