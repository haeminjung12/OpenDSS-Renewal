#include "../v2/results/run_repository.h"
#include "../v2/results/runs_results_controller.h"
#include "../v2/run/run_writer_v2.h"
#include "../v2/sequence/sequence_manifest_v2.h"
#include "../v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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

void createSavedSequence(const QString &runFolder)
{
    const QString sequenceFolder = QDir(runFolder).filePath(QStringLiteral("sequence"));
    require(QDir().mkpath(sequenceFolder), "create saved Sequence folder");
    sequence::SequenceManifestData sequenceData;
    sequenceData.sequenceId = QStringLiteral("sequence-001");
    sequenceData.name = QStringLiteral("Saved sequence");
    sequenceData.experimentType = QStringLiteral("assay");
    sequenceData.status = QStringLiteral("completed");
    sequenceData.createdAt = QStringLiteral("2026-07-24T10:00:00Z");
    sequenceData.startedAt = sequenceData.createdAt;
    sequenceData.endedAt = QStringLiteral("2026-07-24T10:00:01Z");
    sequenceData.requestedDurationSeconds = 1.0;
    sequenceData.stopReason = QStringLiteral("duration");
    sequenceData.opendssVersion = QStringLiteral("2.0");
    sequenceData.frameCount = 1;
    sequenceData.cameraSettings = {{QStringLiteral("exposure_us"), 100}};
    sequenceData.imageWidth = 100;
    sequenceData.imageHeight = 100;
    sequenceData.bitDepth = 16;
    sequenceData.nominalFps = 1.0;
    QString error;
    const bool sequenceSaved = sequence::SequenceManifestV2::save(
        QDir(sequenceFolder).filePath(QStringLiteral("sequence.json")),
        sequenceData, &error);
    require(sequenceSaved,
            qPrintable(error.isEmpty() ? QStringLiteral("save saved Sequence fixture")
                                       : error));
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
    data.model = ModelSnapshot{
        QStringLiteral("model-001"), QStringLiteral("DropletNet"),
        QString(64, QLatin1Char('a')),
        {{QStringLiteral("class-0"), QStringLiteral("Class Zero")},
         {QStringLiteral("class-1"), QStringLiteral("Class One")}}};
    data.routing.triggerMode = TriggerMode::ClassBased;
    data.routing.hitClassId = QStringLiteral("class-1");
    data.routing.physicalDaqOutputEnabled = true;
    data.cameraSettings = {{QStringLiteral("exposure_us"), 100}};
    data.detectorSettings = {{QStringLiteral("threshold"), 4}};
    data.cropSettings = {{QStringLiteral("size"), 64}};
    data.daqSettings = {{QStringLiteral("channel"), QStringLiteral("Dev1/ao0")}};
    data.timingSettings = {{QStringLiteral("delay_us"), 10}};
    data.hitBoundary = {20.0, HitSide::PositiveY, 100, 100};
    data.requestedProcessingFps = 20.0;
    data.files.sequencePath = QStringLiteral("sequence/sequence.json");

    QString error;
    auto writer = RunWriterV2::start(folder, data, &error);
    require(writer.has_value(),
            qPrintable(error.isEmpty() ? QStringLiteral("start Run fixture") : error));
    RunEvent event;
    event.eventId = QStringLiteral("event-001");
    event.detectionTimestamp = data.startedAt;
    event.sourceFrameIndex = 1;
    event.cropPath = QStringLiteral("crops/event.bin");
    event.decision = Route::Hit;
    event.observedRoute = Route::Unresolved;
    event.daqPulseStatus = DaqPulseStatus::SuppressedNotIssued;
    event.predictedClassId = QStringLiteral("class-1");
    event.scores = {0.1, 0.9};
    event.inferenceTimeMs = 1.0;
    const bool eventAppended = writer->appendEvent(event, "crop-content", &error);
    require(eventAppended,
            qPrintable(error.isEmpty() ? QStringLiteral("append Run event fixture")
                                       : error));
    RunEvent rejected;
    rejected.eventId = QStringLiteral("event-002");
    rejected.detectionTimestamp = data.startedAt;
    rejected.sourceFrameIndex = 2;
    rejected.rejected = 1;
    require(writer->appendEvent(rejected, {}, &error),
            qPrintable(error.isEmpty() ? QStringLiteral("append rejected fixture")
                                       : error));
    const bool runFinalized = writer->finalize(
        desktop_app::v2::run::RunStatus::Completed, data.endedAt,
        data.stopReason, 20.0, &error);
    require(runFinalized,
            qPrintable(error.isEmpty() ? QStringLiteral("finalize Run fixture")
                                       : error));
    createSavedSequence(folder);
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
    const QString runFolder =
        QDir(temporary.path()).filePath(QStringLiteral("Runs/run-001"));
    createRun(runFolder);

    ApplicationStateStore store;
    store.publishPreferences({temporary.path(), 100});
    RunRepository repository(store);
    QVector<QUrl> openedUrls;
    RunsResultsController controller(
        repository, store,
        [&openedUrls](const QUrl &url) {
            openedUrls.append(url);
            return true;
        });

    require(controller.refresh(),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("refresh Run repository")
                           : controller.errorMessage()));
    const QVariantList runs = controller.runs();
    require(runs.size() == 1, "one Run projection");
    const QVariantMap entry = runs.first().toMap();
    require(entry.value(QStringLiteral("id")).toString() == QStringLiteral("run-001") &&
                entry.value(QStringLiteral("runName")).toString() == QStringLiteral("Run 001") &&
                entry.value(QStringLiteral("loadable")).toBool() &&
                entry.value(QStringLiteral("totalCount")).toLongLong() == 1 &&
                entry.value(QStringLiteral("rejectedCount")).toLongLong() == 1 &&
                entry.value(QStringLiteral("summaryText")).toString().contains(
                    QStringLiteral("Rejected: 1")),
            "Run projection is factual");

    require(controller.selectRun(QStringLiteral("run-001")), "select discovered Run");
    require(controller.selectedRunId() == QStringLiteral("run-001"),
            "selected id comes from state");
    require(controller.loadSelected(),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("load selected Run")
                           : controller.errorMessage()));
    require(controller.loadedRun().value(QStringLiteral("notes")).toString() ==
                QStringLiteral("original notes"),
            "loaded Run exposes Notes");
    const QVariantMap facts = controller.loadedRun();
    require(facts.value(QStringLiteral("experimentType")).toString() ==
                QStringLiteral("assay") &&
                facts.value(QStringLiteral("modelId")).toString() ==
                    QStringLiteral("model-001") &&
                facts.value(QStringLiteral("modelChecksum")).toString() ==
                    QString(64, QLatin1Char('a')) &&
                facts.value(QStringLiteral("hitClass")).toString().contains(
                    QStringLiteral("class-1")) &&
                facts.value(QStringLiteral("physicalDaqOutput")).toString() ==
                    QStringLiteral("On") &&
                facts.value(QStringLiteral("hitBoundary")).toString() ==
                    QStringLiteral("Sequence Test — Bottom is Hit") &&
                !facts.contains(QStringLiteral("hitBoundaryX")) &&
                !facts.contains(QStringLiteral("hitBoundaryY")) &&
                facts.value(QStringLiteral("cameraSettings")).toString().contains(
                    QStringLiteral("exposure_us")) &&
                facts.value(QStringLiteral("decisionHit")).toLongLong() == 1 &&
                facts.value(QStringLiteral("rejectedCount")).toLongLong() == 1 &&
                facts.value(QStringLiteral("predictedCounts")).toString().contains(
                    QStringLiteral("Rejected: 1")) &&
                facts.value(QStringLiteral("hitDecisionUnresolved")).toLongLong() == 1,
            "loaded Run detail projection is factual");

    QString requestedSequence;
    QObject::connect(&controller, &RunsResultsController::savedSequenceRequested,
                     &controller, [&requestedSequence](const QString &path) {
        requestedSequence = path;
    });
    require(controller.openDropletLog() && controller.openRunFolder(),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("open Run log and folder")
                           : controller.errorMessage()));
    require(controller.openDropletCrop(
                QUrl::fromLocalFile(facts.value(QStringLiteral("cropsPath")).toString() +
                                    QStringLiteral("/event.bin"))),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("open Run crop")
                           : controller.errorMessage()));
    require(openedUrls.size() == 3 &&
                QFileInfo(openedUrls.at(0).toLocalFile()).fileName() ==
                    QStringLiteral("events.csv") &&
                QFileInfo(openedUrls.at(1).toLocalFile()).isDir() &&
                QFileInfo(openedUrls.at(2).toLocalFile()).fileName() ==
                    QStringLiteral("event.bin"),
            "artifact actions open exact loaded paths");
    require(controller.openSavedSequence() &&
                QFileInfo(requestedSequence).fileName() == QStringLiteral("sequence.json"),
            "saved Sequence action requests the exact manifest");

    const QString externalFile =
        QDir(temporary.path()).filePath(QStringLiteral("outside.bin"));
    QFile external(externalFile);
    require(external.open(QIODevice::WriteOnly) && external.write("outside") > 0,
            "external crop fixture");
    external.close();
    require(!controller.openDropletCrop(QUrl::fromLocalFile(externalFile)) &&
                controller.errorMessage() ==
                    QStringLiteral("Selected file is not a Droplet Crop from this Run.") &&
                openedUrls.size() == 3,
            "crop action rejects files outside the loaded Run");

    require(controller.updateLoadedNotes(QStringLiteral("updated notes")),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("update loaded Run Notes")
                           : controller.errorMessage()));
    require(controller.loadedRun().value(QStringLiteral("notes")).toString() ==
                QStringLiteral("updated notes"),
            "Notes update reloads authoritative detail");
    require(controller.removeSelected(),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("remove selected Run")
                           : controller.errorMessage()));
    require(!QFileInfo::exists(runFolder) && controller.runs().isEmpty() &&
                controller.selectedRunId().isEmpty() &&
                controller.loadedRun().isEmpty(),
            "controller refreshes truthful state after Recycle Bin removal");
}

