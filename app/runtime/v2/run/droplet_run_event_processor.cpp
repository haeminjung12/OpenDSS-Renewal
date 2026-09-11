#include "droplet_run_event_processor.h"

#include "../decision/decision_service.h"
#include "../routing/observed_route_tracker.h"

#include <QBuffer>
#include <QDateTime>
#include <QElapsedTimer>
#include <QImage>
#include <QImageWriter>

#include <algorithm>
#include <cmath>
#include <utility>

namespace desktop_app::v2::run {
namespace {

void setError(QString* output, const QString& value) {
    if (output)
        *output = value;
}

QByteArray pngBytes(const cv::Mat& image, QString* error) {
    QImage view(image.data, image.cols, image.rows, image.step,
                QImage::Format_Grayscale8);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Could not open the Droplet Crop buffer."));
        return {};
    }
    QImageWriter writer(&buffer, "PNG");
    if (!writer.write(view)) {
        setError(error, writer.errorString());
        return {};
    }
    return bytes;
}

QString eventId(qint64 number) {
    return QStringLiteral("event_%1").arg(number, 6, 10, QLatin1Char('0'));
}

QString now() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

void DropletRunEventProcessor::reset(std::optional<ModelSnapshot> model,
                                     Classifier classifier) {
    model_ = std::move(model);
    classifier_ = std::move(classifier);
    pending_ = {};
    eventNumber_ = 0;
}

bool DropletRunEventProcessor::processFrame(
    const DropletFrameProcessingResult& processing, qint64 sourceFrameIndex,
    const RoutingSnapshot& entryRouting,
    const HitBoundarySnapshot& currentBoundary,
    const HitDispatcher& hitDispatcher,
    const CompletionSink& completionSink, QString* error) {
    setError(error, {});
    const DropletDetectionFrame& detection = processing.detection;

    for (std::size_t index = 0; index < detection.rejectedCount; ++index) {
        if (!detection.rejectedAreas ||
            !std::isfinite(detection.rejectedAreas[index]) ||
            detection.rejectedAreas[index] <= 0.0) {
            setError(error, QStringLiteral("Rejected candidate area is invalid."));
            return false;
        }
        RunEvent event;
        event.eventId = eventId(++eventNumber_);
        event.detectionTimestamp = now();
        event.sourceFrameIndex = sourceFrameIndex;
        event.rejected = 1;
        if (!complete({std::move(event), {}, false, {}}, completionSink, error))
            return false;
    }

    for (std::size_t index = 0; index < processing.enteredCropCount; ++index) {
        const DropletEnteredCrop& entered = processing.enteredCrops[index];
        const auto duplicate = std::find_if(
            pending_.begin(), pending_.end(),
            [&](const std::optional<PendingEvent>& value) {
                return value && value->trackId == entered.trackId;
            });
        if (duplicate != pending_.end()) {
            setError(error, QStringLiteral("Droplet event track was entered twice."));
            return false;
        }
        const auto slot = std::find_if(
            pending_.begin(), pending_.end(),
            [](const std::optional<PendingEvent>& value) { return !value; });
        if (slot == pending_.end()) {
            setError(error, QStringLiteral("Droplet event track capacity was exceeded."));
            return false;
        }

        slot->emplace();
        PendingEvent& pending = **slot;
        pending.trackId = entered.trackId;
        pending.routing = entryRouting;
        pending.event.eventId = eventId(++eventNumber_);
        pending.event.detectionTimestamp = now();
        pending.event.sourceFrameIndex = sourceFrameIndex;
        pending.event.cropPath =
            QStringLiteral("crops/droplet_%1.png")
                .arg(eventNumber_, 6, 10, QLatin1Char('0'));
        pending.cropBytes = pngBytes(entered.crop.image, error);
        if (pending.cropBytes.isEmpty()) {
            slot->reset();
            return false;
        }

        if (model_) {
            if (!classifier_) {
                slot->reset();
                setError(error, QStringLiteral("Model inference is unavailable."));
                return false;
            }
            QElapsedTimer inferenceTimer;
            inferenceTimer.start();
            auto scores = classifier_(entered.crop.image, error);
            const double inferenceMs =
                static_cast<double>(inferenceTimer.nsecsElapsed()) / 1'000'000.0;
            if (!scores || scores->size() != model_->classes.size() ||
                std::any_of(scores->cbegin(), scores->cend(),
                            [](double score) { return !std::isfinite(score); })) {
                slot->reset();
                if (!error || error->isEmpty())
                    setError(error, QStringLiteral("Model inference result is invalid."));
                return false;
            }
            int bestIndex = 0;
            for (int scoreIndex = 1; scoreIndex < scores->size(); ++scoreIndex) {
                if (scores->at(scoreIndex) > scores->at(bestIndex))
                    bestIndex = scoreIndex;
            }
            pending.event.predictedClassId = model_->classes.at(bestIndex).id;
            pending.event.scores = std::move(*scores);
            pending.event.inferenceTimeMs = inferenceMs;
        }
    }

    for (auto& pending : pending_) {
        if (!pending)
            continue;
        const bool stillVisible = std::any_of(
            detection.visibleTracks.begin(),
            detection.visibleTracks.begin() + detection.visibleTrackCount,
            [&](const DropletTrackObservation& visible) {
                return visible.trackId == pending->trackId;
            });
        if (!stillVisible && !resolveDecision(*pending, hitDispatcher, error))
            return false;
    }

    for (std::size_t index = 0; index < detection.visibleTrackCount; ++index) {
        const DropletTrackObservation& visible = detection.visibleTracks[index];
        if (!std::isfinite(visible.centroid.y) || visible.centroid.y < 0.0)
            continue;
        const auto slot = std::find_if(
            pending_.begin(), pending_.end(),
            [&](const std::optional<PendingEvent>& value) {
                return value && value->trackId == visible.trackId;
            });
        if (slot != pending_.end())
            (*slot)->lastY = visible.centroid.y;
    }

    for (std::size_t index = 0; index < detection.endedTrackCount; ++index) {
        const int trackId = detection.endedTrackIds[index];
        const auto slot = std::find_if(
            pending_.begin(), pending_.end(),
            [&](const std::optional<PendingEvent>& value) {
                return value && value->trackId == trackId;
            });
        if (slot != pending_.end() &&
            !finalize(*slot, currentBoundary, hitDispatcher, completionSink, error)) {
            return false;
        }
    }
    return true;
}

bool DropletRunEventProcessor::finalizeAll(
    const HitBoundarySnapshot& currentBoundary,
    const HitDispatcher& hitDispatcher,
    const CompletionSink& completionSink, QString* error) {
    setError(error, {});
    for (auto& pending : pending_) {
        if (!finalize(pending, currentBoundary, hitDispatcher, completionSink, error))
            return false;
    }
    return true;
}

void DropletRunEventProcessor::discardEntriesFromSource(
    qint64 sourceFrameIndex) noexcept {
    for (auto& pending : pending_) {
        if (pending && pending->event.sourceFrameIndex == sourceFrameIndex)
            pending.reset();
    }
}

std::size_t DropletRunEventProcessor::pendingCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        pending_.cbegin(), pending_.cend(),
        [](const std::optional<PendingEvent>& value) { return value.has_value(); }));
}

