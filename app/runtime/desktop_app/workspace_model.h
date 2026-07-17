#pragma once

#include <QJsonArray>
#include <QString>

#include <functional>

class QAction;
class QComboBox;
class QStackedWidget;
class QWidget;

namespace desktop_app {
struct AppState;
}

namespace desktop_app::workspace {

struct ModelWorkspaceControls {
    QJsonArray registryEntries;
    QString registryFilePath;
    QString registryLoadWarning;

    QComboBox* targetClassCombo = nullptr;
    QAction* imageValidationAction = nullptr;
    QStackedWidget* workspaceStack = nullptr;
    QWidget* validatorWorkspace = nullptr;
    desktop_app::AppState* appState = nullptr;
    std::function<void()> registryChangedCallback;
};

QWidget* buildModelWorkspace(const ModelWorkspaceControls& controls);

} // namespace desktop_app::workspace