void testLegacyRunWithoutBoundaryProvenance()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary legacy boundary root");
    const QString runFolder = QDir(temporary.path()).filePath(QStringLiteral("Run"));
    createRun(runFolder);

    const QString summaryPath = QDir(runFolder).filePath(QStringLiteral("run_summary.json"));
    QFile summary(summaryPath);
    require(summary.open(QIODevice::ReadOnly), "read legacy Run summary");
    QJsonObject document = QJsonDocument::fromJson(summary.readAll()).object();
    summary.close();
    document.remove(QStringLiteral("hit_boundary"));
    require(summary.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "rewrite legacy Run summary");
    require(summary.write(QJsonDocument(document).toJson()) > 0,
            "write legacy Run summary");
    summary.close();

    ApplicationStateStore store;
    store.publishPreferences({temporary.path(), 100});
    RunRepository repository(store);
    RunsResultsController controller(repository, store);
    require(controller.refresh() && controller.selectRun(QStringLiteral("run-001"))
                && controller.loadSelected(),
            "load legacy Run without boundary provenance");
    require(controller.loadedRun().value(QStringLiteral("hitBoundary")).toString()
                == QStringLiteral("Not recorded"),
            "legacy Run does not invent boundary provenance");
}

void testEffectiveRootAggregationAndFallback()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary aggregate controller root");
    const QString liveRoot = QDir(temporary.path()).filePath(QStringLiteral("live"));
    const QString sequenceRoot =
        QDir(temporary.path()).filePath(QStringLiteral("sequence"));
    const QString fallbackRoot =
        QDir(temporary.path()).filePath(QStringLiteral("fallback"));
    createRun(QDir(liveRoot).filePath(QStringLiteral("live-run")));
    createRun(QDir(sequenceRoot).filePath(QStringLiteral("sequence-run")));
    createRun(QDir(fallbackRoot).filePath(QStringLiteral("fallback-run")));

    ApplicationStateStore store;
    RunRepository repository(store);
    RunsResultsController controller(
        repository, store, {}, [fallbackRoot] { return fallbackRoot; });
    require(controller.refreshRoots(liveRoot, QUrl::fromLocalFile(sequenceRoot)),
            qPrintable(controller.errorMessage()));
    require(controller.runs().size() == 2,
            "current Live and Sequence Test roots are aggregated");

    const QString missing =
        QDir(temporary.path()).filePath(QStringLiteral("missing"));
    require(controller.refreshRoots(missing, QUrl::fromLocalFile(missing)),
            qPrintable(controller.errorMessage()));
    require(controller.runs().size() == 1,
            "invalid roots use the standard fallback and identical roots scan once");
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
    require(controller.refresh(),
            qPrintable(controller.errorMessage().isEmpty()
                           ? QStringLiteral("refresh invalid Run entries")
                           : controller.errorMessage()));

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
                    !entry.contains(QStringLiteral("rejectedCount")) &&
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
                "runsResultsController: window.runsResultsController"),
            "root forwards runsResultsController to ShellSingleImage instance");

    QFile form(QStringLiteral(OPENDSS_TEST_DESKTOP_V2_CONTENT_DIR
                              "/RunsWorkspace.ui.qml"));
    require(form.open(QIODevice::ReadOnly), "open RunsWorkspace form source");
    const QByteArray formSource = form.readAll();
    for (const QByteArray alias :
         {"openDropletLogButton", "openRunFolderButton", "openDropletCropButton",
          "openSavedSequenceButton"}) {
        require(formSource.contains(QByteArray("property alias ") + alias + ": " + alias),
                "Run file action is exported from the QDS form");
    }
    require(formSource.contains("property var loadedRunFacts") &&
                !formSource.contains("Illustrative Camera A") &&
                !formSource.contains("Predicted Class 0: 714"),
            "Run form consumes factual loaded detail without illustrative values");

    QFile settingsForm(QStringLiteral(OPENDSS_TEST_DESKTOP_V2_CONTENT_DIR
                                      "/SettingsWorkspace.ui.qml"));
    require(settingsForm.open(QIODevice::ReadOnly), "open SettingsWorkspace form source");
    const QByteArray settingsSource = settingsForm.readAll();
    require(settingsSource.contains(
                "property alias openDiagnosticFolderButton: openDiagnosticFolderButton") &&
                settingsSource.contains("root.runtimeAvailability") &&
                settingsSource.contains("root.cameraDriverAvailability") &&
                settingsSource.contains("root.daqDriverAvailability") &&
                settingsSource.contains("root.gpuEnvironmentAvailability") &&
                !settingsSource.contains("Runtime Availability: Available") &&
                !settingsSource.contains("Camera Driver Availability: Available"),
            "Settings form exports diagnostics action and contains no fabricated availability");

    QFile labelForm(QStringLiteral(OPENDSS_TEST_DESKTOP_V2_CONTENT_DIR
                                   "/LabelWorkspace.ui.qml"));
    require(labelForm.open(QIODevice::ReadOnly), "open LabelWorkspace form source");
    const QByteArray labelSource = labelForm.readAll();
    require(!labelSource.contains("id: pageSpinBox") &&
                !labelSource.contains("id: imagesPerPageSelector") &&
                labelSource.contains("id: cropGridScroll"),
            "Label form removes unsupported pagination while preserving grid scrolling");
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
    require(!controller.openDropletLog() &&
                controller.errorMessage() == QStringLiteral("No Run is loaded.") &&
                !controller.openRunFolder() &&
                !controller.openSavedSequence(),
            "file actions reject the no-loaded-Run state");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().contains(QStringLiteral("--runs-removal"))) {
        testRefreshSelectLoadAndNotes();
        testEffectiveRootAggregationAndFallback();
        return 0;
    }
    testRefreshSelectLoadAndNotes();
    testLegacyRunWithoutBoundaryProvenance();
    testEffectiveRootAggregationAndFallback();
    testInvalidEntriesExposeNoFabricatedFacts();
    testErrorsArePublished();
    testRootControllerBootstrapContract();
    return 0;
}
