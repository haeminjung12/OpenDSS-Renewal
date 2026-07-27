#include "../v2/run/run_manifest_v2.h"
#include "../v2/run/run_writer_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

using namespace desktop_app::v2::run;

namespace {

const char* stage = "";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL " << stage << ": " << message << '\n';
        std::exit(1);
    }
}

RunManifestData baseData() {
    RunManifestData data;
    data.runId = "run-1";
    data.runName = "Sequence test";
    data.operation = RunOperation::SequenceTest;
    data.experimentType = "assay";
    data.notes = "notes";
    data.startedAt = "2026-07-24T10:00:00Z";
    data.endedAt = "2026-07-24T10:00:02Z";
    data.stopReason = "duration";
    data.opendssVersion = "2.0";
    data.sourceSequence = {"sequence-1", "Source sequence", "source/sequence.json"};
    data.routing.triggerMode = TriggerMode::EveryDroplet;
    data.routing.physicalDaqOutputEnabled = false;
    data.cameraSettings = {{"exposure_us", 100}};
    data.requestedProcessingFps = 20.0;
    data.achievedProcessingFps = 19.5;
    data.detectorSettings = {{"threshold", 1}};
    data.cropSettings = {{"size", 64}};
    data.daqSettings = {{"channel", "Dev1/ao0"}};
    data.timingSettings = {{"delay_us", 10}};
    data.hitBoundary = {180.0, HitSide::NegativeY, 1200, 360};
    return data;
}

RunEvent event(QString id, qint64 frame, Route observed) {
    RunEvent value;
    value.eventId = std::move(id);
    value.detectionTimestamp = "2026-07-24T10:00:01Z";
    value.sourceFrameIndex = frame;
    value.cropPath = QString("crops/%1.png").arg(frame);
    value.decision = Route::Hit;
    value.observedRoute = observed;
    value.daqPulseStatus = observed == Route::Waste
                               ? DaqPulseStatus::NotRequested
                               : DaqPulseStatus::SuppressedNotIssued;
    return value;
}

QString createNoModelRun(const QString& root) {
    stage = "create no-model start";
    QString error;
    auto writer = RunWriterV2::start(root, baseData(), &error);
    require(writer.has_value(), qPrintable(error));
    stage = "create no-model append 1";
    bool ok = writer->appendEvent(event("event-1", 1, Route::Hit), "crop1", &error);
    require(ok, qPrintable(error));
    stage = "create no-model append 2";
    ok = writer->appendEvent(event("event-2", 2, Route::Waste), "crop2", &error);
    require(ok, qPrintable(error));
    stage = "create no-model append 3";
    ok = writer->appendEvent(event("event-3", 3, Route::Unresolved), "crop3", &error);
    require(ok, qPrintable(error));
    stage = "create no-model finalize";
    require(writer->finalize(RunStatus::Completed, "2026-07-24T10:00:02Z",
                             "duration", 19.5, &error),
            qPrintable(error));
    return QDir(root).filePath("run_summary.json");
}

void testNoModelRoundTripAndStrictness() {
    stage = "no-model";
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory");
    const QString runRoot = QDir(temporary.path()).filePath("run");
    const QString summary = createNoModelRun(runRoot);
    QString error;
    auto loaded = RunManifestV2::load(summary, &error);
    require(loaded.has_value(), qPrintable(error));
    require(!loaded->data().model && loaded->data().events.size() == 3,
            "no-model events round-trip");
    require(loaded->data().cameraSettings.value("exposure_us").toInt() == 100,
            "camera snapshot round-trips");
    const auto& counts = loaded->derivedCounts();
    require(counts.total == 3 && counts.unclassified == 3 &&
                counts.decisionHit == 3 && counts.observedHit == 1 &&
                counts.observedWaste == 1 && counts.observedUnresolved == 1 &&
                counts.hitDecisionUnresolved == 1,
            "derived Decision-vs-Observed counts");

    QFile existing(summary);
    require(existing.open(QIODevice::ReadOnly), "read summary");
    const QByteArray before = existing.readAll();
    existing.close();
    RunManifestData invalid = loaded->data();
    invalid.requestedProcessingFps = 0.0;
    require(!RunManifestV2::save(summary, invalid, &error),
            "invalid save must fail");
    require(existing.open(QIODevice::ReadOnly) && existing.readAll() == before,
            "invalid save must not replace valid summary");
    existing.close();
    QJsonObject object = QJsonDocument::fromJson(before).object();
    require(!object.contains(QStringLiteral("hit_boundary")),
            "Run summary must not persist Decision Boundary coordinates.");
    QJsonObject staleCounts = object.value("counts").toObject();
    staleCounts.insert("total", 0);
    object.insert("counts", staleCounts);
    QFile corrupt(summary);
    require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate), "open stale canonical");
    corrupt.write(QJsonDocument(object).toJson());
    corrupt.close();
    require(!RunManifestV2::load(summary, &error),
            "canonical summary rejects stale derived counts");

    object = QJsonDocument::fromJson(before).object();
    object.insert("unexpected", true);
    require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate), "open corrupt");
    corrupt.write(QJsonDocument(object).toJson());
    corrupt.close();
    require(!RunManifestV2::load(summary, &error), "unknown root field rejected");
}

