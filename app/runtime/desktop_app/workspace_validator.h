#pragma once

#include <QString>

class QAction;
class QWidget;

namespace desktop_app::workspace {

struct ValidatorWorkspaceControls {
    QString modelPath;
    QString metadataPath;
    QAction* imageValidationAction = nullptr;
};

QWidget* buildValidatorWorkspace(const ValidatorWorkspaceControls& controls);

} // namespace desktop_app::workspace