bool DropletRunEventProcessor::resolveDecision(
    PendingEvent& pending, const HitDispatcher& hitDispatcher, QString* error) {
    if (pending.decisionResolved)
        return true;
    auto decision = decision::DecisionService::decide(
        pending.routing.triggerMode, pending.event.predictedClassId,
        pending.routing.hitClassId, error);
    if (!decision)
        return false;
    pending.event.decision = *decision;
    pending.decisionResolved = true;
    if (*decision == Route::Waste) {
        pending.event.daqPulseStatus = DaqPulseStatus::NotRequested;
        return true;
    }
    if (!hitDispatcher) {
        setError(error, QStringLiteral("Hit pulse dispatcher is unavailable."));
        return false;
    }
    QString pulseError;
    const DaqPulseStatus status = hitDispatcher(pending.routing, &pulseError);
    if (status != DaqPulseStatus::Issued &&
        status != DaqPulseStatus::SuppressedNotIssued &&
        status != DaqPulseStatus::Failed) {
        setError(error, QStringLiteral("Hit pulse callback returned an invalid status."));
        return false;
    }
    pending.event.daqPulseStatus = status;
    if (status == DaqPulseStatus::Failed) {
        pending.pulseFailed = true;
        pending.pulseError = pulseError;
    }
    return true;
}

bool DropletRunEventProcessor::finalize(
    std::optional<PendingEvent>& slot,
    const HitBoundarySnapshot& currentBoundary,
    const HitDispatcher& hitDispatcher,
    const CompletionSink& completionSink, QString* error) {
    if (!slot)
        return true;
    if (!resolveDecision(*slot, hitDispatcher, error))
        return false;

    routing::ObservedRouteTracker route(currentBoundary);
    if (slot->lastY)
        route.addSample(*slot->lastY);
    slot->event.observedRoute = route.finalize();
    CompletedDropletRunEvent completed{std::move(slot->event),
                                       std::move(slot->cropBytes),
                                       slot->pulseFailed,
                                       slot->pulseError};
    const bool pulseFailed = completed.pulseFailed;
    const QString pulseError = completed.pulseError;
    QString completionError;
    const bool completedOk = complete(std::move(completed), completionSink,
                                      &completionError);
    slot.reset();
    if (!completedOk) {
        if (pulseFailed && completionError != pulseError)
            completionError = pulseError + QStringLiteral(" ") + completionError;
        setError(error, completionError);
        return false;
    }
    if (pulseFailed) {
        setError(error, pulseError);
        return false;
    }
    return true;
}

bool DropletRunEventProcessor::complete(
    CompletedDropletRunEvent completed, const CompletionSink& completionSink,
    QString* error) {
    if (!completionSink) {
        setError(error, QStringLiteral("Run event completion sink is unavailable."));
        return false;
    }
    return completionSink(std::move(completed), error);
}

} // namespace desktop_app::v2::run
