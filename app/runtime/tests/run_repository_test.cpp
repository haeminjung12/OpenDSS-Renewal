#include "../v2/results/run_repository.h"
#include "../v2/run/run_writer_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace desktop_app::v2;
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

RunManifestData data(const QString& id, const QString& startedAt) {
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
    value.files.sequencePath = QStringLiteral("sequence");
    return value;
}

RunEvent event(const QString& id, const QString& startedAt) {
    RunEvent value;
    value.eventId = id + "-event";
    value.detectionTimestamp = startedAt;
    value.sourceFrameIndex = 1;
    value.cropPath = "crops/event.bin";
    value.decision = Route::Hit;
    value.observedRoute = Route::Unresolved;
    value.daqPulseStatus = DaqPulseStatus::SuppressedNotIssued;
    return value;
}

void createSavedSequence(const QString& folder);

QString createRun(const QString& folder, const QString& id,
                  const QString& startedAt,
                  desktop_app::v2::run::RunStatus status =
                      desktop_app::v2::run::RunStatus::Completed) {
    QString error;
    auto writer = RunWriterV2::start(folder, data(id, startedAt), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendEvent(event(id, startedAt), "crop-content", &error),
            qPrintable(error));
    require(writer->finalize(status, startedAt, "duration",
                             status == desktop_app::v2::run::RunStatus::Completed
                                 ? 20.0
                                 : 0.0,
                             &error),
            qPrintable(error));
    createSavedSequence(folder);
    return QDir(folder).filePath("run_summary.json");
}

QString createPartialRun(const QString& folder, const QString& id,
                         const QString& startedAt) {
    QString error;
    auto writer = RunWriterV2::start(folder, data(id, startedAt), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendEvent(event(id, startedAt), "crop-content", &error),
            qPrintable(error));
    require(writer->flush(&error), qPrintable(error));
    createSavedSequence(folder);
    return QDir(folder).filePath("run_summary.partial.json");
}

QByteArray bytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read file bytes");
    return file.readAll();
}

void createSavedSequence(const QString& folder) {
    const QString sequenceFolder = QDir(folder).filePath("sequence");
    require(QDir().mkpath(sequenceFolder), "create saved Sequence folder");
    sequence::SequenceManifestData manifest{
        "sequence-001",
        "Saved sequence",
        "assay",
        {},
        "completed",
        "2026-07-24T10:00:00Z",
        "2026-07-24T10:00:00Z",
        "2026-07-24T10:00:01Z",
        1.0,
        "duration",
        "2.0",
        1,
        QJsonObject{{"exposure_us", 100}},
        100,
        100,
        16,
        1.0,
    };
    QString error;
    require(sequence::SequenceManifestV2::save(
                QDir(sequenceFolder).filePath("sequence.json"), manifest, &error),
            qPrintable(error));
}

RunEntry entry(const RunRepository& repository, const QString& id) {
    for (const RunEntry& candidate : repository.entries()) {
        if (candidate.id == id)
            return candidate;
    }
    require(false, "expected Run entry");
    return repository.entries().first();
}

