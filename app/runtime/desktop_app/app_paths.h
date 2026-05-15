#pragma once

#include <QJsonArray>
#include <QString>

#include "model_registry_service.h"

struct AppPaths {
    QString applicationDir;
    QString projectRoot;
    QString sessionLogPath;
    DefaultWorkspacePaths defaultWorkspacePaths;
};

AppPaths resolveAppPaths(const QJsonArray& registryEntries);
