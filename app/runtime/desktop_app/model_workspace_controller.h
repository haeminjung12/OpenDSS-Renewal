#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonArray>
#include <QtCore/QString>

class QAction;
class QDockWidget;
class QTabWidget;
class QTableWidget;
class QTextEdit;
class QWidget;

class ModelWorkspaceController : public QObject {
    Q_OBJECT

public:
    struct Dependencies {
        const QJsonArray* registryEntries = nullptr;
        QString registryFilePath;
        QString registryLoadWarning;
        QTableWidget* modelRegistryTable = nullptr;
        QTextEdit* modelDetailsText = nullptr;
        QAction* modelManagerAction = nullptr;
        QDockWidget* operationDock = nullptr;
        QTabWidget* operationalTabs = nullptr;
        QWidget* modelManagerWidget = nullptr;
    };

    explicit ModelWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

private:
    QString summaryForRow(int row) const;
    void openModelManager();
    void wireRegistrySelection();
    void wireModelManagerAction();

    Dependencies deps_;
};