void testDiscoveryAndRecoverableLoad() {
    stage = "discovery";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary root");
    const QString runs = QDir(temporary.path()).filePath("Runs");
    createRun(QDir(runs).filePath("completed"), "completed",
              "2026-07-24T10:00:00Z");
    createRun(QDir(runs).filePath("interrupted"), "interrupted",
              "2026-07-24T11:00:00Z",
              desktop_app::v2::run::RunStatus::Interrupted);
    createRun(QDir(runs).filePath("failed"), "failed",
              "2026-07-24T12:00:00Z",
              desktop_app::v2::run::RunStatus::Failed);
    createPartialRun(QDir(runs).filePath("partial"), "partial",
                     "2026-07-24T13:00:00Z");

    const QString invalidFolder = QDir(runs).filePath("invalid");
    require(QDir().mkpath(invalidFolder), "invalid folder");
    QFile invalid(QDir(invalidFolder).filePath("run_summary.json"));
    require(invalid.open(QIODevice::WriteOnly), "invalid summary");
    require(invalid.write("{not-json") > 0, "write invalid summary");
    invalid.close();

    const QString emptyFolder = QDir(runs).filePath("empty");
    require(QDir().mkpath(emptyFolder), "empty folder");
    createRun(QDir(runs).filePath("container/nested"), "nested",
              "2026-07-24T14:00:00Z");

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    require(repository.entries().size() == 7,
            "all immediate folders retained and nested Run ignored");
    int partialIndex = -1;
    int failedIndex = -1;
    int interruptedIndex = -1;
    int completedIndex = -1;
    bool sawNested = false;
    for (qsizetype index = 0; index < repository.entries().size(); ++index) {
        const QString id = repository.entries().at(index).id;
        if (id == "partial")
            partialIndex = static_cast<int>(index);
        else if (id == "failed")
            failedIndex = static_cast<int>(index);
        else if (id == "interrupted")
            interruptedIndex = static_cast<int>(index);
        else if (id == "completed")
            completedIndex = static_cast<int>(index);
        else if (id == "nested")
            sawNested = true;
    }
    require(partialIndex < failedIndex && failedIndex < interruptedIndex &&
                interruptedIndex < completedIndex && !sawNested,
            "valid Runs are newest-first and nested Run is not discovered");
    require(entry(repository, "partial").recoverable &&
                entry(repository, "partial").loadable &&
                QFileInfo(entry(repository, "partial").eventsPath).fileName() ==
                    "events.partial.csv" &&
                QFileInfo(entry(repository, "partial").sequencePath.value())
                        .fileName() == "sequence.json",
            "partial-only Run resolves events and saved Sequence manifests");
    require(!entry(repository, "failed").recoverable &&
                QFileInfo(entry(repository, "failed").summaryPath).fileName() ==
                    "run_summary.json" &&
                QFileInfo(entry(repository, "failed").eventsPath).fileName() ==
                    "events.csv",
            "finalized summary is preferred when final and partial coexist");

    bool sawInvalid = false;
    bool sawEmpty = false;
    for (const RunEntry& candidate : repository.entries()) {
        if (candidate.runName == "invalid") {
            sawInvalid = !candidate.loadable && !candidate.reason.isEmpty();
        }
        if (candidate.runName == "empty") {
            sawEmpty = !candidate.loadable &&
                       candidate.reason.contains("missing");
        }
    }
    require(sawInvalid && sawEmpty, "invalid folders retained with direct reasons");

    const QString completedSummary =
        entry(repository, "completed").summaryPath;
    QJsonObject escaped = QJsonDocument::fromJson(bytes(completedSummary)).object();
    QJsonObject escapedFiles = escaped.value("files").toObject();
    escapedFiles.insert("events_csv", "../events.csv");
    escaped.insert("files", escapedFiles);
    QFile escapedFile(completedSummary);
    require(escapedFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open traversal summary");
    require(escapedFile.write(QJsonDocument(escaped).toJson()) > 0,
            "write traversal summary");
    escapedFile.close();
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    bool traversalRejected = false;
    for (const RunEntry& candidate : repository.entries()) {
        if (candidate.runName == "completed") {
            traversalRejected =
                !candidate.loadable && !candidate.reason.isEmpty();
        }
    }
    require(traversalRejected, "parent traversal remains rejected");

    require(repository.selectRun("partial"), "select partial");
    require(repository.loadSelected(&error), qPrintable(error));
    require(repository.loadedRun() &&
                repository.loadedRun()->manifest.data().events.size() == 1 &&
                stateStore.snapshot().results.loadedRunId == "partial",
            "recoverable selected Run loads");
    require(!repository.updateLoadedNotes("must fail", &error) &&
                error.contains("finalized"),
            "recoverable partial Run denies Notes");
}

void testLiveStoppedDiscovery() {
    stage = "live discovery";
    QTemporaryDir temporary;
    const QString runs = QDir(temporary.path()).filePath("Runs");
    RunManifestData live = data("live", "2026-07-24T10:00:00Z");
    live.operation = RunOperation::LiveSorting;
    live.sourceSequence = {};
    live.files.sequencePath.reset();
    live.routing.physicalDaqOutputEnabled = true;
    live.requestedProcessingFps = 0.0;
    QString error;
    const QString folder = QDir(runs).filePath("live");
    auto writer = RunWriterV2::start(folder, live, &error);
    require(writer.has_value(), qPrintable(error));
    RunEvent value = event("live", "2026-07-24T10:00:00Z");
    value.daqPulseStatus = DaqPulseStatus::Issued;
    require(writer->appendEvent(value, "crop-content", &error),
            qPrintable(error));
    require(writer->finalize(desktop_app::v2::run::RunStatus::Stopped,
                             "2026-07-24T10:00:01Z",
                             "user", 0.0, &error),
            qPrintable(error));

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    require(repository.entries().size() == 1 &&
                repository.entries().first().loadable &&
                repository.entries().first().operation ==
                    RunOperation::LiveSorting &&
                repository.entries().first().status ==
                    desktop_app::v2::run::RunStatus::Stopped,
            "Live Stopped Run discovered");
    require(repository.selectRun("live") && repository.loadSelected(&error) &&
                repository.loadedRun()->manifest.data().operation ==
                    RunOperation::LiveSorting,
            qPrintable(error));
}

void testConcurrentNotesUpdatesSerializeOrDeny() {
    stage = "concurrent notes";
    QTemporaryDir temporary;
    const QString runs = QDir(temporary.path()).filePath("Runs");
    createRun(QDir(runs).filePath("run"), "concurrent",
              "2026-07-24T10:00:00Z");
    ApplicationStateStore stateStore;
    RunRepository first(stateStore);
    RunRepository second(stateStore);
    QString setupError;
    require(first.refresh(temporary.path(), &setupError) &&
                second.refresh(temporary.path(), &setupError) &&
                first.selectRun("concurrent") && second.selectRun("concurrent") &&
                first.loadSelected(&setupError) && second.loadSelected(&setupError),
            qPrintable(setupError));

    std::atomic_bool start = false;
    bool firstResult = false;
    bool secondResult = false;
    QString firstError;
    QString secondError;
    std::thread firstThread([&] {
        while (!start.load())
            std::this_thread::yield();
        firstResult = first.updateLoadedNotes("first", &firstError);
    });
    std::thread secondThread([&] {
        while (!start.load())
            std::this_thread::yield();
        secondResult = second.updateLoadedNotes("second", &secondError);
    });
    start = true;
    firstThread.join();
    secondThread.join();
    require(firstResult || secondResult, "one concurrent Notes update succeeds");
    require(firstResult || firstError.contains("locked"),
            "first update succeeds or reports contention");
    require(secondResult || secondError.contains("locked"),
            "second update succeeds or reports contention");

    RunRepository verifier(stateStore);
    require(verifier.refresh(temporary.path(), &setupError) &&
                verifier.selectRun("concurrent") &&
                verifier.loadSelected(&setupError),
            qPrintable(setupError));
    const QString notes = verifier.loadedRun()->manifest.data().notes;
    require((firstResult && notes == "first") ||
                (secondResult && notes == "second"),
            "persisted Notes came from a successful serialized update");
}

void testSelectionAndFailedLoadPreservePriorDetail() {
    stage = "selection";
    QTemporaryDir temporary;
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString aSummary =
        createRun(QDir(runs).filePath("a"), "a", "2026-07-24T10:00:00Z");
    createRun(QDir(runs).filePath("b"), "b", "2026-07-24T11:00:00Z");

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    require(repository.selectRun("b") && repository.loadSelected(&error),
            qPrintable(error));
    require(repository.selectRun("a"), "select A");
    require(repository.loadedRun()->entry.id == "b" &&
                stateStore.snapshot().results.selectedRunId == "a" &&
                stateStore.snapshot().results.loadedRunId == "b",
            "select A leaves loaded B");

    require(QFile::remove(QDir(QFileInfo(aSummary).absolutePath())
                              .filePath("events.csv")),
            "remove selected events");
    require(!repository.loadSelected(&error), "stale selected load fails");
    require(repository.loadedRun()->entry.id == "b" &&
                stateStore.snapshot().results.loadedRunId == "b" &&
                !stateStore.snapshot().results.loadError.isEmpty(),
            "failed load preserves B and publishes direct reason");

    const QString aFolder = QFileInfo(aSummary).absolutePath();
    const QString vanished = QDir(temporary.path()).filePath("vanished-a");
    require(QDir().rename(aFolder, vanished), "move selected Run out of discovery");
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    require(repository.selectedRunId().isEmpty() &&
                repository.loadedRun()->entry.id == "b" &&
                stateStore.snapshot().results.selectedRunId.isEmpty() &&
                stateStore.snapshot().results.loadedRunId == "b",
            "refresh clears vanished selection and preserves loaded detail");

    require(!repository.loadSelected(&error) &&
                error == "No Run is selected." &&
                stateStore.snapshot().results.loadError ==
                    "No Run is selected." &&
                repository.loadedRun()->entry.id == "b",
            "no-selection load publishes error and preserves loaded detail");

    const QVector<RunEntry> beforeFailedRefresh = repository.entries();
    require(!repository.refresh(QDir(temporary.path()).filePath("missing-root"),
                                &error),
            "invalid refresh fails");
    require(repository.entries().size() == beforeFailedRefresh.size() &&
                repository.entries().first().id ==
                    beforeFailedRefresh.first().id &&
                repository.selectedRunId().isEmpty() &&
                repository.loadedRun()->entry.id == "b",
            "failed refresh preserves prior repository state");
}

void testMissingOptionalSequenceStillLoads() {
    stage = "optional sequence";
    QTemporaryDir temporary;
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString missingSummary =
        createRun(QDir(runs).filePath("missing"), "missing",
                  "2026-07-24T10:00:00Z");
    const QString missingFolder = QFileInfo(missingSummary).absolutePath();
    require(QFile::remove(
                QDir(missingFolder).filePath("sequence/sequence.json")) &&
                QDir(missingFolder).rmdir("sequence"),
            "remove optional saved Sequence");

    const QString wrongSummary =
        createRun(QDir(runs).filePath("wrong-shape"), "wrong-shape",
                  "2026-07-24T11:00:00Z");
    const QString wrongFolder = QFileInfo(wrongSummary).absolutePath();
    require(QFile::remove(
                QDir(wrongFolder).filePath("sequence/sequence.json")) &&
                QDir(wrongFolder).rmdir("sequence"),
            "remove valid saved Sequence");
    QFile wrongShape(QDir(wrongFolder).filePath("sequence"));
    require(wrongShape.open(QIODevice::WriteOnly) &&
                wrongShape.write("not-a-directory") > 0,
            "write arbitrary file in place of saved Sequence directory");
    wrongShape.close();

    const QString invalidSummary =
        createRun(QDir(runs).filePath("invalid-manifest"), "invalid-manifest",
                  "2026-07-24T12:00:00Z");
    QFile invalidManifest(
        QDir(QFileInfo(invalidSummary).absolutePath())
            .filePath("sequence/sequence.json"));
    require(invalidManifest.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                invalidManifest.write("{}") == 2,
            "write invalid saved Sequence manifest");
    invalidManifest.close();

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error), qPrintable(error));
    const RunEntry missing = entry(repository, "missing");
    const RunEntry wrong = entry(repository, "wrong-shape");
    const RunEntry invalid = entry(repository, "invalid-manifest");
    require(missing.loadable && !missing.sequencePath &&
                missing.sequenceReason ==
                    "No saved Image Sequence for this Run",
            "missing optional sequence has direct nonfatal reason");
    require(wrong.loadable && !wrong.sequencePath &&
                wrong.sequenceReason ==
                    "No saved Image Sequence for this Run",
            "arbitrary file is not accepted as a saved Sequence");
    require(invalid.loadable && !invalid.sequencePath &&
                invalid.sequenceReason ==
                    "No saved Image Sequence for this Run",
            "invalid sequence.json is nonfatal and unavailable");
    require(repository.selectRun("missing") && repository.loadSelected(&error),
            qPrintable(error));
}

