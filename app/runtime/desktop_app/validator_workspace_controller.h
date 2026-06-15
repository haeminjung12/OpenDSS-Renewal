#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QString>

#include "pipeline_runner.h"
#include "app_types.h"

class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

class BackgroundTaskRegistry;

class ValidatorWorkspaceController : public QObject {
    Q_OBJECT

  public:
    using ResolvePathCallback = std::function<QString(const QString&)>;

    struct Dependencies {
        QWidget* parentWindow = nullptr;
        QAction* imageValidationAction = nullptr;
        QLineEdit* onnxEdit = nullptr;
        QLineEdit* metaEdit = nullptr;
        QLabel* pythonStatusItem = nullptr;
        QString preparedDatasetPath;
        QString validationRunsRoot;
        QString appDir;
        ResolvePathCallback resolveAppRelative;

        QLineEdit* seqFolderEdit = nullptr;
        QPushButton* seqBrowseBtn = nullptr;
        QPushButton* seqLoadBtn = nullptr;
        QPushButton* seqStartBtn = nullptr;
        QPushButton* seqStopBtn = nullptr;
        QLabel* seqStatusLabel = nullptr;
        QLabel* statusLabel = nullptr;
        QWidget* pipelineWidget = nullptr;
        QWidget* labviewWidget = nullptr;
        QWidget* detectWidget = nullptr;
        QPushButton* pipelineStartBtn = nullptr;
        QPushButton* pipelineStopBtn = nullptr;
        QPushButton* startBtn = nullptr;
        QPushButton* stopBtn = nullptr;
        QPushButton* reconnectBtn = nullptr;
        QPushButton* applyBtn = nullptr;
        bool* viewerOnly = nullptr;
        std::atomic<bool>* pipelineEnabled = nullptr;
        std::shared_ptr<std::vector<SequenceFrame>>* sequenceFrames = nullptr;
        QMutex* sequenceMutex = nullptr;
        std::atomic<bool>* sequenceRunning = nullptr;
        std::atomic<bool>* sequenceStop = nullptr;
        std::atomic<bool>* sequenceLoading = nullptr;
        std::thread* sequenceThread = nullptr;
        BackgroundTaskRegistry* backgroundTasks = nullptr;
    };

    explicit ValidatorWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

    void setSequenceUiRunning(bool running) const;
    void updateSequenceStatus(const QString& text) const;
    void stopSequenceTest();

  private:
    static QString formatBytes(size_t bytes);
    static QStringList collectSequenceFiles(const QString& dirPath);

    QString defaultValidationOutput() const;
    QString findPathUpwards(const QString& relativePath) const;
    void openImageValidationDialog();
    void wireValidatorAction();
    void wireSequenceControls();

    Dependencies deps_;
};