void testTwoAndThreeClassHistory() {
    stage = "modeled";
    for (int classCount : {2, 3}) {
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory");
        RunManifestData data = baseData();
        data.runId = QString("model-%1").arg(classCount);
        ModelSnapshot model{"model-id", "Historical Model",
                            QString(64, QLatin1Char('a')), {}};
        model.classes = {{"c0", "Historical Zero"}, {"c1", "Historical One"}};
        if (classCount == 3)
            model.classes.push_back({"c2", "Historical Two"});
        data.model = model;
        data.routing.triggerMode = TriggerMode::ClassBased;
        data.routing.hitClassId = "c0";

        QString error;
        const QString root = QDir(temporary.path()).filePath("run");
        auto writer = RunWriterV2::start(root, data, &error);
        require(writer.has_value(), qPrintable(error));
        for (int i = 0; i < classCount; ++i) {
            RunEvent value = event(QString("modeled-%1").arg(i), i + 1,
                                   i == 0 ? Route::Unresolved : Route::Waste);
            value.cropPath = QString("crops/modeled-%1.png").arg(i);
            value.predictedClassId = model.classes.at(i).id;
            value.scores.fill(0.1, classCount);
            value.scores[i] = 0.8;
            value.decision = i == 0 ? Route::Hit : Route::Waste;
            value.daqPulseStatus = i == 0 ? DaqPulseStatus::SuppressedNotIssued
                                         : DaqPulseStatus::NotRequested;
            value.inferenceTimeMs = 1.25 + i;
            require(writer->appendEvent(value, QByteArray("crop") + QByteArray::number(i),
                                        &error),
                    qPrintable(error));
        }
        require(writer->finalize(RunStatus::Completed, "2026-07-24T10:00:02Z",
                                 "duration", 19.5, &error),
                qPrintable(error));
        auto loaded = RunManifestV2::load(QDir(root).filePath("run_summary.json"),
                                          &error);
        require(loaded.has_value(), qPrintable(error));
        require(loaded->data().model->name == "Historical Model" &&
                    loaded->data().model->classes.at(0).name == "Historical Zero" &&
                    loaded->derivedCounts().predictedByClass.size() == classCount,
                "historical class names and counts");
        for (qint64 count : loaded->derivedCounts().predictedByClass)
            require(count == 1, "per-class derived count");
    }
}