void testNotesGuardAndNotesOnlyReload() {
    stage = "notes";
    QTemporaryDir temporary;
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString summary =
        createRun(QDir(runs).filePath("run"), "notes",
                  "2026-07-24T10:00:00Z");
    const QString runFolder = QFileInfo(summary).absolutePath();
    const QString events = QDir(runFolder).filePath("events.csv");
    const QString crop = QDir(runFolder).filePath("crops/event.bin");
    const QString source =
        QDir(runFolder).filePath("sequence/sequence.json");
    const QByteArray eventsBefore = bytes(events);
    const QByteArray cropBefore = bytes(crop);
    const QByteArray sourceBefore = bytes(source);
    QJsonObject before = QJsonDocument::fromJson(bytes(summary)).object();

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error) &&
                repository.selectRun("notes") && repository.loadSelected(&error),
            qPrintable(error));
    stateStore.publishRun(
        {"notes", runFolder, desktop_app::v2::RunStatus::Open, {}});
    require(!repository.updateLoadedNotes("blocked", &error) &&
                error.contains("active"),
            "active matching Run blocks Notes");
    stateStore.publishRun(
        {"other", runFolder, desktop_app::v2::RunStatus::Open, {}});
    require(!repository.updateLoadedNotes("path blocked", &error) &&
                error.contains("active"),
            "active matching Run path blocks Notes");
    stateStore.publishRun(
        {"other", runFolder + "-other", desktop_app::v2::RunStatus::Open, {}});

    require(repository.updateLoadedNotes("updated notes", &error),
            qPrintable(error));
    require(repository.loadedRun()->manifest.data().notes == "updated notes",
            "loaded detail reloaded after Notes update");
    QJsonObject after = QJsonDocument::fromJson(bytes(summary)).object();
    before.remove("notes");
    after.remove("notes");
    require(before == after, "only Notes field changed");
    require(bytes(events) == eventsBefore && bytes(crop) == cropBefore &&
                bytes(source) == sourceBefore,
            "event, crop, and sequence bytes preserved");

    QLockFile held(summary + ".lock");
    held.setStaleLockTime(0);
    require(held.tryLock(0), "hold Notes lock");
    require(!repository.updateLoadedNotes("denied", &error) &&
                error.contains("locked"),
            "Notes contention denied");
    require(repository.loadedRun()->manifest.data().notes == "updated notes",
            "contention preserves loaded detail");
}

