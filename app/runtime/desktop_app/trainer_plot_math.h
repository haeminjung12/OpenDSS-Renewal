#pragma once

namespace desktop_app {

inline double trainerEpochX(double left, double width, int maximumEpoch, double epoch) {
    return maximumEpoch <= 1 ? left : left + width * (epoch - 1.0) / (maximumEpoch - 1.0);
}

} // namespace desktop_app
