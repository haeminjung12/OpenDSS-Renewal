#pragma once

#include <QString>

class QAction;
class QLineEdit;
class QWidget;

namespace desktop_app::workspace {

struct ReportsWorkspaceControls {
    QString logPath;
    bool viewerOnly = false;
    QString outputRoot;

    QAction* showLogsAction = nullptr;
    QAction* showDiagnosticsAction = nullptr;
    QAction* openRunFolderAction = nullptr;
    QLineEdit* outputRootEdit = nullptr;
};

QWidget* buildReportsWorkspace(const ReportsWorkspaceControls& controls);

} // namespace desktop_app::workspace