void testValidationEdges() {
    stage = "edges";
    QTemporaryDir temporary;
    QString error;
    auto writer = RunWriterV2::start(QDir(temporary.path()).filePath("run"),
                                     baseData(), &error);
    require(writer.has_value(), qPrintable(error));
    RunEvent traversal = event("bad-path", 1, Route::Hit);
    traversal.cropPath = "../outside.png";
    require(!writer->appendEvent(traversal, "crop", &error),
            "path traversal rejected");
    RunEvent issued = event("issued", 2, Route::Hit);
    issued.daqPulseStatus = DaqPulseStatus::Issued;
    require(!writer->appendEvent(issued, "crop", &error),
            "DAQ-disabled issued pulse rejected");

    RunManifestData modeled = baseData();
    modeled.model = ModelSnapshot{"m", "m", QString(64, 'b'),
                                  {{"0", "zero"}, {"1", "one"}}};
    modeled.routing.triggerMode = TriggerMode::ClassBased;
    modeled.routing.hitClassId = "0";
    auto modeledWriter = RunWriterV2::start(
        QDir(temporary.path()).filePath("modeled"), modeled, &error);
    require(modeledWriter.has_value(), qPrintable(error));
    RunEvent badScores = event("scores", 1, Route::Hit);
    badScores.predictedClassId = "0";
    badScores.scores = {0.5, 0.3, 0.2};
    badScores.inferenceTimeMs = 1.0;
    require(!modeledWriter->appendEvent(badScores, "crop", &error),
            "wrong score count rejected");

    RunEvent wrongArgmax = event("argmax", 2, Route::Hit);
    wrongArgmax.predictedClassId = "0";
    wrongArgmax.scores = {0.1, 0.9};
    wrongArgmax.inferenceTimeMs = 1.0;
    require(!modeledWriter->appendEvent(wrongArgmax, "crop", &error),
            "prediction must match argmax");

    RunEvent tie = event("tie", 3, Route::Hit);
    tie.predictedClassId = "0";
    tie.scores = {0.5, 0.5};
    tie.inferenceTimeMs = 1.0;
    require(modeledWriter->appendEvent(tie, "tie-crop", &error),
            "first ordered class wins score ties");

    RunEvent requested = event("requested", 4, Route::Hit);
    requested.daqPulseStatus = DaqPulseStatus::Requested;
    require(!writer->appendEvent(requested, "crop", &error),
            "requested is not a finalized DAQ status");

    RunManifestData physical = baseData();
    physical.routing.physicalDaqOutputEnabled = true;
    physical.model = ModelSnapshot{"physical-model", "Physical model", QString(64, 'c'),
                                   {{"0", "Hit class"}, {"1", "Waste class"}}};
    physical.routing.triggerMode = TriggerMode::ClassBased;
    physical.routing.hitClassId = "0";
    auto physicalWriter = RunWriterV2::start(
        QDir(temporary.path()).filePath("physical"), physical, &error);
    require(physicalWriter.has_value(), qPrintable(error));
    RunEvent physicalHit = event("physical-hit", 1, Route::Hit);
    physicalHit.decision = Route::Waste;
    physicalHit.predictedClassId = "1";
    physicalHit.scores = {0.1, 0.9};
    physicalHit.inferenceTimeMs = 1.0;
    physicalHit.daqPulseStatus = DaqPulseStatus::Issued;
    require(physicalWriter->appendEvent(physicalHit, "crop", &error),
            "final Hit accepts issued independently of Decision");
    RunEvent physicalWaste = event("physical-waste", 2, Route::Waste);
    physicalWaste.predictedClassId = "0";
    physicalWaste.scores = {0.9, 0.1};
    physicalWaste.inferenceTimeMs = 1.0;
    physicalWaste.daqPulseStatus = DaqPulseStatus::NotRequested;
    require(physicalWriter->appendEvent(physicalWaste, "crop", &error),
            "final Waste remains not_requested independently of Decision");
    require(physicalWriter->finalize(RunStatus::Completed,
                                     "2026-07-24T10:00:02Z",
                                     "duration", 19.5, &error),
            qPrintable(error));
    auto physicalRoundTrip = RunManifestV2::load(
        QDir(temporary.path()).filePath("physical/run_summary.json"), &error);
    require(physicalRoundTrip.has_value() &&
                physicalRoundTrip->data().events.size() == 2 &&
                physicalRoundTrip->data().events.at(0).decision == Route::Waste &&
                physicalRoundTrip->data().events.at(0).observedRoute == Route::Hit &&
                physicalRoundTrip->data().events.at(0).daqPulseStatus ==
                    DaqPulseStatus::Issued &&
                physicalRoundTrip->data().events.at(1).decision == Route::Hit &&
                physicalRoundTrip->data().events.at(1).observedRoute == Route::Waste &&
                physicalRoundTrip->data().events.at(1).daqPulseStatus ==
                    DaqPulseStatus::NotRequested,
            "Decision/route mismatches round-trip with route-keyed DAQ status");
}

