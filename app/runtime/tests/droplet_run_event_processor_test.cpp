#include "../v2/run/droplet_run_event_processor.h"

#include <QCoreApplication>
#include <QImage>
#include <QVector>

#include <cmath>
#include <iostream>
#include <limits>

namespace {

bool require(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

DropletFrameProcessingResult enteredFrame(int trackId, double y) {
    DropletFrameProcessingResult result;
    result.enteredCropCount = 1;
    result.enteredCrops[0].trackId = trackId;
    result.enteredCrops[0].crop.image = cv::Mat(4, 4, CV_8UC1, cv::Scalar(127));
    result.detection.visibleTrackCount = 1;
    result.detection.visibleTracks[0].trackId = trackId;
    result.detection.visibleTracks[0].centroid = {2.0f, static_cast<float>(y)};
    return result;
}

bool lifecycleCase() {
    using namespace desktop_app::v2;
    run::DropletRunEventProcessor processor;
    run::ModelSnapshot model;
    model.id = "model";
    model.name = "Model";
    model.sha256 = QString(64, 'a');
    model.classes = {{"0", "Waste"}, {"1", "Hit"}};
    processor.reset(model, [](const cv::Mat&, QString*) {
        return std::optional<QVector<double>>{{0.1, 0.9}};
    });

    QVector<run::CompletedDropletRunEvent> completed;
    int hitDispatches = 0;
    const auto dispatch = [&](const run::RoutingSnapshot& routing, QString*) {
        ++hitDispatches;
        return routing.physicalDaqOutputEnabled
                   ? run::DaqPulseStatus::Issued
                   : run::DaqPulseStatus::SuppressedNotIssued;
    };
    const auto sink = [&](run::CompletedDropletRunEvent event, QString*) {
        completed.push_back(std::move(event));
        return true;
    };
    const run::RoutingSnapshot routing{run::TriggerMode::ClassBased,
                                       QStringLiteral("1"), false};
    run::HitBoundarySnapshot boundary{50.0, run::HitSide::PositiveY, 100, 100};
    QString error;

    double rejectedArea = 12.0;
    auto first = enteredFrame(7, 40.0);
    first.detection.rejectedAreas = &rejectedArea;
    first.detection.rejectedCount = 1;
    if (!require(processor.processFrame(first, 10, routing, boundary, dispatch,
                                        sink, &error),
                 "first lifecycle frame") ||
        !require(completed.size() == 1 && completed[0].event.rejected == 1,
                 "rejected event completion") ||
        !require(processor.pendingCount() == 1, "one pending event")) {
        return false;
    }

    DropletFrameProcessingResult visible;
    visible.detection.visibleTrackCount = 1;
    visible.detection.visibleTracks[0].trackId = 7;
    visible.detection.visibleTracks[0].centroid = {2.0f, 70.0f};
    if (!require(processor.processFrame(visible, 11, routing, boundary, dispatch,
                                        sink, &error),
                 "visible lifecycle frame")) {
        return false;
    }

    DropletFrameProcessingResult missing;
    if (!require(processor.processFrame(missing, 12, routing, boundary, dispatch,
                                        sink, &error),
                 "decision lifecycle frame") ||
        !require(hitDispatches == 1, "decision dispatched once") ||
        !require(completed.size() == 1, "not finalized before lifecycle end")) {
        return false;
    }

    boundary.boundaryY = 60.0;
    DropletFrameProcessingResult ended;
    ended.detection.endedTrackCount = 1;
    ended.detection.endedTrackIds[0] = 7;
    if (!require(processor.processFrame(ended, 13, routing, boundary, dispatch,
                                        sink, &error),
                 "ended lifecycle frame") ||
        !require(completed.size() == 2, "classified event completion")) {
        return false;
    }
    const auto& event = completed[1];
    return require(event.event.eventId == "event_000002", "event identity") &&
           require(event.event.sourceFrameIndex == 10, "entry source index") &&
           require(event.event.predictedClassId == QStringLiteral("1"),
                   "predicted class") &&
           require(event.event.scores == QVector<double>({0.1, 0.9}), "scores") &&
           require(event.event.decision == run::Route::Hit, "decision") &&
           require(event.event.observedRoute == run::Route::Hit,
                   "replacement boundary route") &&
           require(event.event.daqPulseStatus ==
                       run::DaqPulseStatus::SuppressedNotIssued,
                   "suppressed DAQ status") &&
           require(!event.cropBytes.isEmpty(), "crop bytes") &&
           require(processor.pendingCount() == 0, "no pending event");
}

bool invalidInferenceCase() {
    using namespace desktop_app::v2;
    run::DropletRunEventProcessor processor;
    run::ModelSnapshot model;
    model.classes = {{"0", "Waste"}, {"1", "Hit"}};
    processor.reset(model, [](const cv::Mat&, QString*) {
        return std::optional<QVector<double>>{
            {0.5, std::numeric_limits<double>::quiet_NaN()}};
    });
    QString error;
    const auto sink = [](run::CompletedDropletRunEvent, QString*) { return true; };
    const auto dispatch = [](const run::RoutingSnapshot&, QString*) {
        return run::DaqPulseStatus::SuppressedNotIssued;
    };
    const bool accepted = processor.processFrame(
        enteredFrame(1, 20.0), 1,
        {run::TriggerMode::ClassBased, QStringLiteral("1"), false},
        {50.0, run::HitSide::PositiveY, 100, 100}, dispatch, sink, &error);
    return require(!accepted, "invalid inference rejected") &&
           require(error.contains("invalid"), "invalid inference error") &&
           require(processor.pendingCount() == 0,
                   "invalid inference leaves no pending event");
}

bool routingSnapshotSurvivesPendingChangeCase() {
    using namespace desktop_app::v2;
    run::DropletRunEventProcessor processor;
    run::ModelSnapshot model;
    model.classes = {{"c0", "Zero"}, {"c1", "One"}};
    processor.reset(model, [](const cv::Mat&, QString*) {
        return std::optional<QVector<double>>{{0.1, 0.9}};
    });

    QVector<run::CompletedDropletRunEvent> completed;
    QVector<run::RoutingSnapshot> dispatched;
    const auto dispatch = [&](const run::RoutingSnapshot& routing, QString*) {
        dispatched.push_back(routing);
        return routing.physicalDaqOutputEnabled
                   ? run::DaqPulseStatus::Issued
                   : run::DaqPulseStatus::SuppressedNotIssued;
    };
    const auto sink = [&](run::CompletedDropletRunEvent event, QString*) {
        completed.push_back(std::move(event));
        return true;
    };
    const run::RoutingSnapshot initial{
        run::TriggerMode::ClassBased, QStringLiteral("c0"), false};
    const run::RoutingSnapshot changed{
        run::TriggerMode::EveryDroplet, {}, true};
    const run::HitBoundarySnapshot boundary{50.0, run::HitSide::PositiveY, 100,
                                            100};
    QString error;
    auto first = enteredFrame(1, 40.0);
    DropletFrameProcessingResult stillVisible;
    stillVisible.detection.visibleTrackCount = 1;
    stillVisible.detection.visibleTracks[0].trackId = 1;
    stillVisible.detection.visibleTracks[0].centroid = {2.0f, 45.0f};
    if (!require(processor.processFrame(first, 10, initial, boundary, dispatch,
                                        sink, &error),
                 "initial routing entry") ||
        !require(processor.processFrame(stillVisible, 11, initial,
                                        boundary, dispatch, sink, &error),
                 "pending routing visibility")) {
        return false;
    }

    auto second = enteredFrame(2, 30.0);
    second.detection.visibleTracks[0].trackId = 1;
    second.detection.visibleTracks[0].centroid.y = 45.0f;
    second.enteredCrops[0].trackId = 2;
    second.detection.visibleTracks[1] = {
        2, 0, 1.0, second.detection.visibleTracks[0].bbox, {2.0f, 30.0f}};
    second.detection.visibleTrackCount = 2;
    if (!require(processor.processFrame(second, 12, changed, boundary, dispatch,
                                        sink, &error),
                 "changed routing entry") ||
        !require(processor.pendingCount() == 2,
                 "old and new entries remain pending together")) {
        return false;
    }

    DropletFrameProcessingResult ended;
    ended.detection.endedTrackIds[0] = 1;
    ended.detection.endedTrackIds[1] = 2;
    ended.detection.endedTrackCount = 2;
    if (!require(processor.processFrame(ended, 13, changed, boundary, dispatch,
                                        sink, &error),
                 "pending entries finalize after routing change")) {
        return false;
    }
    return require(completed.size() == 2, "both pending entries completed") &&
           require(completed[0].event.decision == run::Route::Waste,
                   "old entry retains its Class-Based decision") &&
           require(completed[1].event.decision == run::Route::Hit,
                   "new entry uses Every-Droplet decision") &&
           require(dispatched.size() == 1 && dispatched[0].triggerMode ==
                       run::TriggerMode::EveryDroplet &&
                       dispatched[0].physicalDaqOutputEnabled,
                   "pending dispatch uses the new entry routing") &&
           require(processor.pendingCount() == 0, "pending entries are drained");
}

bool duplicateAndFlushCase() {
    using namespace desktop_app::v2;
    run::DropletRunEventProcessor processor;
    processor.reset();
    QString error;
    QVector<run::CompletedDropletRunEvent> completed;
    const auto sink = [&](run::CompletedDropletRunEvent event, QString*) {
        completed.push_back(std::move(event));
        return true;
    };
    const auto dispatch = [](const run::RoutingSnapshot&, QString*) {
        return run::DaqPulseStatus::SuppressedNotIssued;
    };
    const run::RoutingSnapshot routing{run::TriggerMode::EveryDroplet, {}, false};
    const run::HitBoundarySnapshot boundary{50.0, run::HitSide::PositiveY, 100, 100};
    if (!require(processor.processFrame(enteredFrame(2, 30.0), 1, routing,
                                        boundary, dispatch, sink, &error),
                 "first duplicate fixture entry")) {
        return false;
    }
    if (!require(!processor.processFrame(enteredFrame(2, 31.0), 2, routing,
                                         boundary, dispatch, sink, &error),
                 "duplicate entry rejected") ||
        !require(error.contains("twice"), "duplicate entry error")) {
        return false;
    }
    error.clear();
    return require(processor.finalizeAll(boundary, dispatch, sink, &error),
                   "pending flush") &&
           require(completed.size() == 1, "pending flush completion") &&
           require(completed[0].event.decision == run::Route::Hit,
                   "pending flush decision");
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    return lifecycleCase() && invalidInferenceCase() &&
                   routingSnapshotSurvivesPendingChangeCase() &&
                   duplicateAndFlushCase()
               ? 0
               : 1;
}