#ifdef Q_OS_WIN
bool createJunction(const QString& junction, const QString& target) {
    return QProcess::execute(
               "cmd.exe",
               {"/c", "mklink", "/J", QDir::toNativeSeparators(junction),
                QDir::toNativeSeparators(target)}) == 0;
}

bool removeJunction(const QString& junction) {
    return QProcess::execute(
               "cmd.exe", {"/c", "rmdir", QDir::toNativeSeparators(junction)}) == 0;
}
#endif

void testLinkedRunIsRetainedButNotTraversed() {
    stage = "links";
    QTemporaryDir temporary;
    const QString outside = QDir(temporary.path()).filePath("outside");
    createRun(outside, "outside", "2026-07-24T10:00:00Z");
    const QString root = QDir(temporary.path()).filePath("root");
    const QString runs = QDir(root).filePath("Runs");
    require(QDir().mkpath(runs), "create Runs");
    const QString link = QDir(runs).filePath("linked");
    std::error_code ec;
    std::filesystem::create_directory_symlink(
        std::filesystem::path(outside.toStdWString()),
        std::filesystem::path(link.toStdWString()), ec);
    if (ec)
        return;

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(root, &error), qPrintable(error));
    require(repository.entries().size() == 1 &&
                !repository.entries().first().loadable &&
                repository.entries().first().reason.contains("linked"),
            "linked Run retained as non-loadable without traversal");
    std::filesystem::remove(std::filesystem::path(link.toStdWString()), ec);
    require(!ec, "remove linked Run");
}

