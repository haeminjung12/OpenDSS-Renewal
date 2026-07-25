#include "../v2/results/run_repository.h"
#include "../v2/results/runs_results_controller.h"
#include "../v2/run/run_writer_v2.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace desktop_app::v2;
using namespace desktop_app::v2::results;
using namespace desktop_app::v2::run;

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void createRun(const QString &folder)
{
    RunManifestData data;
    data.runId = QStringLiteral("run-001");
    data.runName = QStringLiteral("Run 001");
    data.experimentType = QStringLiteral("assay");
    data.notes = QStringLiteral("original notes");
    data.startedAt = QStringLiteral("2026-07-24T10:00:00Z");
    data.endedAt = data.startedAt;
    data.stopReason = QStringLiteral("duration");
    data.opendssVersion = QStringLiteral("2.0");
    data.sourceSequence = {QStringLiteral("sequence"), QStringLiteral("Sequence"),
                           QStringLiteral("source.json")};
    data.routing.triggerMode = TriggerMode::EveryDroplet;
    data.cameraSettings = {{QStringLiteral("exposure_us"), 100}};
    data.detectorSettings = {{QStringLiteral("threshold"), 4}};
    data.cropSettings = {{QStringLiteral("size"), 64}};
    data.daqSettings = {{QStringLiteral("channel"), QStringLiteral("Dev1/ao0")}};
    data.timingSettings = {{QStringLiteral("delay_us"), 10}};
    data.hitBoundary = {20.0, HitSide::PositiveY, 100, 100};
    data.requestedProcessingFps = 20.0;

    QString error;
    auto writer = RunWriterV2::start(folder, data, &error);
    require(writer.has_value(), qPrintable(error));
    RunEvent event;
    event.eventId = QStringLiteral("event-001");
    event.detectionTimestamp = data.startedAt;
    event.sourceFrameIndex = 1;
    event.cropPath = QStringLiteral("crops/event.bin");
    event.decision = Route::Hit;
    event.observedRoute = Route::Unresolved;
    event.daqPulseStatus = DaqPulseStatus::SuppressedNotIssued;
    require(writer->appendEvent(event, "crop-content", &error), qPrintable(error));
    require(writer->finalize(desktop_app::v2::run::RunStatus::Completed,
                             data.endedAt, data.stopReason,
                             20.0, &error), qPrintable(error));
}

QVariantMap runByName(const QVariantList &runs, const QString &name)
{
    for (const QVariant &run : runs) {
        const QVariantMap candidate = run.toMap();
        if (candidate.value(QStringLiteral("runName")).toString() == name)
            return candidate;
    }
    return {};
}

void testRefreshSelectLoadAndNotes()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary root");
    createRun(QDir(temporary.path()).filePath(QStringLiteral("Runs/run-001")));

    ApplicationStateStore store;
    store.publishPreferences({temporary.path(), 100});
    RunRepository repository(store);
    RunsResultsController controller(repository, store);

    require(controller.refresh(), qPrintable(controller.errorMessage()));
    const QVariantList runs = controller.runs();
    require(runs.size() == 1, "one Run projection");
    const QVariantMap entry = runs.first().toMap();
    require(entry.value(QStringLiteral("id")).toString() == QStringLiteral("run-001") &&
                entry.value(QStringLiteral("runName")).toString() == QStringLiteral("Run 001") &&
                entry.value(QStringLiteral("loadable")).toBool(),
            "Run projection is factual");

    require(controller.selectRun(QStringLiteral("run-001")), "select discovered Run");
    require(controller.selectedRunId() == QStringLiteral("run-001"),
            "selected id comes from state");
    require(controller.loadSelected(), qPrintable(controller.errorMessage()));
    require(controller.loadedRun().value(QStringLiteral("notes")).toString() ==
                QStringLiteral("original notes"),
            "loaded Run exposes Notes");

    require(controller.updateLoadedNotes(QStringLiteral("updated notes")),
            qPrintable(controller.errorMessage()));
    require(controller.loadedRun().value(QStringLiteral("notes")).toString() ==
                QStringLiteral("updated notes"),
            "Notes update reloads authoritative detail");
}

