#include "../v2/decision/decision_service.h"
#include "../v2/routing/observed_route_tracker.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace desktop_app::v2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

run::Route observed(run::HitSide side, std::optional<double> y) {
    routing::ObservedRouteTracker tracker({50.0, side, 100, 100});
    if (y)
        tracker.addSample(*y);
    return tracker.finalize();
}

} // namespace

int main() {
    const auto centered = routing::centeredHitBoundary(
        13, 7, run::HitSide::NegativeY);
    require(centered.boundaryY == 3.5 && centered.imageWidth == 13 &&
                centered.imageHeight == 7 &&
                centered.hitSide == run::HitSide::NegativeY,
            "Centered Hit Boundary helper is incorrect.");

    QString error;
    require(decision::DecisionService::decide(run::TriggerMode::EveryDroplet,
                                               std::nullopt, std::nullopt, &error) ==
                run::Route::Hit,
            "Every Droplet must always decide Hit.");
    require(decision::DecisionService::decide(run::TriggerMode::ClassBased,
                                               QString("class_1"), QString("class_1"),
                                               &error) == run::Route::Hit,
            "Matching Class-Based prediction must decide Hit.");
    require(decision::DecisionService::decide(run::TriggerMode::ClassBased,
                                               QString("class_0"), QString("class_1"),
                                               &error) == run::Route::Waste,
            "Nonmatching Class-Based prediction must decide Waste.");
    require(!decision::DecisionService::decide(run::TriggerMode::ClassBased,
                                                std::nullopt, QString("class_1"), &error),
            "Class-Based decision must reject a missing prediction.");
    require(!decision::DecisionService::decide(run::TriggerMode::ClassBased,
                                                QString("class_1"), std::nullopt, &error),
            "Class-Based decision must reject a missing Hit Class.");

    require(observed(run::HitSide::PositiveY, 75.0) == run::Route::Hit &&
                observed(run::HitSide::PositiveY, 25.0) == run::Route::Waste,
            "Positive-Y Hit side is incorrect.");
    require(observed(run::HitSide::NegativeY, 25.0) == run::Route::Hit &&
                observed(run::HitSide::NegativeY, 75.0) == run::Route::Waste,
            "Negative-Y Hit side is incorrect.");
    require(observed(run::HitSide::PositiveY, 50.0) == run::Route::Unresolved &&
                observed(run::HitSide::NegativeY, 50.0) == run::Route::Unresolved &&
                observed(run::HitSide::PositiveY, std::nullopt) ==
                    run::Route::Unresolved,
            "Exact-boundary and no-sample observations must be Unresolved.");
    require(observed(run::HitSide::PositiveY,
                     std::numeric_limits<double>::quiet_NaN()) ==
                run::Route::Unresolved,
            "Nonfinite observations must be ignored.");
    return 0;
}