void testLiveStoppedIntegrityRoundTrip() {
    stage = "live";
    QTemporaryDir temporary;
    RunManifestData data = baseData();
    data.runId = "live";
    data.runName = "Live";
    data.operation = RunOperation::LiveSorting;
    data.sourceSequence = {};
    data.routing.physicalDaqOutputEnabled = true;
    data.requestedProcessingFps = 0.0;
    data.achievedProcessingFps = 0.0;
    QString error;
    const QString root = QDir(temporary.path()).filePath("live");
    auto writer = RunWriterV2::start(root, data, &error);
    require(writer.has_value(), qPrintable(error));
    RunEvent value = event("live-event", 1, Route::Unresolved);
    require(writer->appendEvent(value, "crop", &error), qPrintable(error));
    require(writer->finalize(RunStatus::Stopped, "2026-07-24T10:00:02Z",
                             "user", 0.0, &error),
            qPrintable(error));
    const QString summary = QDir(root).filePath("run_summary.json");
    auto loaded = RunManifestV2::load(summary, &error);
    require(loaded.has_value(), qPrintable(error));
    require(loaded->data().operation == RunOperation::LiveSorting &&
                loaded->data().status == RunStatus::Stopped &&
                loaded->data().sourceSequence.id.isEmpty() &&
                loaded->data().requestedProcessingFps == 0.0 &&
                loaded->data().integrity.sourceFrameGaps.count == 0 &&
                loaded->data().integrity.queueRejections.count == 0 &&
                loaded->data().integrity.consumerFailures.count == 0,
            "Live/Stopped round-trip");
    QFile file(summary);
    require(file.open(QIODevice::ReadOnly), "read Live summary");
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    require(!object.contains("source_sequence") && !object.contains("processing") &&
                object.value("integrity").isObject(),
            "Live omits Sequence Test-only fields");

    QJsonObject legacy = object;
    legacy.remove("integrity");
    QFile legacyFile(summary);
    require(legacyFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open old Run fixture");
    require(legacyFile.write(QJsonDocument(legacy).toJson()) > 0,
            "write old Run fixture");
    legacyFile.close();
    loaded = RunManifestV2::load(summary, &error);
    require(loaded.has_value() &&
                loaded->data().integrity.sourceFrameGaps.count == 0 &&
                loaded->data().integrity.queueRejections.count == 0 &&
                loaded->data().integrity.consumerFailures.count == 0,
            "missing integrity loads as empty");

    QJsonObject invalidStopped = legacy;
    invalidStopped.insert(
        "integrity",
        QJsonObject{
            {"source_frame_gaps",
             QJsonObject{{"count", 0}, {"ranges", QJsonArray{}}}},
            {"queue_rejections",
             QJsonObject{
                 {"count", 1},
                 {"ranges",
                  QJsonArray{
                      QJsonObject{{"first", 8}, {"last", 8}}}}}},
            {"consumer_failures",
             QJsonObject{{"count", 0}, {"ranges", QJsonArray{}}}},
        });
    require(legacyFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "open invalid stopped fixture");
    require(legacyFile.write(QJsonDocument(invalidStopped).toJson()) > 0,
            "write invalid stopped fixture");
    legacyFile.close();
    require(!RunManifestV2::load(summary, &error),
            "imported Stopped manifest rejects event loss");

    const QString lossRoot = QDir(temporary.path()).filePath("loss");
    data.runId = "loss";
    data.runName = "Loss";
    auto lossWriter = RunWriterV2::start(lossRoot, data, &error);
    require(lossWriter.has_value(), qPrintable(error));
    const RunIntegrity sourceLoss{{1, {{4, 4}}}, {}, {}};
    require(lossWriter->checkpoint(sourceLoss, &error), qPrintable(error));
    require(!lossWriter->finalize(RunStatus::Completed,
                                  "2026-07-24T10:00:02Z",
                                  "duration", 0.0, &error),
            "Completed rejects source loss");
    require(lossWriter->finalize(RunStatus::Interrupted,
                                 "2026-07-24T10:00:02Z",
                                 "source_frame_gap", 0.0, &error),
            qPrintable(error));
}

void testLivePhysicalOutputRoundTrip() {
    stage = "Live physical output round-trip";
    QTemporaryDir temporary;
    QString error;
    for (const bool enabled : {false, true}) {
        RunManifestData data = baseData();
        data.runId = enabled ? QStringLiteral("live-on")
                             : QStringLiteral("live-off");
        data.runName = enabled ? QStringLiteral("Live On")
                               : QStringLiteral("Live Off");
        data.operation = RunOperation::LiveSorting;
        data.sourceSequence = {};
        data.routing.physicalDaqOutputEnabled = enabled;
        data.requestedProcessingFps = 0.0;
        data.achievedProcessingFps = 0.0;
        const QString root = QDir(temporary.path()).filePath(
            enabled ? QStringLiteral("on") : QStringLiteral("off"));
        auto writer = RunWriterV2::start(root, data, &error);
        require(writer.has_value(), qPrintable(error));
        require(writer->finalize(RunStatus::Completed,
                                 QStringLiteral("2026-07-24T10:00:02Z"),
                                 QStringLiteral("duration"), 0.0, &error),
                qPrintable(error));
        const QString path =
            QDir(root).filePath(QStringLiteral("run_summary.json"));
        auto loaded = RunManifestV2::load(path, &error);
        require(loaded.has_value()
                    && loaded->data().routing.physicalDaqOutputEnabled
                        == enabled,
                "Live manifest preserves the selected physical-output state");
    }
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testNoModelRoundTripAndStrictness();
    testTwoAndThreeClassHistory();
    testValidationEdges();
    testLiveStoppedIntegrityRoundTrip();
    testLivePhysicalOutputRoundTrip();
    return 0;
}