void testJunctionRunIsRetainedButNotTraversed() {
#ifdef Q_OS_WIN
    stage = "junctions";
    QTemporaryDir temporary;
    const QString outside = QDir(temporary.path()).filePath("outside");
    createRun(outside, "outside", "2026-07-24T10:00:00Z");
    const QString root = QDir(temporary.path()).filePath("root");
    const QString runs = QDir(root).filePath("Runs");
    require(QDir().mkpath(runs), "create Runs");
    const QString junction = QDir(runs).filePath("junction");
    if (!createJunction(junction, outside))
        return;

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(root, &error), qPrintable(error));
    require(repository.entries().size() == 1 &&
                !repository.entries().first().loadable,
            "junction retained but not traversed");
    require(removeJunction(junction), "remove junction");
#endif
}

void testAggregatedRootsAreDeduplicated() {
    stage = "aggregated roots";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary aggregate root");
    const QString liveRoot = QDir(temporary.path()).filePath("live");
    const QString sequenceRoot = QDir(temporary.path()).filePath("sequence");
    createRun(QDir(liveRoot).filePath("live-run"), "live-run",
              "2026-07-24T10:00:00Z");
    createRun(QDir(sequenceRoot).filePath("sequence-run"), "sequence-run",
              "2026-07-24T11:00:00Z");

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refreshRoots(
                {liveRoot, sequenceRoot, QDir(liveRoot).absolutePath()}, &error),
            qPrintable(error));
    require(repository.entries().size() == 2,
            "distinct effective roots aggregate and identical roots scan once");
}

