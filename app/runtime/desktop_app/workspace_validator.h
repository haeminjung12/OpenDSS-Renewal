#pragma once

#include <QString>
#include <QJsonArray>

#include <functional>

class QAction;
class QWidget;

namespace desktop_app::workspace {

struct ValidatorWorkspaceControls {
    QString modelPath;
    QString metadataPath;
    QString pythonExecutable;
    QString datasetPath;
    QString outputPath;
    QString trainerPythonPath;
    QJsonArray registryEntries;
    QAction* imageValidationAction = nullptr;
    std::function<void(const QString&)> imageSummaryChangedCallback;
};

QWidget* buildValidatorWorkspace(const ValidatorWorkspaceControls& controls);

} // namespace desktop_app::workspace
