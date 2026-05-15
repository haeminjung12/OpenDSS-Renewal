#include "app_paths.h"

#include <QCoreApplication>
#include <QDir>

#include "app_utils.h"

AppPaths resolveAppPaths(const QJsonArray& registryEntries) {
    AppPaths paths;
    paths.applicationDir = QCoreApplication::applicationDirPath();
    paths.projectRoot = findProjectRootFromApp();
    paths.sessionLogPath = QDir(paths.applicationDir).filePath("session_log.txt");
    paths.defaultWorkspacePaths = ensureDefaultWorkspaceAssets(registryEntries);
    return paths;
}
