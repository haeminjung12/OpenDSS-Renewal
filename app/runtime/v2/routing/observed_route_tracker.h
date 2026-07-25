#pragma once

#include "../run/run_manifest_v2.h"

#include <optional>

namespace desktop_app::v2::routing {

class ObservedRouteTracker final {
public:
    explicit ObservedRouteTracker(run::HitBoundarySnapshot boundary);

    void addSample(double y);
    run::Route finalize() const;

private:
    run::HitBoundarySnapshot boundary_;
    std::optional<double> lastY_;
};

} // namespace desktop_app::v2::routing
