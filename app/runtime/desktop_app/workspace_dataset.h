#pragma once

class QAction;
class QTabWidget;
class QWidget;

namespace desktop_app::workspace {

struct DatasetWorkspaceControls {
    QAction* datasetReviewAction = nullptr;
    QWidget* operationDock = nullptr;
    QTabWidget* operationalTabs = nullptr;
    QWidget* captureTab = nullptr;
};

QWidget* buildDatasetWorkspace(const DatasetWorkspaceControls& controls);

}  // namespace desktop_app::workspace
