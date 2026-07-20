#include "trainer_plot_math.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace {

bool requireNear(double actual, double expected, const char* label) {
    if (std::abs(actual - expected) <= 1e-9)
        return true;
    std::fprintf(stderr, "%s: expected %.12f, got %.12f\n", label, expected, actual);
    return false;
}

} // namespace

int main() {
    constexpr double left = 62.0;
    constexpr double width = 670.0;
    bool passed = true;

    for (int maximumEpoch : std::array<int, 4>{2, 3, 5, 35}) {
        for (int epoch = 1; epoch <= maximumEpoch; ++epoch) {
            const double pathX = desktop_app::trainerEpochX(left, width, maximumEpoch, epoch);
            const double markerX = desktop_app::trainerEpochX(left, width, maximumEpoch, epoch);
            passed &= requireNear(markerX, pathX, "marker/path coordinate");
        }
    }

    const double singlePathX = desktop_app::trainerEpochX(left, width, 1, 1.0);
    const double singleMarkerX = desktop_app::trainerEpochX(left, width, 1, 1.0);
    passed &= requireNear(singlePathX, left, "single-epoch path coordinate");
    passed &= requireNear(singleMarkerX, singlePathX, "single-epoch marker/path coordinate");
    return passed ? 0 : 2;
}
