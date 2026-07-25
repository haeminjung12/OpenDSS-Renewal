#include "observed_route_tracker.h"

#include <cmath>
#include <utility>

namespace desktop_app::v2::routing {

ObservedRouteTracker::ObservedRouteTracker(run::HitBoundarySnapshot boundary)
    : boundary_(std::move(boundary)) {}

void ObservedRouteTracker::addSample(double y) {
    if (std::isfinite(y) && y >= 0.0 && y < boundary_.imageHeight)
        lastY_ = y;
}

run::Route ObservedRouteTracker::finalize() const {
    if (!lastY_ || !std::isfinite(boundary_.boundaryY) ||
        boundary_.imageWidth <= 0 || boundary_.imageHeight <= 0 ||
        boundary_.boundaryY < 0.0 || boundary_.boundaryY >= boundary_.imageHeight ||
        *lastY_ == boundary_.boundaryY) {
        return run::Route::Unresolved;
    }
    const bool positiveY = *lastY_ > boundary_.boundaryY;
    const bool selected = boundary_.hitSide == run::HitSide::PositiveY
                              ? positiveY
                              : !positiveY;
    return selected ? run::Route::Hit : run::Route::Waste;
}

} // namespace desktop_app::v2::routing
