#include "app_context.h"

AppContext::AppContext() = default;

AppContext::AppContext(const AppOptions& optionsIn, const AppPaths& pathsIn) : options(optionsIn), paths(pathsIn) {}
