#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class QAction;
class QDockWidget;
class QLabel;
class QPushButton;
class QStatusBar;

class ReportsWorkspaceController : public QObject {
    Q_OBJECT

public:
    struct Dependencies {
        QAction* openRunFolderAction = nullptr;
        QAction* openOutputAction = nullptr;
        QPushButton* liveOpenRunButton = nullptr;
        QAction* showLogsAction = nullptr;
        QAction* showDiagnosticsAction = nullptr;
        QAction* systemDiagnosticsAction = nullptr;
        QDockWidget* logDock = nullptr;
        QDockWidget* diagnosticsDock = nullptr;
        QLabel* statusLabel = nullptr;
        QStatusBar* statusBar = nullptr;
    };

    explicit ReportsWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

    void setCurrentRunDir(const QString& runDir);
    void refreshOpenRunAvailability();

private:
    void updateOpenRunAvailability();
    void openCurrentRunFolder();
    void wireActions();

    Dependencies deps_;
    QString currentRunDir_;
};