void testRemovalUsesRecycleBinAndPreservesFailures() {
    stage = "remove Run";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary removal root");
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString protectedFolder = QDir(runs).filePath("protected");
    const QString removableFolder = QDir(runs).filePath("removable");
    createRun(protectedFolder, "protected", "2026-07-24T10:00:00Z");
    createRun(removableFolder, "removable", "2026-07-24T11:00:00Z");

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error) &&
                repository.selectRun("protected") &&
                repository.loadSelected(&error),
            qPrintable(error));
    stateStore.publishRun(
        {"protected", protectedFolder, desktop_app::v2::RunStatus::Open, {}});
    require(!repository.removeSelected(&error) && error.contains("active") &&
                QFileInfo::exists(protectedFolder) &&
                repository.loadedRun()->entry.id == "protected",
            "active removal failure leaves the complete Run and loaded state");

    stateStore.publishRun(
        {"protected", protectedFolder, desktop_app::v2::RunStatus::Finalized, {}});
    require(repository.selectRun("removable") &&
                repository.loadSelected(&error) &&
                repository.removeSelected(&error),
            qPrintable(error));
    require(!QFileInfo::exists(removableFolder) &&
                QFileInfo::exists(protectedFolder),
            "confirmed repository action trashes only the complete selected folder");
    require(repository.entries().size() == 1 &&
                repository.entries().first().id == "protected" &&
                repository.selectedRunId().isEmpty() &&
                !repository.loadedRun(),
            "successful trash refreshes entries and clears removed selection/detail");
}

void testReplacementBetweenRefreshAndRemoveIsPreserved() {
    stage = "replacement before remove";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary replacement root");
    const QString runs = QDir(temporary.path()).filePath("Runs");
    const QString selectedFolder = QDir(runs).filePath("selected");
    const QString movedOriginal = QDir(temporary.path()).filePath("moved-original");
    createRun(selectedFolder, "original", "2026-07-24T10:00:00Z");

    ApplicationStateStore stateStore;
    RunRepository repository(stateStore);
    QString error;
    require(repository.refresh(temporary.path(), &error) &&
                repository.selectRun("original"),
            qPrintable(error));
    require(QDir().rename(selectedFolder, movedOriginal),
            "move selected Run after refresh");
    createRun(selectedFolder, "replacement", "2026-07-24T11:00:00Z");

    require(!repository.removeSelected(&error) &&
                error.contains("changed") &&
                QFileInfo::exists(selectedFolder) &&
                QFileInfo::exists(movedOriginal),
            "identity mismatch preserves replacement and original Run folders");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testDiscoveryAndRecoverableLoad();
    testLiveStoppedDiscovery();
    testSelectionAndFailedLoadPreservePriorDetail();
    testMissingOptionalSequenceStillLoads();
    testNotesGuardAndNotesOnlyReload();
    testConcurrentNotesUpdatesSerializeOrDeny();
    testLinkedRunIsRetainedButNotTraversed();
    testJunctionRunIsRetainedButNotTraversed();
    testAggregatedRootsAreDeduplicated();
    testRemovalUsesRecycleBinAndPreservesFailures();
    testReplacementBetweenRefreshAndRemoveIsPreserved();
    return 0;
}
