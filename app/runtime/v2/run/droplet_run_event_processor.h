#pragma once

#include "../../detection/droplet_frame_processor.h"
#include "run_manifest_v2.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <array>
#include <functional>
#include <optional>

namespace desktop_app::v2::run {

struct CompletedDropletRunEvent {
    RunEvent event;
    QByteArray cropBytes;
    bool pulseFailed = false;
    QString pulseError;
};

class DropletRunEventProcessor final {
public:
    using Classifier =
        std::function<std::optional<QVector<double>>(const cv::Mat&, QString*)>;
    using HitDispatcher =
        std::function<DaqPulseStatus(const RoutingSnapshot&, QString*)>;
    using CompletionSink =
        std::function<bool(CompletedDropletRunEvent, QString*)>;

    void reset(std::optional<ModelSnapshot> model = {}, Classifier classifier = {});

    bool processFrame(const DropletFrameProcessingResult& processing,
                      qint64 sourceFrameIndex,
                      const RoutingSnapshot& entryRouting,
                      const HitBoundarySnapshot& currentBoundary,
                      const HitDispatcher& hitDispatcher,
                      const CompletionSink& completionSink,
                      QString* error = nullptr);

    bool finalizeAll(const HitBoundarySnapshot& currentBoundary,
                     const HitDispatcher& hitDispatcher,
                     const CompletionSink& completionSink,
                     QString* error = nullptr);

    void discardEntriesFromSource(qint64 sourceFrameIndex) noexcept;
    std::size_t pendingCount() const noexcept;

private:
    struct PendingEvent {
        int trackId = 0;
        RunEvent event;
        QByteArray cropBytes;
        std::optional<double> lastY;
        RoutingSnapshot routing;
        bool decisionResolved = false;
        bool pulseFailed = false;
        QString pulseError;
    };

    bool resolveDecision(PendingEvent& pending,
                         const HitDispatcher& hitDispatcher,
                         QString* error);
    bool finalize(std::optional<PendingEvent>& slot,
                  const HitBoundarySnapshot& currentBoundary,
                  const HitDispatcher& hitDispatcher,
                  const CompletionSink& completionSink,
                  QString* error);
    bool complete(CompletedDropletRunEvent completed,
                  const CompletionSink& completionSink,
                  QString* error);

    std::optional<ModelSnapshot> model_;
    Classifier classifier_;
    std::array<std::optional<PendingEvent>, kDropletTrackCapacity> pending_;
    qint64 eventNumber_ = 0;
};

} // namespace desktop_app::v2::run
