#include "../v2/run/run_manifest_v2.h"
#include "../v2/run/run_writer_v2.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
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
    data.requestedProcessingFps = 20.0;
    data.achievedProcessingFps = 19.5;
    data.detectorSettings = {{"threshold", 1}};
    data.cropSettings = {{"size", 64}};
    data.daqSettings = {{"channel", "Dev1/ao0"}};
    data.timingSettings = {{"delay_us", 10}};
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
    return value;
}

QString createNoModelRun(const QString& root) {
    QString error;
    auto writer = RunWriterV2::start(root, baseData(), &error);
    require(writer.has_value(), qPrintable(error));
    require(writer->appendEvent(event("event-1", 1, Route::Hit), "crop1", &error),
            qPrintable(error));
    require(writer->appendEvent(event("event-2", 2, Route::Waste), "crop2", &error),
            qPrintable(error));
    require(writer->appendEvent(event("event-3", 3, Route::Unresolved), "crop3", &error),
            qPrintable(error));
    require(writer->finalize(RunStatus::Completed, "2026-07-24T10:00:02Z",
                             "duration", 19.5, &error),
            qPrintable(error));
    return QDir(root).filePath("run_summary.json");
}

void testNoModelRoundTripAndStrictness() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary directory");
    const QString runRoot = QDir(temporary.path()).filePath("run");
    const QString summary = createNoModelRun(runRoot);
    QString error;
    auto loaded = RunManifestV2::load(summary, &error);
    require(loaded.has_value(), qPrintable(error));
    require(!loaded->data().model && loaded->data().events.size() == 3,
            "no-model events round-trip");
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
    object.insert("unexpected", true);
    QFile corrupt(summary);
    require(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate), "open corrupt");
    corrupt.write(QJsonDocument(object).toJson());
    corrupt.close();
    require(!RunManifestV2::load(summary, &error), "unknown root field rejected");
}

void testTwoAndThreeClassHistory() {
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
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    testNoModelRoundTripAndStrictness();
    testTwoAndThreeClassHistory();
    testValidationEdges();
    return 0;
}
