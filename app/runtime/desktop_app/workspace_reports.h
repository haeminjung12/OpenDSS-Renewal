#pragma once

#include <QString>

class QAction;
class QWidget;

namespace desktop_app::workspace {

struct ReportsWorkspaceControls {
    QString logPath;
    bool hardwareFreeMode = false;
    bool viewerOnly = false;
    bool noDaq = false;

    QAction* showLogsAction = nullptr;
    QAction* showDiagnosticsAction = nullptr;
    QAction* openRunFolderAction = nullptr;
};

QWidget* buildReportsWorkspace(const ReportsWorkspaceControls& controls);

}  // namespace desktop_app::workspace