void testInvalidEntriesExposeNoFabricatedFacts()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary invalid-entry root");
    const QString runsRoot = QDir(temporary.path()).filePath(QStringLiteral("Runs"));
    require(QDir().mkpath(QDir(runsRoot).filePath(QStringLiteral("missing"))),
            "missing-summary folder");
    const QString malformedFolder = QDir(runsRoot).filePath(QStringLiteral("malformed"));
    require(QDir().mkpath(malformedFolder), "malformed-summary folder");
    QFile malformed(QDir(malformedFolder).filePath(QStringLiteral("run_summary.json")));
    require(malformed.open(QIODevice::WriteOnly) && malformed.write("{}") == 2,
            "malformed summary fixture");
    malformed.close();

    ApplicationStateStore store;
    store.publishPreferences({temporary.path(), 100});
    RunRepository repository(store);
    RunsResultsController controller(repository, store);
    require(controller.refresh(), qPrintable(controller.errorMessage()));

    const QVariantMap missing = runByName(controller.runs(), QStringLiteral("missing"));
    const QVariantMap malformedEntry =
        runByName(controller.runs(), QStringLiteral("malformed"));
    for (const QVariantMap &entry : {missing, malformedEntry}) {
        require(!entry.isEmpty() && !entry.value(QStringLiteral("loadable")).toBool(),
                "invalid Run retained as non-loadable");
        require(!entry.value(QStringLiteral("reason")).toString().isEmpty(),
                "invalid Run preserves repository reason");
        require(!entry.contains(QStringLiteral("operation")) &&
                    !entry.contains(QStringLiteral("status")) &&
                    !entry.contains(QStringLiteral("startedAt")) &&
                    !entry.contains(QStringLiteral("durationSeconds")) &&
                    !entry.contains(QStringLiteral("totalCount")) &&
                    !entry.contains(QStringLiteral("modelName")) &&
                    !entry.contains(QStringLiteral("recoverable")),
                "invalid Run exposes no fabricated optional facts");
    }
    require(missing.value(QStringLiteral("reason")).toString() ==
                QStringLiteral("Run Summary is missing."),
            "missing summary reason remains direct");
    require(malformedEntry.value(QStringLiteral("reason")).toString() ==
                QStringLiteral("Unsupported Run schema_version."),
            "malformed summary reason remains direct");
}

void testRootControllerBootstrapContract()
{
    QFile appQml(QStringLiteral(OPENDSS_TEST_DESKTOP_V2_CONTENT_DIR "/App.qml"));
    require(appQml.open(QIODevice::ReadOnly), "open App.qml bootstrap source");
    const QByteArray source = appQml.readAll();
    require(source.contains("property var runsResultsController: null"),
            "root accepts runsResultsController initial property");
    require(source.contains(
                "property var runsResultsController: window.runsResultsController"),
            "root forwards runsResultsController to ShellSingleImage instance");
}

void testErrorsArePublished()
{
    ApplicationStateStore store;
    store.publishPreferences({QStringLiteral("Z:/missing-opendss-root"), 100});
    RunRepository repository(store);
    RunsResultsController controller(repository, store);

    require(!controller.refresh() && !controller.errorMessage().isEmpty(),
            "refresh publishes factual error");
    require(!controller.selectRun(QStringLiteral("missing")) &&
                controller.errorMessage() == QStringLiteral("Selected Run is unavailable."),
            "invalid selection publishes factual error");
    require(!controller.loadSelected() &&
                controller.errorMessage() == QStringLiteral("No Run is selected."),
            "load failure comes from repository");
    require(!controller.updateLoadedNotes(QStringLiteral("notes")) &&
                controller.errorMessage() == QStringLiteral("No Run is loaded."),
            "Notes failure is published");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    testRefreshSelectLoadAndNotes();
    testInvalidEntriesExposeNoFabricatedFacts();
    testErrorsArePublished();
    testRootControllerBootstrapContract();
    return 0;
}
