#include "../desktop_app/app_types.h"

#include <iostream>
#include <string>

namespace {

bool requireEqual(const QString& actual, const QString& expected, const std::string& message) {
    if (actual != expected) {
        std::cerr << "FAIL: " << message << " expected=" << expected.toStdString()
                  << " actual=" << actual.toStdString() << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!requireEqual(decideEventDirection(-0.001, 120.0, 360, true), "Waste",
                      "negative cumulative Y should be Waste"))
        return 1;
    if (!requireEqual(decideEventDirection(0.001, 120.0, 360, true), "Hit",
                      "positive cumulative Y should be Hit"))
        return 1;
    if (!requireEqual(decideEventDirection(0.0, 120.0, 360, true), "Unknown",
                      "zero cumulative Y should not be guessed from final position"))
        return 1;
    if (!requireEqual(decideEventDirection(3.702, 112.739, 360, true), "Hit",
                      "logged downward trajectory should be Hit even in the upper frame half"))
        return 1;
    if (!requireEqual(decideEventDirection(-0.25, 220.0, 360, true), "Waste",
                      "logged upward trajectory should be Waste even in the lower frame half"))
        return 1;
    if (!requireEqual(decideEventDirection(5.0, 120.0, 360, false), "Unknown",
                      "missing centroid should be Unknown"))
        return 1;
    return 0;
}
