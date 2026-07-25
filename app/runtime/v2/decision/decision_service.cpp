#include "decision_service.h"

namespace desktop_app::v2::decision {

std::optional<run::Route>
DecisionService::decide(run::TriggerMode mode,
                        const std::optional<QString>& predictedClassId,
                        const std::optional<QString>& hitClassId,
                        QString* error) {
    if (error)
        error->clear();
    if (mode == run::TriggerMode::EveryDroplet)
        return run::Route::Hit;
    if (mode != run::TriggerMode::ClassBased || !predictedClassId ||
        predictedClassId->trimmed().isEmpty() || !hitClassId ||
        hitClassId->trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Class-Based Sorting requires a Predicted Class and Hit Class.");
        return std::nullopt;
    }
    return *predictedClassId == *hitClassId ? run::Route::Hit : run::Route::Waste;
}

} // namespace desktop_app::v2::decision
