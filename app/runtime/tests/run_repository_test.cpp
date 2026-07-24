#include "../v2/results/run_repository.h"
#include "../v2/run/run_writer_v2.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace desktop_app::v2::results;
using namespace desktop_app::v2::run;

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

RunManifestData data(QString id, QString startedAt) {
    RunManifestData value;
    value.runId = id;
    value.runName = QString("Run %1").arg(id);
    value.experimentType = "assay";
    value.notes = "original";
    value.startedAt = startedAt;
    value.endedAt = startedAt;
    value.stopReason = "duration";
    value.opendssVersion = "2.0";
    value.sourceSequence = {"sequence", "Sequence", "source.json"};
    value.routing.triggerMode = TriggerMode::EveryDroplet;
    value.cameraSettings = {{"exposure_us", 100}};
    value.detectorSettings = {{"threshold", 4}};
    value.cropSettings = {{"size", 64}};
    value.daqSettings = {{"channel", "Dev1/ao0"}};
    value.timingSettings = {{"delay_us", 10}};
    value.hitBoundary = {20.0, HitSide::PositiveY, 100, 100};
    value.requestedProcessingFps = 20.0;
    return value;
}

QString createRun(const QString& folder, const QString& id,
                  const QString& startedAt) {
    QString error;
    stage = "create writer";
    auto writer = RunWriterV2::start(folder, data(id, startedAt), &error);
    if (!writer)
        std::cerr << qPrintable(error) << '\n';
    require(writer.has_value(), "writer start");
    RunEvent event;
    event.eventId = id + "-event";
    event.detectionTimestamp = startedAt;
    event.sourceFrameIndex = 1;
    event.cropPath = "crops/event.bin";
    event.decision = Route::Hit;
    event.observedRoute = Route::Unresolved;
    event.daqPulseStatus = DaqPulseStatus::SuppressedNotIssued;
    stage = "append event";
    if (!writer->appendEvent(event, "crop-content", &error)) {
        std::cerr << qPrintable(error) << '\n';
        require(false, "append event");
    }
    stage = "finalize";
    require(writer->finalize(RunStatus::Completed, startedAt, "duration", 20.0,
                             &error),
            "finalize");
    return QDir(folder).filePath("run_summary.json");
}

QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read file bytes");
    return file.readAll();
}

QByteArray hash(const QString& path) {
    return QCryptographicHash::hash(bytes(path), QCryptographicHash::Sha256);
}

void testDiscoveryLoadAndDiagnostics() {
    stage = "discovery setup";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary root");
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString older =
        createRun(QDir(runs).filePath("older"), "older", "2026-07-24T10:00:00Z");
    const QString newer =
        createRun(QDir(runs).filePath("newer"), "newer", "2026-07-24T12:00:00Z");

    const QString invalidFolder = QDir(runs).filePath("invalid");
    require(QDir().mkpath(invalidFolder), "invalid folder");
    QFile invalid(QDir(invalidFolder).filePath("run_summary.json"));
    require(invalid.open(QIODevice::WriteOnly), "invalid summary");
    require(invalid.write("{not-json") > 0, "write invalid summary");
    invalid.close();

    const QString liveFolder = QDir(runs).filePath("live");
    const QString live = createRun(liveFolder, "live", "2026-07-24T11:00:00Z");
    QJsonObject liveObject = QJsonDocument::fromJson(bytes(live)).object();
    liveObject.insert("operation", "live_sorting");
    QFile liveFile(live);
    require(liveFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open live summary");
    require(liveFile.write(QJsonDocument(liveObject).toJson()) > 0,
            "write live summary");
    liveFile.close();

    const QString nested =
        QDir(runs).filePath("container/nested");
    createRun(nested, "nested", "2026-07-24T13:00:00Z");

    QString error;
    stage = "discover";
    const auto summaries = RunRepository::discover(temporary.path(), &error);
    require(summaries.size() == 2, "only immediate valid Sequence Test Runs");
    require(summaries.at(0).runId == "newer" &&
                summaries.at(1).runId == "older",
            "newest started_at first");
    require(error.contains("invalid") && error.contains("live_sorting"),
            "invalid and unsupported Live diagnostics");
    require(QFileInfo(summaries.at(0).eventsPath).isFile() &&
                QFileInfo(summaries.at(0).cropsPath).isDir(),
            "open paths exist before return");

    stage = "load";
    auto loaded = RunRepository::load(newer, &error);
    require(loaded && loaded->data().runId == "newer" &&
                loaded->data().events.size() == 1,
            qPrintable(error));
    require(!RunRepository::load(
                QDir(QFileInfo(newer).absolutePath()).filePath("../newer/run_summary.json"),
                &error),
            "parent traversal rejected");

    const auto explicitRun =
        RunRepository::discover(QFileInfo(older).absolutePath(), &error);
    require(explicitRun.size() == 1 && explicitRun.first().runId == "older",
            "explicit Run root");
}

void testNotesOnlyAndNonReplacement() {
    stage = "notes";
    QTemporaryDir temporary;
    const QString summary =
        createRun(QDir(temporary.path()).filePath("run"), "notes",
                  "2026-07-24T10:00:00Z");
    const QString runFolder = QFileInfo(summary).absolutePath();
    const QString events = QDir(runFolder).filePath("events.csv");
    const QString crop = QDir(runFolder).filePath("crops/event.bin");
    const QByteArray eventsBefore = hash(events);
    const QByteArray cropBefore = hash(crop);
    QJsonObject before = QJsonDocument::fromJson(bytes(summary)).object();

    QString error;
    require(RunRepository::updateNotes(summary, "updated notes", &error),
            qPrintable(error));
    auto loaded = RunRepository::load(summary, &error);
    require(loaded && loaded->data().notes == "updated notes", qPrintable(error));
    QJsonObject after = QJsonDocument::fromJson(bytes(summary)).object();
    before.remove("notes");
    after.remove("notes");
    require(before == after, "only notes field changed");
    require(hash(events) == eventsBefore && hash(crop) == cropBefore,
            "events and crops unchanged");

    const QByteArray validSummary = bytes(summary);
    require(QFile::remove(events), "remove events");
    require(!RunRepository::updateNotes(summary, "must fail", &error),
            "invalid Run update rejected");
    require(bytes(summary) == validSummary,
            "failed update does not replace summary");
}

void testLinkedRunIsNotTraversed() {
    stage = "links";
    QTemporaryDir temporary;
    const QString outside = QDir(temporary.path()).filePath("outside");
    createRun(outside, "outside", "2026-07-24T10:00:00Z");
    const QString runs = QDir(temporary.path()).filePath("root/Runs");
    require(QDir().mkpath(runs), "create Runs");
    const QString link = QDir(runs).filePath("linked");
    std::error_code ec;
    std::filesystem::create_directory_symlink(
        std::filesystem::path(outside.toStdWString()),
        std::filesystem::path(link.toStdWString()), ec);
    if (ec)
        return;

    QString error;
    const auto discovered =
        RunRepository::discover(QDir(temporary.path()).filePath("root"), &error);
    require(discovered.isEmpty(), "linked Run not discovered");
    require(error.contains("linked Run directory"), "linked Run diagnostic");
    require(!RunRepository::load(QDir(link).filePath("run_summary.json"), &error),
            "summary through linked Run rejected");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testDiscoveryLoadAndDiagnostics();
    testNotesOnlyAndNonReplacement();
    testLinkedRunIsNotTraversed();
    return 0;
}
