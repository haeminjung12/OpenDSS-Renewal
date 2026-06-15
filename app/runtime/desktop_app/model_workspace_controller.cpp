#include "model_workspace_controller.h"

#include <QtGui/QAction>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTextEdit>

#include "model_registry_service.h"

ModelWorkspaceController::ModelWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies) {
    wireRegistrySelection();
    wireModelManagerAction();
}

QString ModelWorkspaceController::summaryForRow(int row) const {
    if (!deps_.registryEntries || row < 0 || row >= deps_.registryEntries->size()) {
        return {};
    }
    return registryEntrySummary(deps_.registryEntries->at(row).toObject(), deps_.registryFilePath,
                                deps_.registryLoadWarning);
}

void ModelWorkspaceController::openModelManager() {
    if (deps_.operationDock) {
        deps_.operationDock->show();
    }
    if (deps_.operationalTabs && deps_.modelManagerWidget) {
        deps_.operationalTabs->setCurrentWidget(deps_.modelManagerWidget);
    }
}

void ModelWorkspaceController::wireRegistrySelection() {
    if (!deps_.modelRegistryTable || !deps_.modelDetailsText || !deps_.registryEntries) {
        return;
    }

    if (!deps_.registryEntries->isEmpty()) {
        deps_.modelDetailsText->setPlainText(summaryForRow(0));
    }

    connect(deps_.modelRegistryTable, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        const QString summary = summaryForRow(currentRow);
        if (summary.isEmpty()) {
            return;
        }
        deps_.modelDetailsText->setPlainText(summary);
    });
}

void ModelWorkspaceController::wireModelManagerAction() {
    if (!deps_.modelManagerAction) {
        return;
    }
    connect(deps_.modelManagerAction, &QAction::triggered, this, [this]() { openModelManager(); });
}
