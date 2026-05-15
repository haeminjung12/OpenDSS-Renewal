#include "reports_workspace_controller.h"

#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtGui/QAction>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>

#include "crash_handler.h"

ReportsWorkspaceController::ReportsWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    wireActions();
    updateOpenRunAvailability();
}

void ReportsWorkspaceController::setCurrentRunDir(const QString& runDir) {
    currentRunDir_ = runDir;
    updateOpenRunAvailability();
}

void ReportsWorkspaceController::refreshOpenRunAvailability() {
    updateOpenRunAvailability();
}

void ReportsWorkspaceController::updateOpenRunAvailability() {
    const bool hasRun = !currentRunDir_.trimmed().isEmpty() && QFileInfo(currentRunDir_).isDir();
    if (deps_.liveOpenRunButton) {
        deps_.liveOpenRunButton->setEnabled(hasRun);
    }
    if (deps_.openRunFolderAction) {
        deps_.openRunFolderAction->setEnabled(hasRun);
    }
}

void ReportsWorkspaceController::openCurrentRunFolder() {
    if (currentRunDir_.trimmed().isEmpty() || !QFileInfo(currentRunDir_).isDir()) {
        if (deps_.statusLabel) {
            deps_.statusLabel->setText("No valid run folder is available yet.");
        }
        if (deps_.statusBar) {
            deps_.statusBar->showMessage("Open Run blocked: no valid run");
        }
        logMessage("Open Run blocked: no valid run folder.");
        updateOpenRunAvailability();
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(currentRunDir_));
}

void ReportsWorkspaceController::wireActions() {
    if (deps_.openRunFolderAction) {
        connect(deps_.openRunFolderAction, &QAction::triggered, this, [this]() {
            openCurrentRunFolder();
        });
    }
    if (deps_.openOutputAction) {
        connect(deps_.openOutputAction, &QAction::triggered, this, [this]() {
            openCurrentRunFolder();
        });
    }
    if (deps_.liveOpenRunButton) {
        connect(deps_.liveOpenRunButton, &QPushButton::clicked, this, [this]() {
            openCurrentRunFolder();
        });
    }
    if (deps_.showLogsAction && deps_.logDock) {
        connect(deps_.showLogsAction, &QAction::triggered, deps_.logDock, &QDockWidget::show);
    }
    if (deps_.showDiagnosticsAction && deps_.diagnosticsDock) {
        connect(deps_.showDiagnosticsAction, &QAction::triggered, deps_.diagnosticsDock, &QDockWidget::show);
    }
    if (deps_.systemDiagnosticsAction && deps_.diagnosticsDock) {
        connect(deps_.systemDiagnosticsAction, &QAction::triggered, deps_.diagnosticsDock, &QDockWidget::show);
    }
}
