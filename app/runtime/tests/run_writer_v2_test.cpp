#include "../v2/run/run_manifest_v2.h"
#include "../v2/run/run_writer_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace desktop_app::v2::run;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
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
    return value;
}

void testCompletedAndEscaping() {
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("completed");
    auto writer = RunWriterV2::start(root, data(), &error);
    require(writer.has_value(), qPrintable(error));
    const QByteArray cropBytes("preserved-source-bytes\0tail", 27);
    require(writer->appendEvent(event(), cropBytes, &error), qPrintable(error));
    require(writer->flush(&error), qPrintable(error));
    require(QFileInfo::exists(QDir(root).filePath("events.partial.csv")),
            "partial exists while active");
    require(writer->finalize(RunStatus::Completed, "2026-07-24T12:00:01Z",
                             "user", 10.0, &error),
            qPrintable(error));
    require(!QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
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
                    QFileInfo::exists(QDir(root).filePath("events.csv")),
                "incomplete Run preserves partial and readable canonical log");
        auto loaded = RunManifestV2::load(QDir(root).filePath("run_summary.json"),
                                          &error);
        require(loaded.has_value() && loaded->data().status == status,
                qPrintable(error));
    }
}

void testActivePartialSurvivesDestruction() {
    QTemporaryDir temporary;
    QString error;
    const QString root = QDir(temporary.path()).filePath("active");
    {
        auto writer = RunWriterV2::start(root, data(), &error);
        require(writer.has_value(), qPrintable(error));
        require(writer->appendEvent(event(), "crop", &error), qPrintable(error));
    }
    require(QFileInfo::exists(QDir(root).filePath("events.partial.csv")) &&
                !QFileInfo::exists(QDir(root).filePath("run_summary.json")),
            "unfinalized writer leaves recoverable partial state");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testCompletedAndEscaping();
    testInterruptedAndFailedRecovery();
    testActivePartialSurvivesDestruction();
    return 0;
}
