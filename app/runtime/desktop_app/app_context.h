#pragma once

#include "pipeline_runner.h"
#include "app_paths.h"
#include "app_types.h"

struct AppContext {
    AppOptions options;
    AppPaths paths;

    AppContext();
    AppContext(const AppOptions& optionsIn, const AppPaths& pathsIn);

    bool hardwareFreeMode() const;
};
