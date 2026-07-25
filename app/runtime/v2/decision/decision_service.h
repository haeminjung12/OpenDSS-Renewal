#pragma once

#include "../run/run_manifest_v2.h"

#include <QString>

#include <optional>

namespace desktop_app::v2::decision {

class DecisionService final {
public:
    static std::optional<run::Route> decide(run::TriggerMode mode,
                                             const std::optional<QString>& predictedClassId,
                                             const std::optional<QString>& hitClassId,
                                             QString* error = nullptr);
};

} // namespace desktop_app::v2::decision
