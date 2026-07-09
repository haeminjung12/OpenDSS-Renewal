#include "main_window.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <QtWidgets>
#include <QtCore>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>
#include <QScrollArea>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStandardPaths>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <functional>
#include <atomic>
#include <exception>
#include <cstdio>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <utility>
#include <chrono>
#include <cmath>
#include <string>
#include <opencv2/core.hpp>
#include "app_state.h"
#include "crash_handler.h"
#include "icons.h"
#include "theme.h"
#include "widget_helpers.h"
#include "workspace_camera.h"
#include "workspace_model.h"
#include "workspace_dataset.h"
#include "workspace_validator.h"
#include "workspace_reports.h"
#include "workspace_settings.h"
#include "pipeline_runner.h"
#include "object_names.h"
#include "app_options.h"
#include "app_paths.h"
#include "app_context.h"
#include "app_types.h"
#include "app_utils.h"
#include "live_log_writer.h"
#include "model_registry_service.h"
#include "sequence_summary_writer.h"
#include "camera_workspace_controller.h"
#include "dataset_workspace_controller.h"
#include "reports_workspace_controller.h"
#include "settings_workspace_controller.h"
#include "validator_workspace_controller.h"
#include "frame_types.h"
#include "background_task_registry.h"
#include "camera_worker.h"
#include "dataset_labeler_dialog.h"
#include "image_validation_dialog.h"
#include "stats_figure_window.h"
#include "viewer_window.h"
#include "zoom_image_view.h"
#include "../dataset_capture_session.h"

namespace {

class MainWindowCloseFilter : public QObject {
  public:
    explicit MainWindowCloseFilter(QObject* parent = nullptr) : QObject(parent) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Close) {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (!widget || widget == watched || !widget->isVisible())
                    continue;
                if (qobject_cast<QDialog*>(widget)) {
                    widget->close();
                }
            }
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
        return QObject::eventFilter(watched, event);
    }
};

class HeaderChipClickFilter : public QObject {
  public:
    HeaderChipClickFilter(std::function<void()> onClick, QObject* parent)
        : QObject(parent), onClick_(std::move(onClick)) {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton && onClick_) {
                onClick_();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    std::function<void()> onClick_;
};

class WheelEventForwarder : public QObject {
  public:
    explicit WheelEventForwarder(QWidget* target, QObject* parent = nullptr) : QObject(parent), target_(target) {}

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event && event->type() == QEvent::Wheel && target_ && target_->isVisible()) {
            auto* wheelEvent = static_cast<QWheelEvent*>(event);
            const QPointF targetPos = target_->mapFromGlobal(wheelEvent->globalPosition().toPoint());
            QWheelEvent forwardedEvent(targetPos, wheelEvent->globalPosition(), wheelEvent->pixelDelta(),
                                       wheelEvent->angleDelta(), wheelEvent->buttons(), wheelEvent->modifiers(),
                                       wheelEvent->phase(), wheelEvent->inverted(), wheelEvent->source(),
                                       wheelEvent->pointingDevice());
            QCoreApplication::sendEvent(target_, &forwardedEvent);
            if (forwardedEvent.isAccepted()) {
                event->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

  private:
    QWidget* target_ = nullptr;
};

constexpr int kRuntimeSettingsSchemaVersion = 1;
constexpr const char* kRuntimeSettingsSchemaVersionKey = "runtime/v1/schemaVersion";

QMutex liveEventMutex;
SequenceEventTracker liveEventTracker;

} // namespace

MainWindow::MainWindow(const AppContext& context, QWidget* parent) : QMainWindow(parent), context_(context) {
    nameWidget(this, "MainWindow");
    setWindowTitle("Open Visual Droplet Sorter Suite");
    setWindowIcon(QIcon(":/branding/opendss-icon-512.png"));
    installEventFilter(new MainWindowCloseFilter(this));
    resize(1280, 720);
    setMinimumSize(1100, 650);
}

const AppContext& MainWindow::appContext() const {
    return context_;
}

int MainWindow::runSetupAndEventLoop(QApplication& app, QSettings& runtimeSettings, desktop_app::AppState& appState,
                                     const QJsonArray& registryEntries, const QString& registryFilePath,
                                     const QString& registryLoadWarning, QSplashScreen& splash,
                                     QElapsedTimer& splashTimer) {
    const AppOptions& options = context_.options;
    const DefaultWorkspacePaths& defaultWorkspacePaths = context_.paths.defaultWorkspacePaths;
    const QString& logPath = context_.paths.sessionLogPath;
    const QString initialDaqStatusText = appState.daqStatusText;
#ifdef HAVE_NIDAQMX
    constexpr bool kDaqBuildEnabled = true;
#else
    constexpr bool kDaqBuildEnabled = false;
#endif
    bool viewerOnly = false;
    auto currentThemeMode =
        runtimeSettings.value("shell/theme", "dark").toString().compare("light", Qt::CaseInsensitive) == 0
            ? desktop_app::theme::ThemeMode::Light
            : desktop_app::theme::ThemeMode::Dark;
    auto applyShellTheme = [&]() {
        app.setPalette(desktop_app::theme::palette(currentThemeMode));
        this->setStyleSheet(desktop_app::theme::shellStyleSheet(currentThemeMode));
    };
    applyShellTheme();

    // Live view area with zoomable/pannable view
    auto imageView = new ZoomImageView;
    nameWidget(imageView, "LiveImageView");
    imageView->setFrameShape(QFrame::NoFrame);
    imageView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    imageView->viewport()->setAutoFillBackground(false);
    imageView->setImageLabelObjectName("LiveImageLabel");
    imageView->setMinimumSize(420, 320);
    imageView->setStyleSheet("background:transparent;");
    imageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto cameraImageView = new ZoomImageView;
    nameWidget(cameraImageView, "CameraPreviewImageView");
    cameraImageView->setFrameShape(QFrame::NoFrame);
    cameraImageView->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
    cameraImageView->viewport()->setAutoFillBackground(false);
    cameraImageView->setImageLabelObjectName("CameraPreviewImageLabel");
    cameraImageView->setMinimumSize(420, 320);
    cameraImageView->setStyleSheet("background:transparent;");
    cameraImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Info panel
    auto statusLabel = new QLabel("Status: Not initialized");
    nameWidget(statusLabel, "RuntimeStatusLabel");
    statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statusLabel->setTextFormat(Qt::PlainText);
    auto statsLabel = new QLabel("Resolution: --\nFPS: --\nFrame: --");
    nameWidget(statsLabel, "RuntimeStatsLabel");
    statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsLabel->setTextFormat(Qt::PlainText);
    statsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    statsLabel->setMinimumWidth(220);
    // Buttons
    auto startBtn = new QPushButton("Start");
    auto pipelineStartBtn = new QPushButton("Start Sorting");
    auto pipelineStopBtn = new QPushButton("Stop Sorting");
    pipelineStopBtn->setEnabled(false);
    auto reconnectBtn = new QPushButton("Reconnect");
    auto applyBtn = new QPushButton("Apply Camera Settings");
    auto viewerBtn = new QPushButton("Viewer");
    nameWidget(startBtn, "CameraStartButton");
    nameWidget(pipelineStartBtn, "PipelineStartButton");
    nameWidget(pipelineStopBtn, "PipelineStopButton");
    nameWidget(reconnectBtn, "CameraReconnectButton");
    nameWidget(applyBtn, "CameraApplySettingsButton");
    nameWidget(viewerBtn, "OpenViewerButton");

    auto addDisabledAction = [](QMenu* menu, const QString& text, const char* objectName,
                                const QString& statusTip = QString()) {
        QAction* action = menu->addAction(text);
        nameAction(action, objectName);
        action->setEnabled(false);
        if (!statusTip.isEmpty())
            action->setStatusTip(statusTip);
        return action;
    };

    auto fileMenu = this->menuBar()->addMenu("&File");
    auto openViewerAction = fileMenu->addAction("Open &Viewer");
    auto openOutputAction = fileMenu->addAction("Open Current &Output Folder");
    fileMenu->addSeparator();
    addDisabledAction(fileMenu, "New Project", "FileNewProjectAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Open Project", "FileOpenProjectAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Recent Projects", "FileRecentProjectsAction",
                      "Project files are not wired in this shell step.");
    addDisabledAction(fileMenu, "Save Session", "FileSaveSessionAction", "Session packaging is a future workflow.");
    addDisabledAction(fileMenu, "Export Support Bundle", "FileExportSupportBundleAction",
                      "Support bundle export is a future workflow.");
    fileMenu->addSeparator();
    auto exitAction = fileMenu->addAction("E&xit");

    auto cameraMenu = this->menuBar()->addMenu("&Camera");
    auto reconnectAction = cameraMenu->addAction("&Reconnect");
    auto startPreviewAction = cameraMenu->addAction("Start Preview");
    auto stopPreviewAction = cameraMenu->addAction("Stop Preview");
    auto captureStillAction = cameraMenu->addAction("Capture Still");
    addDisabledAction(cameraMenu, "Camera Presets", "CameraPresetsAction",
                      "Camera preset management is not wired in this shell step.");

    auto datasetMenu = this->menuBar()->addMenu("&Dataset");
    addDisabledAction(datasetMenu, "New Dataset", "DatasetNewDatasetAction",
                      "Dataset workflows are placeholder-only in this shell step.");
    auto datasetOpenAction = datasetMenu->addAction("Open Dataset");
    datasetOpenAction->setStatusTip("Open the Dataset Builder review workspace.");
    auto datasetBuildAction = datasetMenu->addAction("Build Dataset");
    datasetBuildAction->setStatusTip("Open collected crops for Dataset Builder manual review.");
    addDisabledAction(datasetMenu, "Import Images", "DatasetImportImagesAction",
                      "Dataset workflows are placeholder-only in this shell step.");
    auto datasetCaptureFromCameraAction = datasetMenu->addAction("Capture From Camera");
    nameAction(datasetCaptureFromCameraAction, "DatasetCaptureFromCameraAction");
    datasetCaptureFromCameraAction->setStatusTip(
        "Start a live Dataset Builder capture session from the camera stream.");
    auto datasetLabelDatasetAction = datasetMenu->addAction("Label Dataset");
    datasetLabelDatasetAction->setStatusTip(
        "Open the Dataset Builder review workspace. Builder manifests can save reviewed labels.");
    auto datasetReadinessAction = datasetMenu->addAction("Readiness Check");

    auto trainingMenu = this->menuBar()->addMenu("&Training");
    auto trainingEnvironmentSettingsAction = trainingMenu->addAction("Training Environment Settings");
    auto trainingValidateEnvironmentAction = trainingMenu->addAction("Validate Environment");
    auto trainingNewRunAction =
        addDisabledAction(trainingMenu, "New Training Run", "TrainingNewRunAction",
                          "Full GUI-launched training is intentionally unavailable in this readiness prototype.");
    addDisabledAction(trainingMenu, "Open Training Output", "TrainingOpenOutputAction",
                      "Trainer outputs are not wired in this shell step.");

    auto validationMenu = this->menuBar()->addMenu("&Validation");
    auto imageValidationAction = validationMenu->addAction("Image Validation");
    imageValidationAction->setStatusTip("Launch image-level ONNX validation through the external Python validator.");
    auto sequenceValidationAction = addDisabledAction(
        validationMenu, "Sequence Validation", "ValidationSequenceValidationAction",
        "Runner-wrapped sequence validation is not available; artifact comparison remains internal/provisional.");
    addDisabledAction(validationMenu, "Compare Models", "ValidationCompareModelsAction",
                      "Model comparison is not wired in this shell step.");
    addDisabledAction(validationMenu, "Export Validation Report", "ValidationExportReportAction",
                      "Validation reports are not wired in this shell step.");

    auto sortingMenu = this->menuBar()->addMenu("&Sorting");
    auto startSortingAction = sortingMenu->addAction("Start Sorting");
    auto stopSortingAction = sortingMenu->addAction("Stop Sorting");
    auto triggerDisabledAction = addDisabledAction(sortingMenu, "Trigger Disabled", "SortingTriggerDisabledAction",
                                                   "DAQ trigger output is disabled until manually armed.");
    auto armTriggerAction = addDisabledAction(sortingMenu, "Arm Trigger", "SortingArmTriggerAction",
                                              "Trigger arming is not introduced in this declutter pass.");
    auto manualTriggerAction = sortingMenu->addAction("Manual Trigger");
    manualTriggerAction->setStatusTip("Fires the configured DAQ waveform from Live View when hardware is available.");
    auto openRunFolderAction = sortingMenu->addAction("Open Run Folder");

    auto viewMenu = this->menuBar()->addMenu("&View");
    auto showLogsAction = viewMenu->addAction("Show Logs Dock");
    auto showDiagnosticsAction = viewMenu->addAction("Show Diagnostics Dock");
    viewMenu->addSeparator();
    auto resetLayoutAction = viewMenu->addAction("Reset Layout");

    auto settingsMenu = this->menuBar()->addMenu("&Settings");
    addDisabledAction(settingsMenu, "Preferences", "SettingsPreferencesAction",
                      "Preferences are placeholder-only in this shell step.");
    addDisabledAction(settingsMenu, "Paths", "SettingsPathsAction",
                      "Path settings are still controlled by the existing runtime fields.");
    addDisabledAction(settingsMenu, "Hardware Configuration", "SettingsHardwareConfigurationAction",
                      "Hardware settings are still controlled by the existing runtime fields.");

    auto toolsMenu = this->menuBar()->addMenu("&Tools");
    auto systemDiagnosticsAction = toolsMenu->addAction("System Diagnostics");
    addDisabledAction(toolsMenu, "Model Artifact Verification", "ToolsModelArtifactVerificationAction",
                      "Model verification is not wired in this shell step.");
    addDisabledAction(toolsMenu, "Dataset Manifest Verification", "ToolsDatasetManifestVerificationAction",
                      "Dataset verification is not wired in this shell step.");

    auto helpMenu = this->menuBar()->addMenu("&Help");
    auto aboutAction = helpMenu->addAction("&About");

    nameWidget(this->menuBar(), "MainMenuBar");
    nameObject(fileMenu, "FileMenu");
    nameObject(cameraMenu, "CameraMenu");
    nameObject(datasetMenu, "DatasetMenu");
    nameObject(trainingMenu, "TrainingMenu");
    nameObject(validationMenu, "ValidationMenu");
    nameAction(imageValidationAction, "ValidationImageValidationAction");
    nameAction(sequenceValidationAction, "ValidationSequenceValidationAction");
    nameObject(sortingMenu, "SortingMenu");
    nameObject(viewMenu, "ViewMenu");
    nameObject(settingsMenu, "SettingsMenu");
    nameObject(toolsMenu, "ToolsMenu");
    nameObject(helpMenu, "HelpMenu");
    nameAction(openViewerAction, "FileOpenViewerAction");
    nameAction(openOutputAction, "FileOpenOutputFolderAction");
    nameAction(exitAction, "FileExitAction");
    nameAction(reconnectAction, "CameraReconnectAction");
    nameAction(startPreviewAction, "CameraStartPreviewAction");
    nameAction(stopPreviewAction, "CameraStopPreviewAction");
    nameAction(captureStillAction, "CameraCaptureStillAction");
    nameAction(datasetOpenAction, "DatasetOpenDatasetAction");
    nameAction(datasetBuildAction, "DatasetBuildDatasetAction");
    nameAction(datasetLabelDatasetAction, "DatasetLabelDatasetAction");
    nameAction(datasetReadinessAction, "DatasetReadinessCheckAction");
    nameAction(trainingEnvironmentSettingsAction, "TrainingEnvironmentSettingsAction");
    nameAction(trainingValidateEnvironmentAction, "TrainingValidateEnvironmentAction");
    nameAction(trainingNewRunAction, "TrainingNewRunAction");
    nameAction(startSortingAction, "SortingStartLiveAction");
    nameAction(stopSortingAction, "SortingStopLiveAction");
    nameAction(triggerDisabledAction, "SortingTriggerDisabledAction");
    nameAction(armTriggerAction, "SortingArmTriggerAction");
    nameAction(manualTriggerAction, "SortingForceTriggerAction");
    nameAction(openRunFolderAction, "SortingOpenRunFolderAction");
    nameAction(showLogsAction, "ViewShowLogsDockAction");
    nameAction(showDiagnosticsAction, "ViewShowDiagnosticsDockAction");
    nameAction(resetLayoutAction, "ViewResetLayoutAction");
    nameAction(systemDiagnosticsAction, "ToolsSystemDiagnosticsAction");
    nameAction(aboutAction, "HelpAboutAction");

    auto commandStrip = new QToolBar("Command Strip", this);
    commandStrip->setObjectName("CommandStrip");
    commandStrip->setMovable(false);
    commandStrip->setIconSize(QSize(16, 16));
    commandStrip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    commandStrip->addAction(reconnectAction);
    commandStrip->addAction(startPreviewAction);
    commandStrip->addAction(stopPreviewAction);
    commandStrip->addSeparator();
    commandStrip->addAction(startSortingAction);
    commandStrip->addAction(stopSortingAction);
    commandStrip->addSeparator();
    commandStrip->addAction(triggerDisabledAction);
    commandStrip->addAction(armTriggerAction);
    commandStrip->addAction(manualTriggerAction);
    commandStrip->addSeparator();
    commandStrip->addAction(captureStillAction);
    commandStrip->addAction(openViewerAction);
    this->addToolBar(Qt::TopToolBarArea, commandStrip);

    auto displayStrip = new QToolBar("Display Tools", this);
    displayStrip->setObjectName("DisplayToolsStrip");
    displayStrip->setMovable(false);
    displayStrip->setIconSize(QSize(16, 16));
    displayStrip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto copyFrameAction = displayStrip->addAction("Copy");
    auto copyDocumentAction = displayStrip->addAction("Copy Doc");
    displayStrip->addSeparator();
    auto fitAction = displayStrip->addAction("Fit");
    auto oneToOneAction = displayStrip->addAction("1x");
    auto zoomInAction = displayStrip->addAction("Zoom +");
    auto zoomOutAction = displayStrip->addAction("Zoom -");
    displayStrip->addSeparator();
    auto imageRegionAction = displayStrip->addAction("Image Region");
    imageRegionAction->setCheckable(true);
    auto crosshairAction = displayStrip->addAction("Crosshair");
    crosshairAction->setCheckable(true);
    displayStrip->addSeparator();
    auto calibrationAction = displayStrip->addAction("Calibration");
    nameAction(copyFrameAction, "DisplayCopyFrameAction");
    nameAction(copyDocumentAction, "DisplayCopyDocumentAction");
    nameAction(fitAction, "DisplayFitAction");
    nameAction(oneToOneAction, "DisplayOneToOneAction");
    nameAction(zoomInAction, "DisplayZoomInAction");
    nameAction(zoomOutAction, "DisplayZoomOutAction");
    nameAction(imageRegionAction, "DisplayImageRegionAction");
    nameAction(crosshairAction, "DisplayCrosshairAction");
    nameAction(calibrationAction, "DisplayCalibrationAction");
    for (auto* action : {copyFrameAction, copyDocumentAction, fitAction, oneToOneAction, zoomInAction, zoomOutAction,
                         imageRegionAction, crosshairAction, calibrationAction}) {
        action->setStatusTip("Display shell control; runtime behavior is unchanged in this alignment step.");
    }
    fitAction->setStatusTip("Fit the live image inside the available viewer area.");
    crosshairAction->setStatusTip("Show or hide the Live center crosshair.");
    this->addToolBarBreak(Qt::TopToolBarArea);
    this->addToolBar(Qt::TopToolBarArea, displayStrip);
    commandStrip->hide();
    displayStrip->hide();

    // Settings controls
    auto presetCombo = new QComboBox;
    presetCombo->addItem("2304 x 2304", QVariant::fromValue(QSize(2304, 2304)));
    presetCombo->addItem("2304 x 1152", QVariant::fromValue(QSize(2304, 1152)));
    presetCombo->addItem("2304 x 576", QVariant::fromValue(QSize(2304, 576)));
    presetCombo->addItem("2304 x 288", QVariant::fromValue(QSize(2304, 288)));
    presetCombo->addItem("2304 x 144", QVariant::fromValue(QSize(2304, 144)));
    presetCombo->addItem("2304 x 72", QVariant::fromValue(QSize(2304, 72)));
    presetCombo->addItem("2304 x 36", QVariant::fromValue(QSize(2304, 36)));
    presetCombo->addItem("2304 x 16", QVariant::fromValue(QSize(2304, 16)));
    presetCombo->addItem("2304 x 8", QVariant::fromValue(QSize(2304, 8)));
    presetCombo->addItem("2304 x 4", QVariant::fromValue(QSize(2304, 4)));
    presetCombo->addItem("1152 x 1152", QVariant::fromValue(QSize(1152, 1152)));
    presetCombo->addItem("1152 x 576", QVariant::fromValue(QSize(1152, 576)));
    presetCombo->addItem("1152 x 288", QVariant::fromValue(QSize(1152, 288)));
    presetCombo->addItem("1152 x 144", QVariant::fromValue(QSize(1152, 144)));
    presetCombo->addItem("576 x 576", QVariant::fromValue(QSize(576, 576)));
    presetCombo->addItem("576 x 288", QVariant::fromValue(QSize(576, 288)));
    presetCombo->addItem("576 x 144", QVariant::fromValue(QSize(576, 144)));
    presetCombo->addItem("288 x 288", QVariant::fromValue(QSize(288, 288)));
    presetCombo->addItem("288 x 144", QVariant::fromValue(QSize(288, 144)));
    presetCombo->addItem("144 x 144", QVariant::fromValue(QSize(144, 144)));
    presetCombo->addItem("Custom", QVariant::fromValue(QSize(-1, -1)));

    auto customWidthSpin = new QSpinBox;
    customWidthSpin->setRange(1, 4096);
    customWidthSpin->setValue(2304);
    auto customHeightSpin = new QSpinBox;
    customHeightSpin->setRange(1, 4096);
    customHeightSpin->setValue(2304);
    presetCombo->addItem("512 x 128", QVariant::fromValue(QSize(512, 128)));
    presetCombo->addItem("512 x 64", QVariant::fromValue(QSize(512, 64)));
    presetCombo->addItem("256 x 64", QVariant::fromValue(QSize(256, 64)));
    presetCombo->addItem("256 x 32", QVariant::fromValue(QSize(256, 32)));

    auto binCombo = new QComboBox;
    binCombo->addItems({"1", "2", "4"});
    binCombo->setCurrentIndex(0);

    auto bitsCombo = new QComboBox;
    bitsCombo->addItems({"8", "12", "16"});
    bitsCombo->setCurrentIndex(0); // default 8-bit

    auto lutMinSpin = new QSpinBox;
    auto lutMaxSpin = new QSpinBox;
    auto lutMinSlider = new QSlider(Qt::Horizontal);
    auto lutMaxSlider = new QSlider(Qt::Horizontal);
    auto lutRangeLabel = new QLabel("Scale: 0 - 255");
    lutMinSpin->setRange(0, 255);
    lutMaxSpin->setRange(0, 255);
    lutMinSpin->setValue(0);
    lutMaxSpin->setValue(255);
    lutMinSlider->setRange(0, 255);
    lutMaxSlider->setRange(0, 255);
    lutMinSlider->setValue(0);
    lutMaxSlider->setValue(255);
    lutMinSlider->setTickPosition(QSlider::TicksBelow);
    lutMaxSlider->setTickPosition(QSlider::TicksBelow);

    auto exposureSpin = new QDoubleSpinBox;
    exposureSpin->setSuffix(" ms");
    exposureSpin->setDecimals(3);
    exposureSpin->setSingleStep(0.1);
    exposureSpin->setMinimum(0.01);
    exposureSpin->setMaximum(10000.0);
    exposureSpin->setValue(10.0);

    auto readoutCombo = new QComboBox;
    readoutCombo->addItem("Fast", DCAMPROP_READOUTSPEED__FASTEST);
    readoutCombo->addItem("Slow", DCAMPROP_READOUTSPEED__SLOWEST);
    readoutCombo->setCurrentIndex(0);

    auto logCheck = new QCheckBox("Enable logging (session_log.txt)");
    logCheck->setChecked(true);

    // Save controls
    QString defaultSaveDir = defaultWorkspacePaths.root;
    auto savePathEdit = new QLineEdit(defaultSaveDir);
    auto saveBrowseBtn = new QPushButton("...");
    auto saveOpenBtn = new QPushButton("Open Folder");
    auto saveStartBtn = new QPushButton("Start Save");
    auto saveStopBtn = new QPushButton("Stop Save");
    saveStopBtn->setEnabled(false);
    auto captureBtn = new QPushButton("Capture Frame");
    auto saveInfoLabel = new QLabel("Elapsed: 0.0 s\nFrames: 0");
    QDialog* savingDialog = nullptr;
    QLabel* savingDialogLabel = nullptr;
    QProgressBar* savingProgress = nullptr;

    auto displayEverySpin = new QSpinBox;
    displayEverySpin->setMinimum(1);
    displayEverySpin->setMaximum(1000);
    displayEverySpin->setValue(1);

    auto controlLayout = new QVBoxLayout;

    auto grid = new QGridLayout;
    grid->addWidget(new QLabel("Preset"), 0, 0);
    grid->addWidget(presetCombo, 0, 1);
    grid->addWidget(new QLabel("Custom W/H"), 1, 0);
    auto customLayout = new QHBoxLayout;
    customLayout->addWidget(customWidthSpin);
    customLayout->addWidget(customHeightSpin);
    grid->addLayout(customLayout, 1, 1);
    grid->addWidget(new QLabel("Binning"), 2, 0);
    grid->addWidget(binCombo, 2, 1);
    grid->addWidget(new QLabel("Bits"), 5, 0);
    grid->addWidget(bitsCombo, 5, 1);
    grid->addWidget(new QLabel("Exposure (ms)"), 6, 0);
    grid->addWidget(exposureSpin, 6, 1);
    grid->addWidget(new QLabel("Readout speed"), 7, 0);
    grid->addWidget(readoutCombo, 7, 1);
    grid->addWidget(logCheck, 9, 0, 1, 2);
    // Pipeline defaults (fast event detection)
    FastEventConfig pipelineDetectCfg;
    pipelineDetectCfg.bgFrames = 100;
    pipelineDetectCfg.bgUpdateFrames = 50;
    pipelineDetectCfg.resetFrames = 2;
    pipelineDetectCfg.minArea = -1.0;
    pipelineDetectCfg.minAreaFrac = 0.0;
    pipelineDetectCfg.maxAreaFrac = 0.10;
    pipelineDetectCfg.minBbox = 32;
    pipelineDetectCfg.margin = 5;
    pipelineDetectCfg.diffThresh = 15;
    pipelineDetectCfg.blurRadius = 1;
    pipelineDetectCfg.morphRadius = 1;
    pipelineDetectCfg.scale = 0.5;
    pipelineDetectCfg.gapFireShift = 0;

    // Pipeline controls (event detection + ONNX + DAQ)
    auto pipelineEnableCheck = new QCheckBox("Enable pipeline");
    pipelineEnableCheck->setChecked(false);
    auto pipelineStatusLabel = new QLabel("Pipeline: not loaded");
    pipelineStatusLabel->setWordWrap(true);

    auto onnxEdit = new QLineEdit;
    auto onnxBrowseBtn = new QPushButton("...");
    auto metaEdit = new QLineEdit;
    auto metaBrowseBtn = new QPushButton("...");
    auto outputEdit = new QLineEdit;
    auto outputBrowseBtn = new QPushButton("...");
    auto liveModelCombo = new QComboBox;
    liveModelCombo->setEditable(false);
    liveModelCombo->setMinimumContentsLength(32);
    liveModelCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto openLiveModelManagerBtn = new QPushButton("Models");
    openLiveModelManagerBtn->setText("...");
    auto refreshLiveModelsBtn = new QPushButton("Refresh Models");
    auto liveModelSummaryText = new QTextEdit;
    liveModelSummaryText->setReadOnly(true);
    liveModelSummaryText->setMaximumHeight(116);
    liveModelSummaryText->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto targetClassCombo = new QComboBox;
    targetClassCombo->setEditable(false);
    targetClassCombo->setMinimumContentsLength(12);
    targetClassCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto saveCropCheck = new QCheckBox("Save crops");
    auto saveOverlayCheck = new QCheckBox("Save overlays");
    auto liveConfigureSettingsBtn = new QPushButton("Configure");
    nameWidget(liveConfigureSettingsBtn, "LiveConfigureSettingsButton");
    auto datasetCaptureModeCombo = new QComboBox;
    datasetCaptureModeCombo->addItems({"mixed", "hit-only", "waste-only"});
    auto datasetBatchTargetSpin = new QSpinBox;
    datasetBatchTargetSpin->setRange(1, 100000);
    datasetBatchTargetSpin->setValue(100);
    auto datasetStartCaptureBtn = new QPushButton("Start Dataset Capture");
    auto datasetStopCaptureBtn = new QPushButton("Stop and Review");
    datasetStopCaptureBtn->setEnabled(false);
    auto datasetCaptureStatusLabel = new QLabel("Dataset Builder capture: idle");
    datasetCaptureStatusLabel->setWordWrap(true);
    auto loadPipelineBtn = new QPushButton("Load Pipeline");

    auto frameSkipSpin = new QSpinBox;
    frameSkipSpin->setRange(0, 1000);
    frameSkipSpin->setValue(0);

    auto daqDeviceCombo = new QComboBox;
    daqDeviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
    auto daqChannelEdit = new QLineEdit("Dev1/ao0");
    auto amplitudeSpin = new QDoubleSpinBox;
    amplitudeSpin->setDecimals(3);
    amplitudeSpin->setRange(0.0, 10.0);
    amplitudeSpin->setValue(5.0);
    amplitudeSpin->setSuffix(" V");
    auto freqSpin = new QDoubleSpinBox;
    freqSpin->setDecimals(3);
    freqSpin->setRange(0.001, 200.0);
    freqSpin->setValue(10.0);
    freqSpin->setSuffix(" kHz");
    auto durationSpin = new QDoubleSpinBox;
    durationSpin->setDecimals(3);
    durationSpin->setRange(0.1, 10000.0);
    durationSpin->setValue(5.0);
    durationSpin->setSuffix(" ms");
    auto delaySpin = new QDoubleSpinBox;
    delaySpin->setDecimals(3);
    delaySpin->setRange(0.0, 10000.0);
    delaySpin->setValue(0.0);
    delaySpin->setSuffix(" ms");

    auto pipelineLayout = new QGridLayout;
    int row = 0;
    pipelineLayout->addWidget(pipelineEnableCheck, row++, 0, 1, 4);
    pipelineLayout->addWidget(new QLabel("Live model"), row, 0);
    pipelineLayout->addWidget(liveModelCombo, row, 1, 1, 3);
    row++;
    pipelineLayout->addWidget(openLiveModelManagerBtn, row, 1);
    pipelineLayout->addWidget(refreshLiveModelsBtn, row++, 2);
    pipelineLayout->addWidget(new QLabel("Model provenance"), row, 0);
    pipelineLayout->addWidget(liveModelSummaryText, row++, 1, 1, 3);
    pipelineLayout->addWidget(new QLabel("ONNX path"), row, 0);
    pipelineLayout->addWidget(onnxEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(onnxBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Metadata path"), row, 0);
    pipelineLayout->addWidget(metaEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(metaBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Output dir"), row, 0);
    pipelineLayout->addWidget(outputEdit, row, 1, 1, 2);
    pipelineLayout->addWidget(outputBrowseBtn, row++, 3);
    pipelineLayout->addWidget(new QLabel("Target class"), row, 0);
    pipelineLayout->addWidget(targetClassCombo, row++, 1, 1, 3);
    pipelineLayout->addWidget(saveCropCheck, row, 0);
    pipelineLayout->addWidget(saveOverlayCheck, row++, 1, 1, 2);
    pipelineLayout->addWidget(new QLabel("Dataset capture"), row, 0);
    pipelineLayout->addWidget(datasetCaptureModeCombo, row, 1);
    pipelineLayout->addWidget(datasetBatchTargetSpin, row, 2);
    pipelineLayout->addWidget(datasetStartCaptureBtn, row++, 3);
    pipelineLayout->addWidget(datasetStopCaptureBtn, row, 1);
    pipelineLayout->addWidget(datasetCaptureStatusLabel, row++, 2, 1, 2);
    pipelineLayout->addWidget(new QLabel("Frame skip"), row, 0);
    pipelineLayout->addWidget(frameSkipSpin, row++, 1, 1, 2);
    pipelineLayout->addWidget(loadPipelineBtn, row++, 0, 1, 2);
    pipelineLayout->addWidget(pipelineStatusLabel, row++, 0, 1, 4);

    auto pipelineWidget = new QWidget;
    pipelineWidget->setLayout(pipelineLayout);

    auto labviewStatusDot = new QLabel;
    labviewStatusDot->setFixedSize(14, 14);
    labviewStatusDot->setStyleSheet("background:#666;border-radius:7px;border:1px solid #333;");
    auto labviewStatusText = new QLabel("Disconnected");
    auto labviewStatusRow = new QHBoxLayout;
    labviewStatusRow->setContentsMargins(0, 0, 0, 0);
    labviewStatusRow->addWidget(labviewStatusDot);
    labviewStatusRow->addWidget(labviewStatusText, 1);

    auto labviewOutputLabel = new QLabel("Output: --");
    labviewOutputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    auto labviewLayout = new QGridLayout;
    int labRow = 0;
    labviewLayout->addWidget(new QLabel("Status"), labRow, 0);
    labviewLayout->addLayout(labviewStatusRow, labRow++, 1, 1, 2);
    labviewLayout->addWidget(labviewOutputLabel, labRow++, 0, 1, 3);
    labviewLayout->addWidget(new QLabel("Output range"), labRow, 0);
    labviewLayout->addWidget(new QLabel("-10 V to +10 V"), labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("AO channel"), labRow, 0);
    labviewLayout->addWidget(daqChannelEdit, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Amplitude"), labRow, 0);
    labviewLayout->addWidget(amplitudeSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Frequency (kHz)"), labRow, 0);
    labviewLayout->addWidget(freqSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Duration"), labRow, 0);
    labviewLayout->addWidget(durationSpin, labRow++, 1, 1, 2);
    labviewLayout->addWidget(new QLabel("Delay"), labRow, 0);
    labviewLayout->addWidget(delaySpin, labRow++, 1, 1, 2);
    auto labviewTestBtn = new QPushButton("Manual Trigger");
    labviewTestBtn->setEnabled(false);
    labviewTestBtn->setVisible(false);
    labviewTestBtn->setToolTip("Internal DAQ trigger endpoint; use Live View Manual Trigger.");
    labviewLayout->addWidget(labviewTestBtn, labRow++, 0, 1, 2);
    auto labviewReconnectBtn = new QPushButton("Reconnect DAQ");
    labviewLayout->addWidget(labviewReconnectBtn, labRow++, 0, 1, 2);

    auto labviewWidget = new QWidget;
    labviewWidget->setLayout(labviewLayout);

    auto bgFramesSpin = new QSpinBox;
    bgFramesSpin->setRange(1, 10000);
    bgFramesSpin->setValue(pipelineDetectCfg.bgFrames);
    bgFramesSpin->setSuffix(" frames");
    auto bgUpdateSpin = new QSpinBox;
    bgUpdateSpin->setRange(0, 10000);
    bgUpdateSpin->setValue(pipelineDetectCfg.bgUpdateFrames);
    bgUpdateSpin->setSuffix(" frames");
    auto resetFramesSpin = new QSpinBox;
    resetFramesSpin->setRange(1, 1000);
    resetFramesSpin->setValue(pipelineDetectCfg.resetFrames);
    resetFramesSpin->setSuffix(" frames");
    auto minAreaSpin = new QDoubleSpinBox;
    minAreaSpin->setDecimals(1);
    minAreaSpin->setRange(-1.0, 1e9);
    minAreaSpin->setValue(pipelineDetectCfg.minArea);
    minAreaSpin->setSuffix(" px^2");
    minAreaSpin->setToolTip("Minimum detected pixel area. Use -1 for the automatic detector default.");
    auto minAreaFracSpin = new QDoubleSpinBox;
    minAreaFracSpin->setDecimals(4);
    minAreaFracSpin->setRange(0.0, 1.0);
    minAreaFracSpin->setSingleStep(0.001);
    minAreaFracSpin->setValue(pipelineDetectCfg.minAreaFrac);
    auto maxAreaFracSpin = new QDoubleSpinBox;
    maxAreaFracSpin->setDecimals(4);
    maxAreaFracSpin->setRange(0.0, 1.0);
    maxAreaFracSpin->setSingleStep(0.001);
    maxAreaFracSpin->setValue(pipelineDetectCfg.maxAreaFrac);
    maxAreaFracSpin->setToolTip("Maximum detected object area as a fraction of the frame area.");
    auto minBboxSpin = new QSpinBox;
    minBboxSpin->setRange(1, 10000);
    minBboxSpin->setValue(pipelineDetectCfg.minBbox);
    minBboxSpin->setSuffix(" px");
    minBboxSpin->setToolTip(
        "Minimum bounding rectangle width and height. A detected object must be at least this many pixels wide and "
        "this many pixels high.");
    auto marginSpin = new QSpinBox;
    marginSpin->setRange(0, 10000);
    marginSpin->setValue(pipelineDetectCfg.margin);
    marginSpin->setSuffix(" px");
    auto diffThreshSpin = new QSpinBox;
    diffThreshSpin->setRange(0, 255);
    diffThreshSpin->setValue(pipelineDetectCfg.diffThresh);
    diffThreshSpin->setToolTip("Grayscale difference threshold, 0-255.");
    auto blurRadiusSpin = new QSpinBox;
    blurRadiusSpin->setRange(0, 25);
    blurRadiusSpin->setValue(pipelineDetectCfg.blurRadius);
    blurRadiusSpin->setSuffix(" px");
    auto morphRadiusSpin = new QSpinBox;
    morphRadiusSpin->setRange(0, 25);
    morphRadiusSpin->setValue(pipelineDetectCfg.morphRadius);
    morphRadiusSpin->setSuffix(" px");
    auto scaleSpin = new QDoubleSpinBox;
    scaleSpin->setDecimals(3);
    scaleSpin->setRange(0.05, 1.0);
    scaleSpin->setSingleStep(0.05);
    scaleSpin->setValue(pipelineDetectCfg.scale);
    auto gapFireSpin = new QSpinBox;
    gapFireSpin->setRange(0, 10000);
    gapFireSpin->setValue(pipelineDetectCfg.gapFireShift);
    gapFireSpin->setSuffix(" px");

    auto detectLayout = new QGridLayout;
    int detRow = 0;
    detectLayout->addWidget(new QLabel("Background frames"), detRow, 0);
    detectLayout->addWidget(bgFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("BG update frames"), detRow, 0);
    detectLayout->addWidget(bgUpdateSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Reset frames"), detRow, 0);
    detectLayout->addWidget(resetFramesSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area px^2 (-1=auto)"), detRow, 0);
    detectLayout->addWidget(minAreaSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min area frac"), detRow, 0);
    detectLayout->addWidget(minAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Max area frame fraction"), detRow, 0);
    detectLayout->addWidget(maxAreaFracSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Min rectangle size"), detRow, 0);
    detectLayout->addWidget(minBboxSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Margin px"), detRow, 0);
    detectLayout->addWidget(marginSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Diff threshold 0-255"), detRow, 0);
    detectLayout->addWidget(diffThreshSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Blur radius px"), detRow, 0);
    detectLayout->addWidget(blurRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Morph radius px"), detRow, 0);
    detectLayout->addWidget(morphRadiusSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Scale"), detRow, 0);
    detectLayout->addWidget(scaleSpin, detRow++, 1);
    detectLayout->addWidget(new QLabel("Gap fire shift px"), detRow, 0);
    detectLayout->addWidget(gapFireSpin, detRow++, 1);
    auto detectWidget = new QWidget;
    detectWidget->setLayout(detectLayout);
    std::function<void()> scheduleDetectorApply = []() {};

    auto statsEventsLabel = new QLabel("Events: 0");
    auto statsClassLabel = new QLabel("Classes:\n(none)");
    auto statsHitLabel = new QLabel("Classified Hit: 0\nClassified Waste: 0\nWent to Hit: 0\nWent to Waste: 0");
    auto statsLastLabel = new QLabel("Last event: --");
    statsEventsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsClassLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsHitLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsLastLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    statsClassLabel->setWordWrap(true);
    statsLastLabel->setWordWrap(true);
    auto statsResetBtn = new QPushButton("Reset Stats");
    auto statsShowBtn = new QPushButton("Show Figures");

    auto statsLayout = new QVBoxLayout;
    statsLayout->addWidget(statsEventsLabel);
    statsLayout->addWidget(statsHitLabel);
    statsLayout->addWidget(statsLastLabel);
    statsLayout->addWidget(statsClassLabel, 1);
    statsLayout->addWidget(statsShowBtn);
    statsLayout->addWidget(statsResetBtn);
    auto statsWidget = new QWidget;
    statsWidget->setLayout(statsLayout);

    auto seqFolderEdit = new QLineEdit;
    seqFolderEdit->setPlaceholderText("Select recorded sequence folder...");
    auto seqBrowseBtn = new QPushButton("Browse");
    auto seqLoadBtn = new QPushButton("Load into memory");
    auto seqStartBtn = new QPushButton("Run Recorded Sequence");
    seqStartBtn->setToolTip("Replay recorded frames through the detector/classifier with DAQ output disabled.");
    seqStartBtn->setProperty("daqOutputMode", "disabled-for-replay");
    auto seqStopBtn = new QPushButton("Stop Replay");
    seqStartBtn->setEnabled(false);
    seqStopBtn->setEnabled(false);

    auto seqFpsSpin = new QDoubleSpinBox;
    seqFpsSpin->setDecimals(2);
    seqFpsSpin->setRange(0.1, 100000.0);
    seqFpsSpin->setValue(500.0);

    auto seqStatusLabel = new QLabel("No sequence loaded.");
    seqStatusLabel->setWordWrap(true);
    auto seqLogLabel = new QLabel("Log: (none)");
    seqLogLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

    auto seqLayout = new QGridLayout;
    int seqRow = 0;
    seqLayout->addWidget(new QLabel("Folder"), seqRow, 0);
    seqLayout->addWidget(seqFolderEdit, seqRow, 1, 1, 2);
    seqLayout->addWidget(seqBrowseBtn, seqRow++, 3);
    seqLayout->addWidget(new QLabel("FPS"), seqRow, 0);
    seqLayout->addWidget(seqFpsSpin, seqRow++, 1, 1, 2);
    seqLayout->addWidget(seqLoadBtn, seqRow, 0, 1, 2);
    seqLayout->addWidget(seqStartBtn, seqRow, 2);
    seqLayout->addWidget(seqStopBtn, seqRow++, 3);
    seqLayout->addWidget(seqStatusLabel, seqRow++, 0, 1, 4);
    seqLayout->addWidget(seqLogLabel, seqRow++, 0, 1, 4);
    seqLayout->setColumnStretch(1, 1);
    seqLayout->setColumnStretch(2, 1);
    auto seqWidget = new QWidget;
    seqWidget->setLayout(seqLayout);

    auto trainerPythonEdit = new QLineEdit("python");
    auto trainerPythonBrowseBtn = new QPushButton("Browse");
    auto trainerDatasetEdit = new QLineEdit;
    trainerDatasetEdit->setPlaceholderText("Select prepared dataset or labeled dataset root...");
    auto trainerDatasetBrowseBtn = new QPushButton("Browse");
    auto trainerOutputEdit = new QLineEdit;
    trainerOutputEdit->setPlaceholderText("Select training output directory...");
    auto trainerOutputBrowseBtn = new QPushButton("Browse");
    for (auto* edit : {trainerPythonEdit, trainerDatasetEdit, trainerOutputEdit}) {
        edit->setMinimumWidth(0);
        edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }
    auto trainerEnvCheckBtn = new QPushButton("Validate Environment");
    auto trainerConfigurePathBtn = new QPushButton("Configure Path");
    trainerConfigurePathBtn->setFlat(true);
    trainerConfigurePathBtn->setCursor(Qt::PointingHandCursor);
    auto trainerCancelBtn = new QPushButton("Cancel");
    trainerCancelBtn->setEnabled(false);
    auto trainerStartTrainingBtn = new QPushButton("Start Training");
    auto trainerDryRunBtn = new QPushButton("Dry Run");
    auto trainerStatusLabel =
        new QLabel("Trainer idle. Validate the Python environment or run a dry run before training.");
    trainerStatusLabel->setWordWrap(true);
    trainerStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    auto trainerResultText = new QPlainTextEdit;
    trainerResultText->setReadOnly(true);
    trainerResultText->setMinimumHeight(210);
    trainerResultText->setPlainText("Trainer process output appears here.");
    auto trainerProgressBar = new QProgressBar;
    trainerProgressBar->setRange(0, 100);
    trainerProgressBar->setValue(0);
    trainerProgressBar->setTextVisible(true);
    trainerProgressBar->setFormat("Idle");

    auto trainerPathsLayout = new QGridLayout;
    trainerPathsLayout->setColumnStretch(0, 0);
    trainerPathsLayout->setColumnStretch(1, 1);
    trainerPathsLayout->setColumnStretch(2, 0);
    trainerPathsLayout->setColumnStretch(3, 0);
    int trainerRow = 0;
    trainerPathsLayout->addWidget(new QLabel("Python"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerPythonEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerPythonBrowseBtn, trainerRow++, 3);
    trainerPathsLayout->addWidget(new QLabel("Dataset"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerDatasetEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerDatasetBrowseBtn, trainerRow++, 3);
    trainerPathsLayout->addWidget(new QLabel("Output"), trainerRow, 0);
    trainerPathsLayout->addWidget(trainerOutputEdit, trainerRow, 1, 1, 2);
    trainerPathsLayout->addWidget(trainerOutputBrowseBtn, trainerRow++, 3);
    auto trainerPathsGroup = new QGroupBox("Paths");
    trainerPathsGroup->setMinimumWidth(0);
    trainerPathsGroup->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    trainerPathsGroup->setLayout(trainerPathsLayout);

    auto trainerActionsLayout = new QHBoxLayout;
    trainerActionsLayout->addWidget(trainerEnvCheckBtn);
    trainerActionsLayout->addStretch(1);

    auto trainerEnvironmentPanel = new QFrame;
    trainerEnvironmentPanel->setProperty("panel", true);
    auto trainerEnvironmentLayout = new QVBoxLayout;
    trainerEnvironmentLayout->setContentsMargins(12, 12, 12, 12);
    trainerEnvironmentLayout->setSpacing(10);
    auto trainerEnvironmentTitle = new QLabel("ENVIRONMENT");
    trainerEnvironmentTitle->setProperty("panelTitle", true);
    auto trainerEnvironmentSubtitle = new QLabel("External Python trainer - not bundled");
    trainerEnvironmentSubtitle->setProperty("mutedText", true);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentTitle);
    trainerEnvironmentLayout->addWidget(trainerEnvironmentSubtitle);
    const QVector<QPair<QString, QString>> trainerCheckRows = {
        {"Python executable", "Configured by TrainerPythonPathEdit"},
        {"Trainer package", "Validated by module import"},
        {"PyTorch / CUDA", "Validated by env-check"},
        {"Dataset manifest", "Checked by dry run or training command"},
        {"Training output", "Written by external trainer process"},
    };
    for (const auto& row : trainerCheckRows) {
        auto* checkRow = new QFrame;
        checkRow->setProperty("trainerCheckRow", true);
        auto* checkLayout = new QHBoxLayout;
        checkLayout->setContentsMargins(8, 6, 8, 6);
        checkLayout->setSpacing(8);
        auto* dot = new QLabel("!");
        dot->setProperty("statusDot", true);
        auto* label = new QLabel(row.first);
        label->setProperty("panelTitle", true);
        auto* value = new QLabel(row.second);
        value->setProperty("mutedText", true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        checkLayout->addWidget(dot);
        checkLayout->addWidget(label, 1);
        checkLayout->addWidget(value, 2);
        checkRow->setLayout(checkLayout);
        trainerEnvironmentLayout->addWidget(checkRow);
    }
    trainerActionsLayout->addWidget(trainerConfigurePathBtn);
    trainerEnvironmentLayout->addLayout(trainerActionsLayout);
    auto trainerLastCheckedLabel = new QLabel("Last checked: this session only");
    trainerLastCheckedLabel->setProperty("mutedText", true);
    trainerEnvironmentLayout->addWidget(trainerLastCheckedLabel, 0, Qt::AlignRight);
    trainerEnvironmentPanel->setLayout(trainerEnvironmentLayout);

    auto trainerFormPanel = new QFrame;
    trainerFormPanel->setProperty("panel", true);
    auto trainerFormLayout = new QVBoxLayout;
    trainerFormLayout->setContentsMargins(12, 12, 12, 12);
    trainerFormLayout->setSpacing(10);
    auto trainerFormTitle = new QLabel("RUN TRAINING");
    trainerFormTitle->setProperty("panelTitle", true);
    trainerFormLayout->addWidget(trainerFormTitle);
    trainerFormLayout->addWidget(trainerPathsGroup);
    auto trainerHyperGrid = new QGridLayout;
    trainerHyperGrid->setHorizontalSpacing(10);
    trainerHyperGrid->setVerticalSpacing(8);
    auto trainerArchitectureCombo = new QComboBox;
    trainerArchitectureCombo->addItem("SqueezeNet", "squeezenet1_1");
    trainerArchitectureCombo->addItem("ResNet-18", "resnet18");
    trainerArchitectureCombo->addItem("ResNet-34", "resnet34");
    auto trainerPretrainedGroup = new QButtonGroup(this);
    trainerPretrainedGroup->setExclusive(true);
    auto trainerPretrainedImageNetBtn = new QPushButton("ImageNet");
    auto trainerPretrainedNoneBtn = new QPushButton("None");
    for (auto* button : {trainerPretrainedImageNetBtn, trainerPretrainedNoneBtn}) {
        button->setCheckable(true);
        button->setMinimumHeight(28);
    }
    trainerPretrainedImageNetBtn->setChecked(true);
    trainerPretrainedGroup->addButton(trainerPretrainedImageNetBtn, 1);
    trainerPretrainedGroup->addButton(trainerPretrainedNoneBtn, 0);
    auto trainerPretrainedSegment = new QWidget;
    auto trainerPretrainedLayout = new QHBoxLayout;
    trainerPretrainedLayout->setContentsMargins(0, 0, 0, 0);
    trainerPretrainedLayout->setSpacing(2);
    trainerPretrainedLayout->addWidget(trainerPretrainedImageNetBtn);
    trainerPretrainedLayout->addWidget(trainerPretrainedNoneBtn);
    trainerPretrainedSegment->setLayout(trainerPretrainedLayout);
    auto trainerEpochsSpin = new QSpinBox;
    trainerEpochsSpin->setRange(1, 500);
    trainerEpochsSpin->setValue(50);
    auto trainerBatchSpin = new QSpinBox;
    trainerBatchSpin->setRange(1, 256);
    trainerBatchSpin->setValue(32);
    auto trainerLrSpin = new QDoubleSpinBox;
    trainerLrSpin->setDecimals(5);
    trainerLrSpin->setRange(0.0001, 1.0);
    trainerLrSpin->setValue(0.001);
    auto addTrainerFormCell = [&](int row, int column, const QString& labelText, QWidget* editor) {
        auto* label = new QLabel(labelText);
        label->setProperty("metricLabel", true);
        trainerHyperGrid->addWidget(label, row, column);
        trainerHyperGrid->addWidget(editor, row, column + 1);
    };
    addTrainerFormCell(0, 0, "Architecture", trainerArchitectureCombo);
    addTrainerFormCell(0, 2, "Pretrained", trainerPretrainedSegment);
    auto* trainerEpochsLabel = new QLabel("Epochs");
    trainerEpochsLabel->setProperty("metricLabel", true);
    auto* trainerBatchLabel = new QLabel("Batch size");
    trainerBatchLabel->setProperty("metricLabel", true);
    auto* trainerLrLabel = new QLabel("Learning rate");
    trainerLrLabel->setProperty("metricLabel", true);
    trainerHyperGrid->addWidget(trainerEpochsLabel, 1, 0);
    trainerHyperGrid->addWidget(trainerEpochsSpin, 1, 1);
    trainerHyperGrid->addWidget(trainerBatchLabel, 1, 2);
    trainerHyperGrid->addWidget(trainerBatchSpin, 1, 3);
    trainerHyperGrid->addWidget(trainerLrLabel, 2, 0);
    trainerHyperGrid->addWidget(trainerLrSpin, 2, 1);
    trainerFormLayout->addLayout(trainerHyperGrid);
    auto trainerLaunchRow = new QHBoxLayout;
    trainerLaunchRow->addWidget(trainerStartTrainingBtn);
    trainerLaunchRow->addWidget(trainerDryRunBtn);
    trainerLaunchRow->addWidget(trainerCancelBtn);
    trainerLaunchRow->addStretch(1);
    trainerFormLayout->addLayout(trainerLaunchRow);
    trainerFormPanel->setLayout(trainerFormLayout);

    auto trainerLogPanel = new QFrame;
    trainerLogPanel->setProperty("panel", true);
    auto trainerLogLayout = new QVBoxLayout;
    trainerLogLayout->setContentsMargins(12, 12, 12, 12);
    trainerLogLayout->setSpacing(10);
    auto trainerLogTitle = new QLabel("PROGRESS / LOG");
    trainerLogTitle->setProperty("panelTitle", true);
    trainerLogLayout->addWidget(trainerLogTitle);
    trainerLogLayout->addWidget(trainerStatusLabel);
    trainerLogLayout->addWidget(trainerProgressBar);
    trainerLogLayout->addWidget(trainerResultText, 1);
    trainerLogPanel->setLayout(trainerLogLayout);

    auto trainerRecentRunsPanel = new QFrame;
    trainerRecentRunsPanel->setProperty("panel", true);
    auto trainerRecentRunsLayout = new QVBoxLayout;
    trainerRecentRunsLayout->setContentsMargins(12, 12, 12, 12);
    trainerRecentRunsLayout->setSpacing(10);
    auto trainerRecentRunsTitle = new QLabel("RECENT RUNS");
    trainerRecentRunsTitle->setProperty("panelTitle", true);
    trainerRecentRunsLayout->addWidget(trainerRecentRunsTitle);
    auto trainerRecentRunsTable = new QTableWidget(3, 3);
    trainerRecentRunsTable->setObjectName("TrainerRecentRunsTable");
    trainerRecentRunsTable->setHorizontalHeaderLabels({"Run", "Acc", "State"});
    trainerRecentRunsTable->verticalHeader()->setVisible(false);
    trainerRecentRunsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    trainerRecentRunsTable->setSelectionMode(QAbstractItemView::NoSelection);
    trainerRecentRunsTable->setMinimumHeight(140);
    const QVector<std::array<QString, 3>> trainerRuns = {
        {QString("run_2026-04-29"), QString("96.4%"), QString("complete")},
        {QString("run_2026-04-22"), QString("94.1%"), QString("complete")},
        {QString("run_2026-04-15"), QString("--"), QString("failed")},
    };
    for (int row = 0; row < trainerRuns.size(); ++row) {
        for (int col = 0; col < 3; ++col) {
            auto* item = new QTableWidgetItem(trainerRuns.at(row).at(col));
            item->setToolTip(trainerRuns.at(row).at(col));
            trainerRecentRunsTable->setItem(row, col, item);
        }
    }
    trainerRecentRunsTable->horizontalHeader()->setStretchLastSection(true);
    trainerRecentRunsLayout->addWidget(trainerRecentRunsTable);
    trainerRecentRunsPanel->setLayout(trainerRecentRunsLayout);

    auto trainerAdvancedPanel = new QGroupBox("Advanced - augmentations & schedulers");
    auto trainerAdvancedLayout = new QVBoxLayout;
    auto trainerFlipCheck = new QCheckBox("Random horizontal flip");
    trainerFlipCheck->setChecked(true);
    auto trainerRotationCheck = new QCheckBox("Random rotation +/-15 deg");
    trainerRotationCheck->setChecked(true);
    auto trainerColorJitterCheck = new QCheckBox("Color jitter");
    auto trainerRandomCropCheck = new QCheckBox("Random crop");
    trainerRandomCropCheck->setChecked(true);
    auto trainerSchedulerCombo = new QComboBox;
    trainerSchedulerCombo->addItems({"StepLR", "CosineAnnealing", "None"});
    trainerAdvancedLayout->addWidget(trainerFlipCheck);
    trainerAdvancedLayout->addWidget(trainerRotationCheck);
    trainerAdvancedLayout->addWidget(trainerColorJitterCheck);
    trainerAdvancedLayout->addWidget(trainerRandomCropCheck);
    trainerAdvancedLayout->addWidget(new QLabel("Scheduler"));
    trainerAdvancedLayout->addWidget(trainerSchedulerCombo);
    trainerAdvancedPanel->setLayout(trainerAdvancedLayout);

    auto trainerWidget = new QWidget;
    auto trainerLayout = new QHBoxLayout;
    trainerLayout->setContentsMargins(16, 16, 16, 16);
    trainerLayout->setSpacing(12);
    auto trainerLeftScroll = new QScrollArea;
    trainerLeftScroll->setObjectName("TrainerWorkspaceLeftScrollArea");
    trainerLeftScroll->setWidgetResizable(true);
    trainerLeftScroll->setFrameShape(QFrame::NoFrame);
    trainerLeftScroll->setMinimumWidth(0);
    trainerLeftScroll->setFixedWidth(520);
    trainerLeftScroll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto trainerLeftContent = new QWidget;
    trainerLeftContent->setMinimumWidth(0);
    trainerLeftContent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto trainerLeftLayout = new QVBoxLayout;
    trainerLeftLayout->setContentsMargins(0, 0, 0, 0);
    trainerLeftLayout->setSpacing(12);
    trainerLeftLayout->addWidget(trainerEnvironmentPanel);
    trainerLeftLayout->addWidget(trainerFormPanel);
    trainerLeftLayout->addWidget(trainerLogPanel, 1);
    trainerLeftContent->setLayout(trainerLeftLayout);
    trainerLeftScroll->setWidget(trainerLeftContent);
    auto trainerRightScroll = new QScrollArea;
    trainerRightScroll->setObjectName("TrainerWorkspaceRightScrollArea");
    trainerRightScroll->setWidgetResizable(true);
    trainerRightScroll->setFrameShape(QFrame::NoFrame);
    trainerRightScroll->setFixedWidth(300);
    auto trainerRightContent = new QWidget;
    trainerRightContent->setObjectName("TrainerWorkspaceRightStack");
    auto trainerRightLayout = new QVBoxLayout;
    trainerRightLayout->setContentsMargins(0, 0, 0, 0);
    trainerRightLayout->setSpacing(12);
    trainerRightLayout->addWidget(trainerRecentRunsPanel);
    trainerRightLayout->addWidget(trainerAdvancedPanel);
    trainerRightLayout->addStretch(1);
    trainerRightContent->setLayout(trainerRightLayout);
    trainerRightScroll->setWidget(trainerRightContent);
    trainerLayout->addWidget(trainerLeftScroll, 0);
    trainerLayout->addWidget(trainerRightScroll, 0);
    trainerLayout->addStretch(1);
    trainerWidget->setLayout(trainerLayout);

    auto trainerDockProxy = new QWidget;
    auto trainerDockProxyLayout = new QVBoxLayout;
    trainerDockProxyLayout->setContentsMargins(12, 12, 12, 12);
    auto trainerDockProxyLabel =
        new QLabel("Trainer readiness now opens in the main Trainer workspace. Training launch remains disabled.");
    trainerDockProxyLabel->setWordWrap(true);
    auto trainerDockProxyButton = new QPushButton("Open Trainer Workspace");
    nameWidget(trainerDockProxyButton, "TrainerDockOpenWorkspaceButton");
    trainerDockProxyLayout->addWidget(trainerDockProxyLabel);
    trainerDockProxyLayout->addWidget(trainerDockProxyButton);
    trainerDockProxyLayout->addStretch(1);
    trainerDockProxy->setLayout(trainerDockProxyLayout);

    nameWidget(presetCombo, "CameraPresetComboBox");
    nameWidget(customWidthSpin, "CameraCustomWidthSpinBox");
    nameWidget(customHeightSpin, "CameraCustomHeightSpinBox");
    nameWidget(binCombo, "CameraBinningComboBox");
    nameWidget(bitsCombo, "CameraBitsComboBox");
    nameWidget(exposureSpin, "CameraExposureSpinBox");
    nameWidget(readoutCombo, "CameraReadoutSpeedComboBox");
    nameWidget(displayEverySpin, "CameraDisplayEverySpinBox");
    nameWidget(lutMinSpin, "CameraLutMinSpinBox");
    nameWidget(lutMaxSpin, "CameraLutMaxSpinBox");
    nameWidget(lutMinSlider, "CameraLutMinSlider");
    nameWidget(lutMaxSlider, "CameraLutMaxSlider");
    nameWidget(lutRangeLabel, "CameraLutRangeLabel");
    nameWidget(logCheck, "CameraLoggingCheckBox");
    nameWidget(savePathEdit, "SavePathEdit");
    nameWidget(saveBrowseBtn, "SaveBrowseButton");
    nameWidget(saveOpenBtn, "SaveOpenFolderButton");
    nameWidget(saveStartBtn, "SaveStartButton");
    nameWidget(saveStopBtn, "SaveStopButton");
    nameWidget(captureBtn, "SaveCaptureFrameButton");
    nameWidget(saveInfoLabel, "SaveInfoLabel");
    nameWidget(pipelineEnableCheck, "PipelineEnableCheckBox");
    nameWidget(pipelineStatusLabel, "PipelineStatusLabel");
    nameWidget(onnxEdit, "PipelineOnnxPathEdit");
    nameWidget(onnxBrowseBtn, "PipelineOnnxBrowseButton");
    nameWidget(metaEdit, "PipelineMetadataPathEdit");
    nameWidget(metaBrowseBtn, "PipelineMetadataBrowseButton");
    nameWidget(outputEdit, "PipelineOutputDirEdit");
    nameWidget(outputBrowseBtn, "PipelineOutputBrowseButton");
    nameWidget(liveModelCombo, "LiveModelSelectionComboBox");
    nameWidget(openLiveModelManagerBtn, "LiveModelOpenModelManagerButton");
    nameWidget(refreshLiveModelsBtn, "LiveModelRefreshButton");
    nameWidget(liveModelSummaryText, "LiveModelSummaryText");
    nameWidget(targetClassCombo, "PipelineTargetClassComboBox");
    nameWidget(frameSkipSpin, "PipelineFrameSkipSpinBox");
    nameWidget(saveCropCheck, "PipelineSaveCropsCheckBox");
    nameWidget(saveOverlayCheck, "PipelineSaveOverlaysCheckBox");
    nameWidget(datasetCaptureModeCombo, "DatasetCaptureModeComboBox");
    nameWidget(datasetBatchTargetSpin, "DatasetCaptureBatchTargetSpinBox");
    nameWidget(datasetStartCaptureBtn, "DatasetCaptureStartButton");
    nameWidget(datasetStopCaptureBtn, "DatasetCaptureStopReviewButton");
    nameWidget(datasetCaptureStatusLabel, "DatasetCaptureStatusLabel");
    nameWidget(loadPipelineBtn, "PipelineLoadButton");
    nameWidget(pipelineWidget, "PipelineConfigTab");
    nameWidget(labviewStatusDot, "DaqStatusDot");
    nameWidget(labviewStatusText, "DaqStatusTextLabel");
    nameWidget(labviewOutputLabel, "DaqOutputLabel");
    nameWidget(daqDeviceCombo, "DaqDeviceComboBox");
    nameWidget(daqChannelEdit, "DaqChannelEdit");
    nameWidget(amplitudeSpin, "DaqAmplitudeSpinBox");
    nameWidget(freqSpin, "DaqFrequencySpinBox");
    nameWidget(durationSpin, "DaqDurationSpinBox");
    nameWidget(delaySpin, "DaqDelaySpinBox");
    nameWidget(labviewTestBtn, "DaqManualTriggerButton");
    nameWidget(labviewReconnectBtn, "DaqReconnectButton");
    nameWidget(labviewWidget, "LabviewTab");
    nameWidget(bgFramesSpin, "DetectorBackgroundFramesSpinBox");
    nameWidget(bgUpdateSpin, "DetectorBackgroundUpdateFramesSpinBox");
    nameWidget(resetFramesSpin, "DetectorResetFramesSpinBox");
    nameWidget(minAreaSpin, "DetectorMinAreaSpinBox");
    nameWidget(minAreaFracSpin, "DetectorMinAreaFractionSpinBox");
    nameWidget(maxAreaFracSpin, "DetectorMaxAreaFractionSpinBox");
    nameWidget(minBboxSpin, "DetectorMinBboxSpinBox");
    nameWidget(marginSpin, "DetectorMarginSpinBox");
    nameWidget(diffThreshSpin, "DetectorDiffThresholdSpinBox");
    nameWidget(blurRadiusSpin, "DetectorBlurRadiusSpinBox");
    nameWidget(morphRadiusSpin, "DetectorMorphRadiusSpinBox");
    nameWidget(scaleSpin, "DetectorScaleSpinBox");
    nameWidget(gapFireSpin, "DetectorGapFireShiftSpinBox");
    nameWidget(detectWidget, "EventDetectionTab");
    nameWidget(statsEventsLabel, "StatsEventsLabel");
    nameWidget(statsClassLabel, "StatsClassCountsLabel");
    nameWidget(statsHitLabel, "StatsHitWasteLabel");
    nameWidget(statsLastLabel, "StatsLastEventLabel");
    nameWidget(statsShowBtn, "StatsShowFiguresButton");
    nameWidget(statsResetBtn, "StatsResetButton");
    nameWidget(statsWidget, "StatsTab");
    nameWidget(seqFolderEdit, "SequenceFolderEdit");
    nameWidget(seqBrowseBtn, "SequenceBrowseButton");
    nameWidget(seqLoadBtn, "SequenceLoadButton");
    nameWidget(seqStartBtn, "SequenceStartTestButton");
    nameWidget(seqStopBtn, "SequenceStopButton");
    nameWidget(seqFpsSpin, "SequenceFpsSpinBox");
    nameWidget(seqStatusLabel, "SequenceStatusLabel");
    nameWidget(seqLogLabel, "SequenceLogLabel");
    nameWidget(seqWidget, "SequenceTestTab");
    nameWidget(trainerWidget, "TrainerReadinessTab");
    nameWidget(trainerPathsGroup, "TrainerReadinessInputsGroup");
    nameWidget(trainerPythonEdit, "TrainerPythonPathEdit");
    nameWidget(trainerPythonBrowseBtn, "TrainerPythonBrowseButton");
    nameWidget(trainerDatasetEdit, "TrainerDatasetPathEdit");
    nameWidget(trainerDatasetBrowseBtn, "TrainerDatasetBrowseButton");
    nameWidget(trainerOutputEdit, "TrainerOutputDirEdit");
    nameWidget(trainerOutputBrowseBtn, "TrainerOutputBrowseButton");
    nameWidget(trainerEnvCheckBtn, "TrainerEnvCheckButton");
    nameWidget(trainerConfigurePathBtn, "TrainerConfigurePathButton");
    nameWidget(trainerCancelBtn, "TrainerCancelCheckButton");
    nameWidget(trainerStartTrainingBtn, "TrainerStartTrainingButton");
    nameWidget(trainerDryRunBtn, "TrainerDryRunButton");
    nameWidget(trainerStatusLabel, "TrainerReadinessStatusLabel");
    nameWidget(trainerResultText, "TrainerReadinessResultText");
    nameWidget(trainerProgressBar, "TrainerWorkspaceProgressBar");
    nameWidget(trainerEnvironmentPanel, "TrainerEnvironmentPanel");
    nameWidget(trainerFormPanel, "TrainerRunTrainingPanel");
    nameWidget(trainerLogPanel, "TrainerProgressLogPanel");
    nameWidget(trainerArchitectureCombo, "TrainerArchitectureCombo");
    nameWidget(trainerPretrainedSegment, "TrainerPretrainedSegmentedControl");
    nameWidget(trainerPretrainedImageNetBtn, "TrainerPretrainedImageNetButton");
    nameWidget(trainerPretrainedNoneBtn, "TrainerPretrainedNoneButton");
    nameWidget(trainerEpochsSpin, "TrainerEpochsSpinBox");
    nameWidget(trainerBatchSpin, "TrainerBatchSizeSpinBox");
    nameWidget(trainerLrSpin, "TrainerLearningRateSpinBox");
    nameWidget(trainerRecentRunsPanel, "TrainerRecentRunsPanel");
    nameWidget(trainerAdvancedPanel, "TrainerAdvancedAugmentationSchedulerGroup");
    nameWidget(trainerFlipCheck, "TrainerRandomHorizontalFlipCheckBox");
    nameWidget(trainerRotationCheck, "TrainerRandomRotationCheckBox");
    nameWidget(trainerColorJitterCheck, "TrainerColorJitterCheckBox");
    nameWidget(trainerRandomCropCheck, "TrainerRandomCropCheckBox");
    nameWidget(trainerSchedulerCombo, "TrainerSchedulerCombo");
    auto runStateGroup = new QGroupBox("Run State");
    nameWidget(runStateGroup, "RunStateGroup");
    auto runStateLayout = new QVBoxLayout;
    runStateLayout->addWidget(statusLabel);
    runStateLayout->addWidget(pipelineStatusLabel);
    runStateGroup->setLayout(runStateLayout);

    auto liveMetricsGroup = new QGroupBox("Live Metrics");
    nameWidget(liveMetricsGroup, "LiveMetricsGroup");
    auto liveMetricsLayout = new QVBoxLayout;
    liveMetricsLayout->addWidget(statsLabel);
    liveMetricsLayout->addWidget(statsEventsLabel);
    liveMetricsLayout->addWidget(statsHitLabel);
    liveMetricsLayout->addWidget(statsLastLabel);
    liveMetricsGroup->setLayout(liveMetricsLayout);

    auto currentConfigGroup = new QGroupBox("Current Configuration");
    nameWidget(currentConfigGroup, "CurrentConfigurationGroup");
    auto currentConfigLayout = new QVBoxLayout;
    auto modelSummaryLabel = new QLabel("Model/target: configure in Analysis");
    auto cameraSummaryLabel = new QLabel("Camera preset: configure in Devices");
    auto outputSummaryLabel = new QLabel("Output folder: configure in Analysis or Capture");
    auto triggerSummaryLabel = new QLabel("Trigger: disabled");
    for (auto* item : {modelSummaryLabel, cameraSummaryLabel, outputSummaryLabel, triggerSummaryLabel}) {
        item->setWordWrap(true);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        currentConfigLayout->addWidget(item);
    }
    currentConfigGroup->setLayout(currentConfigLayout);

    auto blockersGroup = new QGroupBox("Blockers");
    nameWidget(blockersGroup, "BlockersGroup");
    auto blockersLayout = new QVBoxLayout;
    auto blockersLabel =
        new QLabel("Camera, model, and DAQ readiness appear here when startup or run actions are blocked.");
    blockersLabel->setWordWrap(true);
    blockersLayout->addWidget(blockersLabel);
    blockersGroup->setLayout(blockersLayout);

    controlLayout->addWidget(runStateGroup);
    controlLayout->addWidget(liveMetricsGroup);
    controlLayout->addWidget(currentConfigGroup);
    controlLayout->addWidget(blockersGroup);
    controlLayout->addStretch(1);

    auto rightWidget = new QWidget;
    nameWidget(rightWidget, "RuntimePanel");
    rightWidget->setLayout(controlLayout);
    rightWidget->setMinimumWidth(320);

    auto zoomStatusLabel = new QLabel("Zoom 100%");
    auto scaleStatusLabel = new QLabel("SF: 1.000 Px");
    auto profileStatusLabel = new QLabel("Default");
    nameWidget(zoomStatusLabel, "ImageZoomStatusLabel");
    nameWidget(scaleStatusLabel, "ImageScaleFactorStatusLabel");
    nameWidget(profileStatusLabel, "ImageProfileStatusLabel");
    for (auto* item : {zoomStatusLabel, scaleStatusLabel, profileStatusLabel}) {
        item->setFrameStyle(QFrame::NoFrame);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }

    auto imageStatusStrip = new QHBoxLayout;
    nameObject(imageStatusStrip, "ImageStatusStrip");
    imageStatusStrip->setContentsMargins(8, 3, 8, 3);
    imageStatusStrip->setSpacing(8);
    imageStatusStrip->addWidget(new QLabel("Zoom"));
    imageStatusStrip->addWidget(zoomStatusLabel);
    imageStatusStrip->addSpacing(8);
    imageStatusStrip->addWidget(scaleStatusLabel);
    imageStatusStrip->addWidget(profileStatusLabel);
    imageStatusStrip->addStretch(1);

    auto imageOverlayStatusFrame = new QFrame;
    nameWidget(imageOverlayStatusFrame, "LiveImageOverlayStatusStrip");
    imageOverlayStatusFrame->setLayout(imageStatusStrip);

    auto liveViewerStack = new QFrame;
    nameWidget(liveViewerStack, "LiveViewerStack");
    QPixmap viewerPattern(36, 36);
    viewerPattern.fill(QColor("#0A0A0A"));
    {
        QPainter painter(&viewerPattern);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(QRect(0, 0, 36, 36), QColor("#0A0A0A"));
        painter.setPen(QPen(QColor(31, 35, 43, 150), 2));
        painter.drawLine(-8, 36, 36, -8);
        painter.drawLine(10, 46, 46, 10);
        painter.setPen(QPen(QColor(20, 184, 166, 28), 1));
        painter.drawLine(-12, 22, 22, -12);
        painter.drawLine(22, 48, 48, 22);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(125, 211, 252, 42));
        painter.drawEllipse(QPointF(18, 18), 1.3, 1.3);
    }
    liveViewerStack->setAutoFillBackground(true);
    QPalette viewerPalette = liveViewerStack->palette();
    viewerPalette.setBrush(QPalette::Window, QBrush(viewerPattern));
    liveViewerStack->setPalette(viewerPalette);
    auto liveViewerStackLayout = new QStackedLayout;
    liveViewerStackLayout->setStackingMode(QStackedLayout::StackAll);
    liveViewerStackLayout->setContentsMargins(0, 0, 0, 0);
    liveViewerStackLayout->addWidget(imageView);

    auto liveViewerOverlay = new QWidget;
    nameWidget(liveViewerOverlay, "LiveViewerHudOverlay");
    liveViewerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    auto liveViewerOverlayLayout = new QVBoxLayout;
    liveViewerOverlayLayout->setContentsMargins(12, 10, 12, 0);
    liveViewerOverlayLayout->setSpacing(0);

    auto liveHudTop = new QHBoxLayout;
    liveHudTop->setContentsMargins(0, 0, 0, 0);
    liveHudTop->setSpacing(8);
    auto liveHudResolution = new QLabel("RES -- x --\nCAM IDLE");
    auto liveHudFrameTime = new QLabel("EXP -- ms\nPROC -- ms");
    auto liveHudToolbar = new QFrame;
    auto liveHudFps = new QLabel("FPS --\nFRAME --\nDROP --");
    nameWidget(liveHudResolution, "LiveViewerHudResolutionLabel");
    nameWidget(liveHudFrameTime, "LiveViewerHudFrameTimeLabel");
    nameWidget(liveHudToolbar, "LiveViewerHudToolbar");
    nameWidget(liveHudFps, "LiveViewerHudFpsLabel");
    for (auto* item : {liveHudResolution, liveHudFrameTime, liveHudFps}) {
        item->setProperty("hudPill", true);
        item->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }
    liveHudToolbar->setProperty("hudPill", true);
    auto liveHudToolbarLayout = new QHBoxLayout;
    liveHudToolbarLayout->setContentsMargins(5, 3, 5, 3);
    liveHudToolbarLayout->setSpacing(2);
    auto addViewerTool = [&](const QString& tip, const QString& iconKey, bool checked = false) {
        auto* button = new QToolButton;
        button->setProperty("viewerTool", true);
        button->setIcon(makeBrandIcon(iconKey, QColor("#FFFFFF"), QColor("#7DD3FC")));
        button->setIconSize(QSize(14, 14));
        button->setToolTip(tip);
        button->setCheckable(checked);
        button->setChecked(checked);
        button->setAutoRaise(true);
        liveHudToolbarLayout->addWidget(button);
        return button;
    };
    auto liveFitTool = addViewerTool("Fit to View: fit the live image inside the viewer.", "fit");
    auto liveCrosshairTool = addViewerTool("Crosshair: show or hide the center reticle.", "crosshair", false);
    liveCrosshairTool->setCheckable(true);
    nameWidget(liveFitTool, "LiveViewerFitButton");
    nameWidget(liveCrosshairTool, "LiveViewerCrosshairToggle");
    liveFitTool->setAccessibleName("Live Fit to View");
    liveCrosshairTool->setAccessibleName("Live Crosshair");
    QObject::connect(liveFitTool, &QToolButton::clicked, fitAction, &QAction::trigger);
    liveHudToolbar->setLayout(liveHudToolbarLayout);
    liveHudTop->addWidget(liveHudResolution);
    liveHudTop->addWidget(liveHudFrameTime);
    liveHudTop->addStretch(1);
    liveHudTop->addWidget(liveHudToolbar, 0, Qt::AlignTop);
    liveHudTop->addStretch(1);
    liveHudTop->addWidget(liveHudFps, 0, Qt::AlignTop);
    liveViewerOverlayLayout->addLayout(liveHudTop);
    liveViewerOverlayLayout->addStretch(1);

    auto liveViewerEmpty = new QLabel("NO LIVE FRAMES  |  PRESS START");
    nameWidget(liveViewerEmpty, "LiveViewerEmptyState");
    liveViewerEmpty->setAlignment(Qt::AlignCenter);
    liveViewerOverlayLayout->addWidget(liveViewerEmpty, 0, Qt::AlignHCenter);

    auto cameraHudResolution = new QLabel("RES -- x --\nCAM IDLE");
    auto cameraHudFrameTime = new QLabel("EXP -- ms\nPROC -- ms");
    auto cameraHudFps = new QLabel("FPS --\nFRAME --\nDROP --");
    auto cameraViewerEmpty = new QLabel("NO LIVE FRAMES  |  PRESS START PREVIEW");
    nameWidget(cameraHudResolution, "CameraViewerHudResolutionLabel");
    nameWidget(cameraHudFrameTime, "CameraViewerHudFrameTimeLabel");
    nameWidget(cameraHudFps, "CameraViewerHudFpsLabel");
    nameWidget(cameraViewerEmpty, "CameraViewerEmptyState");
    cameraViewerEmpty->setAlignment(Qt::AlignCenter);
    auto liveRunBarSlot = new QWidget;
    nameWidget(liveRunBarSlot, "LiveRunControlBarSlot");
    liveRunBarSlot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    liveRunBarSlot->setFixedHeight(0);
    auto liveRunBarSlotLayout = new QVBoxLayout;
    liveRunBarSlotLayout->setContentsMargins(12, 8, 12, 0);
    liveRunBarSlotLayout->setSpacing(0);
    liveRunBarSlot->setLayout(liveRunBarSlotLayout);
    liveViewerOverlayLayout->addWidget(liveRunBarSlot);
    liveViewerOverlayLayout->addSpacing(12);
    liveViewerOverlayLayout->addWidget(imageOverlayStatusFrame);
    liveViewerOverlay->setLayout(liveViewerOverlayLayout);
    auto* liveViewerWheelForwarder = new WheelEventForwarder(imageView->viewport(), liveViewerOverlay);
    liveViewerOverlay->installEventFilter(liveViewerWheelForwarder);
    for (QWidget* widget : liveViewerOverlay->findChildren<QWidget*>()) {
        widget->installEventFilter(liveViewerWheelForwarder);
    }
    liveViewerStackLayout->addWidget(liveViewerOverlay);

    auto liveCrosshairOverlay = new QWidget;
    nameWidget(liveCrosshairOverlay, "LiveViewerCrosshairOverlay");
    liveCrosshairOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    liveCrosshairOverlay->setVisible(liveCrosshairTool->isChecked());
    auto liveCrosshairGrid = new QGridLayout;
    liveCrosshairGrid->setContentsMargins(0, 0, 0, 0);
    liveCrosshairGrid->setSpacing(0);
    liveCrosshairGrid->setRowStretch(0, 1);
    liveCrosshairGrid->setRowStretch(2, 1);
    liveCrosshairGrid->setColumnStretch(0, 1);
    liveCrosshairGrid->setColumnStretch(2, 1);
    auto* crosshairVertical = new QFrame;
    nameWidget(crosshairVertical, "LiveViewerCrosshairVerticalLine");
    crosshairVertical->setFixedWidth(1);
    crosshairVertical->setStyleSheet("background:rgba(125,211,252,0.82);");
    auto* crosshairHorizontal = new QFrame;
    nameWidget(crosshairHorizontal, "LiveViewerCrosshairHorizontalLine");
    crosshairHorizontal->setFixedHeight(1);
    crosshairHorizontal->setStyleSheet("background:rgba(125,211,252,0.82);");
    liveCrosshairGrid->addWidget(crosshairVertical, 0, 1, 3, 1);
    liveCrosshairGrid->addWidget(crosshairHorizontal, 1, 0, 1, 3);
    liveCrosshairOverlay->setLayout(liveCrosshairGrid);
    liveViewerStackLayout->addWidget(liveCrosshairOverlay);
    QObject::connect(liveCrosshairTool, &QToolButton::toggled, crosshairAction, &QAction::setChecked);
    QObject::connect(crosshairAction, &QAction::toggled, liveCrosshairTool, &QToolButton::setChecked);
    QObject::connect(crosshairAction, &QAction::toggled, liveCrosshairOverlay, &QWidget::setVisible);
    crosshairAction->setChecked(liveCrosshairTool->isChecked());

    auto liveDetectorDrawer = new QFrame;
    nameWidget(liveDetectorDrawer, "LiveDetectorTuningDrawer");
    liveDetectorDrawer->setProperty("panel", true);
    liveDetectorDrawer->setFixedWidth(320);
    liveDetectorDrawer->setMinimumHeight(0);
    liveDetectorDrawer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    liveDetectorDrawer->setVisible(true);
    auto liveDetectorDrawerLayout = new QVBoxLayout;
    liveDetectorDrawerLayout->setContentsMargins(12, 10, 12, 12);
    liveDetectorDrawerLayout->setSpacing(8);
    auto liveDetectorHeader = new QHBoxLayout;
    liveDetectorHeader->setContentsMargins(0, 0, 0, 0);
    auto liveDetectorTitle = new QLabel("Detector settings");
    liveDetectorTitle->setProperty("panelTitle", true);
    auto liveDetectorClose = new QToolButton;
    nameWidget(liveDetectorClose, "LiveDetectorTuningCloseButton");
    liveDetectorClose->setText("x");
    liveDetectorClose->setAutoRaise(true);
    liveDetectorHeader->addWidget(liveDetectorTitle);
    liveDetectorHeader->addStretch(1);
    liveDetectorHeader->addWidget(liveDetectorClose);
    liveDetectorDrawerLayout->addLayout(liveDetectorHeader);
    auto liveDetectorBanner = new QLabel("Changes auto-apply after 250 ms. Live capture continues.");
    nameWidget(liveDetectorBanner, "LiveDetectorTuningDebounceLabel");
    liveDetectorBanner->setWordWrap(true);
    liveDetectorBanner->setProperty("mutedText", true);
    liveDetectorDrawerLayout->addWidget(liveDetectorBanner);
    auto liveDetectorGrid = new QGridLayout;
    liveDetectorGrid->setContentsMargins(0, 0, 0, 0);
    liveDetectorGrid->setHorizontalSpacing(8);
    liveDetectorGrid->setVerticalSpacing(6);
    int liveDetectorRow = 0;
    auto addLiveDetectorSpin = [&](const QString& label, QAbstractSpinBox* spin, const char* objectName) {
        auto* labelWidget = new QLabel(label);
        labelWidget->setProperty("mutedText", true);
        nameWidget(spin, objectName);
        if (!spin->toolTip().isEmpty())
            labelWidget->setToolTip(spin->toolTip());
        liveDetectorGrid->addWidget(labelWidget, liveDetectorRow, 0);
        liveDetectorGrid->addWidget(spin, liveDetectorRow++, 1);
    };
    auto makeLinkedIntSpin = [&](QSpinBox* source, int min, int max) {
        auto* spin = new QSpinBox;
        spin->setRange(min, max);
        spin->setValue(source->value());
        spin->setSuffix(source->suffix());
        spin->setToolTip(source->toolTip());
        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged), [=, &scheduleDetectorApply](int value) {
            if (source->value() != value)
                source->setValue(value);
            scheduleDetectorApply();
        });
        QObject::connect(source, qOverload<int>(&QSpinBox::valueChanged), spin, &QSpinBox::setValue);
        return spin;
    };
    auto makeLinkedDoubleSpin = [&](QDoubleSpinBox* source, double min, double max, int decimals, double step) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(min, max);
        spin->setDecimals(decimals);
        spin->setSingleStep(step);
        spin->setValue(source->value());
        spin->setSuffix(source->suffix());
        spin->setToolTip(source->toolTip());
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         [=, &scheduleDetectorApply](double value) {
                             if (!qFuzzyCompare(source->value() + 1.0, value + 1.0))
                                 source->setValue(value);
                             scheduleDetectorApply();
                         });
        QObject::connect(source, qOverload<double>(&QDoubleSpinBox::valueChanged), spin, &QDoubleSpinBox::setValue);
        return spin;
    };
    addLiveDetectorSpin("BG frames", makeLinkedIntSpin(bgFramesSpin, 1, 10000), "LiveDetectorBgFramesSpinBox");
    addLiveDetectorSpin("Diff threshold 0-255", makeLinkedIntSpin(diffThreshSpin, 0, 255),
                        "LiveDetectorDiffThresholdSpinBox");
    addLiveDetectorSpin("Min area (-1 auto)", makeLinkedDoubleSpin(minAreaSpin, -1.0, 1e9, 1, 1.0),
                        "LiveDetectorMinAreaSpinBox");
    addLiveDetectorSpin("Max area frame frac", makeLinkedDoubleSpin(maxAreaFracSpin, 0.0, 1.0, 4, 0.001),
                        "LiveDetectorMaxAreaSpinBox");
    addLiveDetectorSpin("Blur radius", makeLinkedIntSpin(blurRadiusSpin, 0, 25), "LiveDetectorBlurRadiusSpinBox");
    addLiveDetectorSpin("Min rectangle size", makeLinkedIntSpin(minBboxSpin, 1, 10000),
                        "LiveDetectorMinRectangleSizeSpinBox");
    liveDetectorDrawerLayout->addLayout(liveDetectorGrid);
    liveDetectorDrawerLayout->addStretch(1);
    liveDetectorDrawer->setLayout(liveDetectorDrawerLayout);
    auto liveDetectorDrawerOverlay = new QWidget;
    nameWidget(liveDetectorDrawerOverlay, "LiveDetectorTuningDrawerOverlay");
    liveDetectorDrawerOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    liveDetectorDrawerOverlay->setVisible(false);
    auto liveDetectorOverlayLayout = new QHBoxLayout;
    liveDetectorOverlayLayout->setContentsMargins(0, 0, 0, 0);
    liveDetectorOverlayLayout->addStretch(1);
    liveDetectorOverlayLayout->addWidget(liveDetectorDrawer, 0, Qt::AlignRight | Qt::AlignTop);
    liveDetectorDrawerOverlay->setLayout(liveDetectorOverlayLayout);
    liveViewerStackLayout->addWidget(liveDetectorDrawerOverlay);
    liveViewerOverlay->raise();
    liveDetectorDrawerOverlay->raise();
    liveViewerStack->setLayout(liveViewerStackLayout);

    auto imageDisplayWidget = new QWidget;
    nameWidget(imageDisplayWidget, "ImageDisplayWidget");
    imageDisplayWidget->setProperty("viewerCanvas", true);
    imageDisplayWidget->setMinimumHeight(360);
    imageDisplayWidget->setMaximumHeight(QWIDGETSIZE_MAX);
    imageDisplayWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto imageDisplayLayout = new QVBoxLayout;
    imageDisplayLayout->setContentsMargins(0, 0, 0, 0);
    imageDisplayLayout->setSpacing(0);
    imageDisplayLayout->addWidget(liveViewerStack, 1);
    imageDisplayWidget->setLayout(imageDisplayLayout);

    QMdiSubWindow* imageSubWindow = nullptr;

    using desktop_app::ui::makeCollapsedGroup;
    using desktop_app::ui::makeMetric;
    using desktop_app::ui::makeMutedLabel;
    using desktop_app::ui::makePanel;
    using desktop_app::ui::makePanelBody;
    using desktop_app::ui::makeStatusRow;
    using desktop_app::ui::makeToolButton;
    using desktop_app::ui::makeWorkspacePlaceholder;

    auto liveImagePanel = makePanel("Live Image", "Idle");
    liveImagePanel->setObjectName("LiveImagePanel");
    liveImagePanel->setMinimumWidth(480);
    liveImagePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto liveImageBody = makePanelBody(liveImagePanel, 0, 0, 0, 0);
    liveImageBody->addWidget(imageDisplayWidget, 1);

    auto liveRunBar = new QFrame;
    nameWidget(liveRunBar, "LiveRunControlBar");
    liveRunBar->setProperty("panel", true);
    liveRunBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    liveRunBar->setMinimumHeight(54);
    liveRunBar->setMaximumHeight(64);
    auto liveRunLayout = new QHBoxLayout;
    liveRunLayout->setContentsMargins(10, 6, 10, 6);
    liveRunLayout->setSpacing(8);
    pipelineStartBtn->setText("Start Sorting");
    pipelineStartBtn->setEnabled(false);
    pipelineStopBtn->setText("Stop Sorting");
    auto liveForceTriggerBtn = new QPushButton("Manual Trigger");
    nameWidget(liveForceTriggerBtn, "LiveForceTriggerButton");
    liveForceTriggerBtn->setEnabled(false);
    auto liveSnapshotBtn = new QPushButton("Snapshot");
    nameWidget(liveSnapshotBtn, "LiveSnapshotButton");
    auto liveOpenRunBtn = new QPushButton("Open Run");
    nameWidget(liveOpenRunBtn, "LiveOpenRunButton");
    liveOpenRunBtn->setEnabled(false);
    openRunFolderAction->setEnabled(false);
    auto liveDetectorTuningBtn = new QPushButton("Detector");
    nameWidget(liveDetectorTuningBtn, "LiveDetectorTuningButton");
    liveDetectorTuningBtn->setToolTip("Open detector settings.");
    for (auto* button : {startBtn, pipelineStartBtn, pipelineStopBtn, liveForceTriggerBtn,
                         liveSnapshotBtn, liveOpenRunBtn, liveDetectorTuningBtn}) {
        button->setFixedHeight(34);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    startBtn->setText("Start Camera");
    startBtn->setMinimumWidth(138);
    startBtn->setMaximumWidth(150);
    pipelineStartBtn->setMaximumWidth(150);
    pipelineStopBtn->setMaximumWidth(138);
    liveForceTriggerBtn->setMaximumWidth(132);
    liveSnapshotBtn->setMaximumWidth(110);
    liveOpenRunBtn->setMaximumWidth(108);
    liveDetectorTuningBtn->setMaximumWidth(112);
    liveRunLayout->addWidget(startBtn);
    liveRunLayout->addWidget(pipelineStartBtn);
    liveRunLayout->addWidget(pipelineStopBtn);
    liveRunLayout->addSpacing(4);
    liveRunLayout->addWidget(liveForceTriggerBtn);
    liveRunLayout->addStretch(1);
    liveRunLayout->addWidget(liveSnapshotBtn);
    liveRunLayout->addWidget(liveOpenRunBtn);
    liveRunLayout->addWidget(liveDetectorTuningBtn);
    liveRunBar->setLayout(liveRunLayout);
    liveImageBody->addWidget(liveRunBar, 0);

    auto updateLiveRunStartStopVisibility = [&]() {
        const bool running = pipelineEnableCheck->isChecked();
        pipelineStartBtn->setVisible(!running);
        pipelineStopBtn->setVisible(running);
    };
    updateLiveRunStartStopVisibility();

    std::function<void()> updateForceTriggerState = []() {};
    updateForceTriggerState = [&]() {
        const bool waveformValid = !daqChannelEdit->text().trimmed().isEmpty() && amplitudeSpin->value() > 0.0 &&
                                   freqSpin->value() > 0.0 && durationSpin->value() > 0.0;
        appState.daqWaveformValid = waveformValid;
        const bool manualTriggerReady = appState.daqAvailable && !appState.daqDisabled && !appState.daqFault;
        QStringList manualBlockers;
        if (!appState.daqAvailable || appState.daqDisabled)
            manualBlockers << "DAQ is not available";
        if (appState.daqFault)
            manualBlockers << (appState.daqFaultText.isEmpty() ? QStringLiteral("DAQ fault is active")
                                                               : appState.daqFaultText);
        const QString waveformInvalidTip =
            QStringLiteral("Waveform settings are incomplete; click will block before output.");
        const QString manualTriggerTip =
            manualTriggerReady
                ? (waveformValid ? QStringLiteral("Send the configured manual DAQ trigger.")
                                 : waveformInvalidTip)
                : QStringLiteral("Manual Trigger disabled: %1.").arg(manualBlockers.join("; "));
        labviewTestBtn->setEnabled(manualTriggerReady);
        labviewTestBtn->setToolTip(manualTriggerTip);
        labviewTestBtn->setStatusTip(manualTriggerTip);
        const QString liveManualTriggerTip =
            manualTriggerReady
                ? (waveformValid ? QStringLiteral("Send the configured manual DAQ trigger from Live View.")
                                 : QStringLiteral("Live View Manual Trigger: %1").arg(waveformInvalidTip))
                : QStringLiteral("Live View Manual Trigger disabled: %1.").arg(manualBlockers.join("; "));
        liveForceTriggerBtn->setEnabled(manualTriggerReady);
        liveForceTriggerBtn->setToolTip(liveManualTriggerTip);
        liveForceTriggerBtn->setStatusTip(liveManualTriggerTip);
        manualTriggerAction->setEnabled(manualTriggerReady);
        manualTriggerAction->setStatusTip(liveManualTriggerTip);
        manualTriggerAction->setToolTip(liveManualTriggerTip);
    };
    updateForceTriggerState();

    auto eventsMetricLabel = new QLabel("0");
    auto classifiedHitMetricLabel = new QLabel("0");
    auto classifiedWasteMetricLabel = new QLabel("0");
    auto wentToHitMetricLabel = new QLabel("0");
    auto wentToWasteMetricLabel = new QLabel("0");
    auto trigMetricLabel = new QLabel("--");
    nameWidget(eventsMetricLabel, "LiveRunEventsMetricLabel");
    nameWidget(classifiedHitMetricLabel, "LiveRunClassifiedHitMetricLabel");
    nameWidget(classifiedWasteMetricLabel, "LiveRunClassifiedWasteMetricLabel");
    nameWidget(wentToHitMetricLabel, "LiveRunWentToHitMetricLabel");
    nameWidget(wentToWasteMetricLabel, "LiveRunWentToWasteMetricLabel");
    nameWidget(trigMetricLabel, "LiveRunTriggerRateMetricLabel");

    auto runPanel = makePanel("Run");
    runPanel->setObjectName("LiveRunPanel");
    auto runBody = makePanelBody(runPanel);
    auto metricGrid = new QGridLayout;
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setSpacing(1);
    metricGrid->addWidget(makeMetric("Events", eventsMetricLabel), 0, 0);
    metricGrid->addWidget(makeMetric("Classified Hit", classifiedHitMetricLabel), 0, 1);
    metricGrid->addWidget(makeMetric("Classified Waste", classifiedWasteMetricLabel), 1, 0);
    metricGrid->addWidget(makeMetric("Went to Hit", wentToHitMetricLabel), 1, 1);
    metricGrid->addWidget(makeMetric("Went to Waste", wentToWasteMetricLabel), 2, 0);
    metricGrid->addWidget(makeMetric("Trig/s", trigMetricLabel), 2, 1);
    runBody->addLayout(metricGrid);
    auto runStateResetButton = new QPushButton("Reset Counters");
    nameWidget(runStateResetButton, "RunStateResetCountersButton");
    runStateResetButton->setToolTip("Reset the visible live run counters to zero.");
    runStateResetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    runBody->addWidget(runStateResetButton, 0, Qt::AlignRight);
    auto lastDecisionCard = new QFrame;
    nameWidget(lastDecisionCard, "LiveLastDecisionCard");
    auto lastDecisionLayout = new QHBoxLayout;
    lastDecisionLayout->setContentsMargins(10, 8, 10, 8);
    lastDecisionLayout->setSpacing(10);
    auto lastDecisionThumb = new QLabel("64x64");
    nameWidget(lastDecisionThumb, "LiveLastDecisionThumbnail");
    lastDecisionThumb->setAlignment(Qt::AlignCenter);
    lastDecisionThumb->setFixedSize(54, 42);
    lastDecisionThumb->setStyleSheet("background:#0A0A0A;color:#94A3B8;border-radius:3px;font-size:10px;");
    auto lastDecisionText = new QVBoxLayout;
    lastDecisionText->setContentsMargins(0, 0, 0, 0);
    lastDecisionText->setSpacing(1);
    auto lastDecisionTitle = new QLabel("Last decision");
    lastDecisionTitle->setProperty("metricLabel", true);
    auto lastDecisionValue = new QLabel("--");
    nameWidget(lastDecisionValue, "LiveLastDecisionValueLabel");
    lastDecisionValue->setProperty("metricValue", true);
    lastDecisionValue->setStyleSheet("font-size:16px;");
    statsLastLabel->setProperty("mutedText", true);
    statsLastLabel->setWordWrap(true);
    lastDecisionText->addWidget(lastDecisionTitle);
    lastDecisionText->addWidget(lastDecisionValue);
    lastDecisionText->addWidget(statsLastLabel);
    lastDecisionText->addStretch(1);
    lastDecisionLayout->addWidget(lastDecisionThumb);
    lastDecisionLayout->addLayout(lastDecisionText, 1);
    lastDecisionCard->setLayout(lastDecisionLayout);
    runBody->addWidget(lastDecisionCard);

    auto pipelinePanel = makePanel("Pipeline");
    pipelinePanel->setObjectName("LivePipelinePanel");
    auto pipelineBody = makePanelBody(pipelinePanel);
    auto pipelineGrid = new QGridLayout;
    pipelineGrid->setContentsMargins(0, 0, 0, 0);
    pipelineGrid->setHorizontalSpacing(8);
    pipelineGrid->setVerticalSpacing(8);
    pipelineGrid->addWidget(new QLabel("Target"), 0, 0);
    pipelineGrid->addWidget(targetClassCombo, 0, 1);
    pipelineGrid->addWidget(new QLabel("Model"), 1, 0);
    pipelineGrid->addWidget(liveModelCombo, 1, 1);
    pipelineGrid->addWidget(openLiveModelManagerBtn, 1, 2);
    pipelineGrid->addWidget(new QLabel("Output"), 2, 0);
    pipelineGrid->addWidget(outputEdit, 2, 1, 1, 2);
    pipelineGrid->addWidget(saveCropCheck, 3, 0);
    pipelineGrid->addWidget(saveOverlayCheck, 3, 1);
    pipelineGrid->addWidget(loadPipelineBtn, 3, 2);
    pipelineGrid->addWidget(liveConfigureSettingsBtn, 4, 2);
    pipelineBody->addLayout(pipelineGrid);
    pipelineBody->addWidget(pipelineStatusLabel);

    statusLabel->setWordWrap(true);
    statusLabel->setProperty("mutedText", true);
    statsLabel->setProperty("mutedText", true);
    statsLabel->setWordWrap(true);
    statsLabel->setMaximumHeight(72);
    labviewOutputLabel->setProperty("mutedText", true);
    labviewOutputLabel->setWordWrap(true);
    blockersLabel->setProperty("mutedText", true);
    blockersLabel->setWordWrap(true);
    auto rightScroll = new QScrollArea;
    nameWidget(rightScroll, "LiveRightMetricsScrollArea");
    rightScroll->setWidgetResizable(true);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    rightScroll->setMinimumWidth(330);
    rightScroll->setMaximumWidth(360);
    rightScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto rightStack = new QWidget;
    nameWidget(rightStack, "LiveRightMetricsStack");
    rightStack->setMinimumWidth(310);
    auto rightStackLayout = new QVBoxLayout;
    rightStackLayout->setContentsMargins(0, 0, 2, 0);
    rightStackLayout->setSpacing(12);
    rightStackLayout->addWidget(runPanel);
    rightStackLayout->addWidget(pipelinePanel);
    rightStackLayout->addStretch(1);
    rightStack->setLayout(rightStackLayout);
    rightScroll->setWidget(rightStack);

    auto mainSplitter = new QSplitter(Qt::Horizontal);
    nameWidget(mainSplitter, "MainSplitter");
    mainSplitter->addWidget(liveImagePanel);
    mainSplitter->addWidget(rightScroll);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 0);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setCollapsible(1, false);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setSizes({760, 340});

    auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(12);
    mainLayout->addWidget(mainSplitter, 1);
    auto liveWorkspacePage = new QWidget;
    nameWidget(liveWorkspacePage, "LiveWorkspace");
    liveWorkspacePage->setLayout(mainLayout);

    auto leftCaptureTab = new QWidget;
    nameWidget(leftCaptureTab, "OperationalCaptureTab");
    auto leftCaptureLayout = new QVBoxLayout;
    leftCaptureLayout->setContentsMargins(8, 8, 8, 8);
    auto captureContextGroup = new QGroupBox("Acquisition");
    auto captureContextLayout = new QGridLayout;
    auto captureDepthCombo = new QComboBox;
    captureDepthCombo->addItems({"Auto Depth", "8-bit", "12-bit", "16-bit"});
    auto captureTargetCombo = new QComboBox;
    captureTargetCombo->addItems({"Disk", "Memory", "Viewer only"});
    auto leftLoadBtn = makeToolButton("Load");
    auto leftClearBtn = makeToolButton("Clear");
    nameWidget(captureDepthCombo, "CaptureDepthComboBox");
    nameWidget(captureTargetCombo, "CaptureTargetComboBox");
    nameWidget(leftLoadBtn, "CaptureLoadButton");
    nameWidget(leftClearBtn, "CaptureClearButton");
    leftClearBtn->setEnabled(false);
    captureContextLayout->addWidget(new QLabel("Depth"), 0, 0);
    captureContextLayout->addWidget(captureDepthCombo, 0, 1);
    captureContextLayout->addWidget(new QLabel("Target"), 1, 0);
    captureContextLayout->addWidget(captureTargetCombo, 1, 1);
    captureContextLayout->addWidget(leftLoadBtn, 2, 0);
    captureContextLayout->addWidget(leftClearBtn, 2, 1);
    captureContextGroup->setLayout(captureContextLayout);
    leftCaptureLayout->addWidget(captureContextGroup);
    leftCaptureLayout->addStretch(1);
    leftCaptureTab->setLayout(leftCaptureLayout);

    auto leftDevicesTab = new QWidget;
    nameWidget(leftDevicesTab, "OperationalDevicesTab");
    auto leftDevicesLayout = new QVBoxLayout;
    leftDevicesLayout->setContentsMargins(8, 8, 8, 8);
    auto deviceSummaryGroup = new QGroupBox("Camera");
    auto deviceSummaryLayout = new QVBoxLayout;
    auto leftReconnectBtn = makeToolButton("Reconnect Camera");
    nameWidget(leftReconnectBtn, "DevicesReconnectCameraButton");
    deviceSummaryLayout->addWidget(new QLabel("Camera/DCAM startup and reconnect use the existing runtime path."));
    deviceSummaryLayout->addWidget(leftReconnectBtn);
    deviceSummaryGroup->setLayout(deviceSummaryLayout);
    leftDevicesLayout->addWidget(deviceSummaryGroup);
    auto pipelineLabviewGroup = makeCollapsedGroup("DAQ / Trigger", labviewWidget);
    nameWidget(pipelineLabviewGroup, "PipelineLabviewGroup");
    if (auto* pipelineLabviewToggle = pipelineLabviewGroup->findChild<QToolButton*>()) {
        nameWidget(pipelineLabviewToggle, "PipelineLabviewToggleButton");
        pipelineLabviewToggle->setAccessibleName("DAQ / Trigger");
    }
    leftDevicesLayout->addWidget(pipelineLabviewGroup);
    leftDevicesLayout->addStretch(1);
    leftDevicesTab->setLayout(leftDevicesLayout);

    auto leftAnalysisTab = new QWidget;
    nameWidget(leftAnalysisTab, "OperationalAnalysisTab");
    auto leftAnalysisLayout = new QVBoxLayout;
    leftAnalysisLayout->setContentsMargins(8, 8, 8, 8);
    leftAnalysisLayout->addWidget(pipelineWidget);
    auto detectorGroup = makeCollapsedGroup("Detector", detectWidget);
    nameWidget(detectorGroup, "DetectorGroup");
    if (auto* detectorToggle = detectorGroup->findChild<QToolButton*>()) {
        nameWidget(detectorToggle, "DetectorToggleButton");
        detectorToggle->setAccessibleName("Detector");
    }
    leftAnalysisLayout->addWidget(detectorGroup);
    auto detailedStatsGroup = makeCollapsedGroup("Detailed Stats", statsWidget);
    nameWidget(detailedStatsGroup, "DetailedStatsGroup");
    if (auto* detailedStatsToggle = detailedStatsGroup->findChild<QToolButton*>()) {
        nameWidget(detailedStatsToggle, "DetailedStatsToggleButton");
        detailedStatsToggle->setAccessibleName("Detailed Stats");
    }
    leftAnalysisLayout->addWidget(detailedStatsGroup);
    leftAnalysisLayout->addStretch(1);
    leftAnalysisTab->setLayout(leftAnalysisLayout);

    auto operationalTabs = new QTabWidget;
    operationalTabs->setObjectName("OperationalTabs");
    operationalTabs->setAccessibleName("OperationalTabs");
    operationalTabs->addTab(leftCaptureTab, "Capture");
    operationalTabs->addTab(leftDevicesTab, "Devices");
    operationalTabs->addTab(leftAnalysisTab, "Analysis");
    operationalTabs->addTab(trainerDockProxy, "Trainer");

    auto operationDock = new QDockWidget("Capture", this);
    operationDock->setObjectName("OperationalDock");
    operationDock->setAccessibleName("OperationalDock");
    operationDock->setWidget(operationalTabs);
    operationDock->setMinimumWidth(260);
    this->addDockWidget(Qt::LeftDockWidgetArea, operationDock);
    operationDock->hide();

    desktop_app::workspace::CameraWorkspaceControls cameraWorkspaceControls;
    cameraWorkspaceControls.presetCombo = presetCombo;
    cameraWorkspaceControls.bitsCombo = bitsCombo;
    cameraWorkspaceControls.customWidthSpin = customWidthSpin;
    cameraWorkspaceControls.customHeightSpin = customHeightSpin;
    cameraWorkspaceControls.exposureSpin = exposureSpin;
    cameraWorkspaceControls.readoutCombo = readoutCombo;
    cameraWorkspaceControls.binCombo = binCombo;
    cameraWorkspaceControls.lutMinSpin = lutMinSpin;
    cameraWorkspaceControls.lutMaxSpin = lutMaxSpin;
    cameraWorkspaceControls.lutMinSlider = lutMinSlider;
    cameraWorkspaceControls.lutMaxSlider = lutMaxSlider;
    cameraWorkspaceControls.displayEverySpin = displayEverySpin;
    cameraWorkspaceControls.lutRangeLabel = lutRangeLabel;
    cameraWorkspaceControls.savePathEdit = savePathEdit;
    cameraWorkspaceControls.saveBrowseButton = saveBrowseBtn;
    cameraWorkspaceControls.saveOpenButton = saveOpenBtn;
    cameraWorkspaceControls.saveStartButton = saveStartBtn;
    cameraWorkspaceControls.saveStopButton = saveStopBtn;
    cameraWorkspaceControls.saveInfoLabel = saveInfoLabel;
    cameraWorkspaceControls.sequenceWidget = seqWidget;
    cameraWorkspaceControls.sequenceFolderEdit = seqFolderEdit;
    cameraWorkspaceControls.sequenceBrowseButton = seqBrowseBtn;
    cameraWorkspaceControls.sequenceLoadButton = seqLoadBtn;
    cameraWorkspaceControls.sequenceStartButton = seqStartBtn;
    cameraWorkspaceControls.sequenceStopButton = seqStopBtn;
    cameraWorkspaceControls.sequenceFpsSpin = seqFpsSpin;
    cameraWorkspaceControls.sequenceStatusLabel = seqStatusLabel;
    cameraWorkspaceControls.sequenceLogLabel = seqLogLabel;
    auto cameraControlsStack = desktop_app::workspace::buildCameraControlsStack(cameraWorkspaceControls);
    rightStackLayout->insertWidget(2, cameraControlsStack);

    desktop_app::workspace::ModelWorkspaceControls modelWorkspaceControls;
    modelWorkspaceControls.registryEntries = registryEntries;
    modelWorkspaceControls.registryFilePath = registryFilePath;
    modelWorkspaceControls.registryLoadWarning = registryLoadWarning;
    modelWorkspaceControls.targetClassCombo = targetClassCombo;
    modelWorkspaceControls.imageValidationAction = imageValidationAction;
    modelWorkspaceControls.appState = &appState;
    auto modelWorkspacePage = desktop_app::workspace::buildModelWorkspace(modelWorkspaceControls);

    desktop_app::workspace::DatasetWorkspaceControls datasetWorkspaceControls;
    datasetWorkspaceControls.datasetReviewAction = datasetLabelDatasetAction;
    datasetWorkspaceControls.operationDock = operationDock;
    datasetWorkspaceControls.operationalTabs = operationalTabs;
    datasetWorkspaceControls.captureTab = leftCaptureTab;
    auto datasetWorkspacePage = desktop_app::workspace::buildDatasetWorkspace(datasetWorkspaceControls);

    auto trainerWorkspacePage = new QWidget;
    nameWidget(trainerWorkspacePage, "TrainerWorkspace");
    auto trainerWorkspaceLayout = new QVBoxLayout;
    trainerWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
    trainerWorkspaceLayout->setSpacing(0);
    trainerWorkspaceLayout->addWidget(trainerWidget, 1);
    trainerWorkspacePage->setLayout(trainerWorkspaceLayout);

    auto validatorResolveAppRelative = [](const QString& path) -> QString {
        if (path.isEmpty())
            return path;
        QFileInfo info(path);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = dir.filePath(path);
            if (QFileInfo::exists(candidate))
                return QFileInfo(candidate).absoluteFilePath();
            const QString modelCandidate = dir.filePath("models/" + QFileInfo(path).fileName());
            if (QFileInfo::exists(modelCandidate))
                return QFileInfo(modelCandidate).absoluteFilePath();
            if (!dir.cdUp())
                break;
        }
        return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path);
    };
    auto validatorTrainerPythonPath = []() -> QString {
        QDir dir(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = dir.filePath("training/python");
            if (QFileInfo(candidate).isDir())
                return QFileInfo(candidate).absoluteFilePath();
            if (!dir.cdUp())
                break;
        }
        QDir cwd(QDir::currentPath());
        for (int i = 0; i < 10; ++i) {
            const QString candidate = cwd.filePath("training/python");
            if (QFileInfo(candidate).isDir())
                return QFileInfo(candidate).absoluteFilePath();
            if (!cwd.cdUp())
                break;
        }
        return QString();
    };

    desktop_app::workspace::ValidatorWorkspaceControls validatorWorkspaceControls;
    validatorWorkspaceControls.modelPath = validatorResolveAppRelative(onnxEdit->text().trimmed());
    validatorWorkspaceControls.metadataPath = validatorResolveAppRelative(metaEdit->text().trimmed());
    validatorWorkspaceControls.pythonExecutable =
        runtimeSettings.value("validator/pythonExecutable", "python").toString();
    validatorWorkspaceControls.datasetPath =
        runtimeSettings.value("validator/imageDataset", defaultWorkspacePaths.preparedDataset).toString();
    validatorWorkspaceControls.outputPath =
        runtimeSettings
            .value("validator/outputFolder",
                   QDir(defaultWorkspacePaths.runs)
                       .filePath("validation_gui_image_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")))
            .toString();
    validatorWorkspaceControls.trainerPythonPath = validatorTrainerPythonPath();
    validatorWorkspaceControls.imageValidationAction = imageValidationAction;
    auto validatorWorkspacePage = desktop_app::workspace::buildValidatorWorkspace(validatorWorkspaceControls);

    desktop_app::workspace::ReportsWorkspaceControls reportsWorkspaceControls;
    reportsWorkspaceControls.logPath = logPath;
    reportsWorkspaceControls.viewerOnly = viewerOnly;
    reportsWorkspaceControls.outputRoot = defaultWorkspacePaths.runs;
    reportsWorkspaceControls.showLogsAction = showLogsAction;
    reportsWorkspaceControls.showDiagnosticsAction = showDiagnosticsAction;
    reportsWorkspaceControls.openRunFolderAction = openRunFolderAction;
    reportsWorkspaceControls.outputRootEdit = outputEdit;
    auto reportsWorkspacePage = desktop_app::workspace::buildReportsWorkspace(reportsWorkspaceControls);

    desktop_app::workspace::SettingsWorkspaceControls settingsWorkspaceControls;
    settingsWorkspaceControls.outputRoot = defaultWorkspacePaths.runs;
    settingsWorkspaceControls.modelPath = defaultWorkspacePaths.models;
    settingsWorkspaceControls.metadataPath = metaEdit->text().trimmed();
    settingsWorkspaceControls.datasetsRoot = defaultWorkspacePaths.datasets;
    settingsWorkspaceControls.logPath = logPath;
    settingsWorkspaceControls.cameraSavePathEdit = savePathEdit;
    settingsWorkspaceControls.cameraPresetCombo = presetCombo;
    settingsWorkspaceControls.exposureSpin = exposureSpin;
    settingsWorkspaceControls.daqDeviceCombo = daqDeviceCombo;
    settingsWorkspaceControls.daqChannelEdit = daqChannelEdit;
    settingsWorkspaceControls.amplitudeSpin = amplitudeSpin;
    settingsWorkspaceControls.frequencySpin = freqSpin;
    settingsWorkspaceControls.durationSpin = durationSpin;
    settingsWorkspaceControls.delaySpin = delaySpin;
    settingsWorkspaceControls.logCheck = logCheck;
    settingsWorkspaceControls.outputRootEdit = outputEdit;
    settingsWorkspaceControls.trainerPythonEdit = trainerPythonEdit;
    settingsWorkspaceControls.trainerDatasetRootEdit = trainerDatasetEdit;
    settingsWorkspaceControls.operationDock = operationDock;
    settingsWorkspaceControls.operationalTabs = operationalTabs;
    settingsWorkspaceControls.analysisTab = leftAnalysisTab;
    settingsWorkspaceControls.devicesTab = leftDevicesTab;
    auto settingsWorkspacePage = desktop_app::workspace::buildSettingsWorkspace(settingsWorkspaceControls);

    auto workspaceStack = new QStackedWidget;
    nameWidget(workspaceStack, "OpenDssWorkspaceStack");
    workspaceStack->addWidget(liveWorkspacePage);
    workspaceStack->addWidget(modelWorkspacePage);
    workspaceStack->addWidget(datasetWorkspacePage);
    workspaceStack->addWidget(trainerWorkspacePage);
    workspaceStack->addWidget(validatorWorkspacePage);
    workspaceStack->addWidget(reportsWorkspacePage);
    workspaceStack->addWidget(settingsWorkspacePage);
    workspaceStack->setCurrentWidget(liveWorkspacePage);
    auto liveModelMenu = new QMenu(openLiveModelManagerBtn);
    liveModelMenu->addAction("Open Model workspace", [=]() { workspaceStack->setCurrentWidget(modelWorkspacePage); });
    openLiveModelManagerBtn->setMenu(liveModelMenu);
    QObject::connect(liveConfigureSettingsBtn, &QPushButton::clicked,
                     [=]() { workspaceStack->setCurrentWidget(settingsWorkspacePage); });

    auto headerProductLabel = new QLabel("OpenDSS");
    nameWidget(headerProductLabel, "OpenDssHeaderProductTitle");
    auto headerTitleLabel = new QLabel("/ Live View");
    nameWidget(headerTitleLabel, "OpenDssHeaderWorkspaceTitle");
    auto headerStatusText = new QLabel("Live View workspace");
    nameWidget(headerStatusText, "OpenDssHeaderStatusText");
    headerStatusText->setTextInteractionFlags(Qt::NoTextInteraction);
    headerStatusText->setProperty("statusChip", true);
    headerStatusText->setProperty("chipTone", "info");
    headerStatusText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerStatusText->setMinimumWidth(190);
    headerStatusText->setMaximumWidth(300);
    headerStatusText->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    auto headerCameraChip = new QLabel("Camera startup");
    auto headerModelChip = new QLabel("Model not loaded");
    auto headerDaqChip = new QLabel(!kDaqBuildEnabled ? "DAQ unavailable" : "DAQ unchecked");
    auto headerTriggerChip = new QLabel("Manual trigger blocked");
    nameWidget(headerCameraChip, "OpenDssHeaderCameraChip");
    nameWidget(headerModelChip, "OpenDssHeaderModelChip");
    nameWidget(headerDaqChip, "OpenDssHeaderDaqChip");
    nameWidget(headerTriggerChip, "OpenDssHeaderTriggerChip");
    for (auto* chip : {headerCameraChip, headerModelChip, headerDaqChip, headerTriggerChip}) {
        chip->setProperty("statusChip", true);
        chip->setTextInteractionFlags(Qt::NoTextInteraction);
        chip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        chip->setMinimumWidth(92);
        chip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    }
    headerCameraChip->setProperty("chipTone", "warn");
    headerModelChip->setProperty("chipTone", "warn");
    headerDaqChip->setProperty("chipTone", !kDaqBuildEnabled ? "disabled" : "warn");
    headerTriggerChip->setProperty("chipTone", "warn");
    auto shellMenuButton = new QToolButton;
    nameWidget(shellMenuButton, "OpenDssHeaderMenuButton");
    shellMenuButton->setProperty("headerIcon", true);
    shellMenuButton->setIcon(makeBrandIcon("menu", QColor("#FFFFFF"), QColor("#7DD3FC")));
    shellMenuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    shellMenuButton->setToolTip("Menu");
    shellMenuButton->setAccessibleName("OpenDssHeaderMenuButton");
    shellMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto shellMenu = new QMenu(shellMenuButton);
    for (auto* action : this->menuBar()->actions()) {
        shellMenu->addAction(action);
    }
    shellMenuButton->setMenu(shellMenu);
    auto diagnosticsHeaderButton = new QToolButton;
    nameWidget(diagnosticsHeaderButton, "OpenDssHeaderDiagnosticsButton");
    diagnosticsHeaderButton->setProperty("headerIcon", true);
    diagnosticsHeaderButton->setIcon(makeBrandIcon("info", QColor("#FFFFFF"), QColor("#7DD3FC")));
    diagnosticsHeaderButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    diagnosticsHeaderButton->setToolTip("Diagnostics");
    diagnosticsHeaderButton->setAccessibleName("OpenDssHeaderDiagnosticsButton");
    auto themeToggleButton = new QToolButton;
    nameWidget(themeToggleButton, "OpenDssHeaderThemeToggleButton");
    themeToggleButton->setCheckable(true);
    themeToggleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    themeToggleButton->setMinimumWidth(54);
    themeToggleButton->setAccessibleName("OpenDssHeaderThemeToggleButton");
    auto updateThemeToggleButton = [&]() {
        const bool lightTheme = currentThemeMode == desktop_app::theme::ThemeMode::Light;
        themeToggleButton->setChecked(lightTheme);
        themeToggleButton->setText(lightTheme ? "Dark" : "Light");
        themeToggleButton->setToolTip(lightTheme ? "Switch to dark mode" : "Switch to light mode");
    };
    updateThemeToggleButton();
    QObject::connect(themeToggleButton, &QToolButton::clicked, [&]() {
        currentThemeMode =
            themeToggleButton->isChecked() ? desktop_app::theme::ThemeMode::Light : desktop_app::theme::ThemeMode::Dark;
        runtimeSettings.setValue("shell/theme",
                                 currentThemeMode == desktop_app::theme::ThemeMode::Light ? "light" : "dark");
        applyShellTheme();
        updateThemeToggleButton();
    });
    auto shellHeader = new QFrame;
    nameWidget(shellHeader, "OpenDssHeader");
    shellHeader->setFrameShape(QFrame::NoFrame);
    auto shellHeaderLayout = new QHBoxLayout;
    shellHeaderLayout->setContentsMargins(12, 0, 12, 0);
    shellHeaderLayout->setSpacing(8);
    auto headerLogoLabel = new QLabel;
    nameWidget(headerLogoLabel, "OpenDssHeaderLogo");
    headerLogoLabel->setPixmap(QPixmap(":/branding/opendss-icon-512.png")
                                   .scaled(QSize(24, 24), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    headerLogoLabel->setFixedSize(26, 26);
    headerLogoLabel->setAlignment(Qt::AlignCenter);
    shellHeaderLayout->addWidget(headerLogoLabel);
    shellHeaderLayout->addWidget(headerProductLabel);
    shellHeaderLayout->addWidget(headerTitleLabel);
    shellHeaderLayout->addWidget(headerStatusText);
    shellHeaderLayout->addWidget(headerCameraChip);
    shellHeaderLayout->addWidget(headerModelChip);
    shellHeaderLayout->addWidget(headerDaqChip);
    shellHeaderLayout->addWidget(headerTriggerChip);
    shellHeaderLayout->addStretch(1);
    shellHeaderLayout->addWidget(themeToggleButton);
    shellHeaderLayout->addWidget(shellMenuButton);
    shellHeaderLayout->addWidget(diagnosticsHeaderButton);
    shellHeader->setLayout(shellHeaderLayout);

    auto shellStatusStrip = new QFrame;
    nameWidget(shellStatusStrip, "OpenDssStatusStrip");
    shellStatusStrip->setFrameShape(QFrame::NoFrame);
    auto shellStatusLayout = new QHBoxLayout;
    shellStatusLayout->setContentsMargins(12, 0, 12, 0);
    shellStatusLayout->setSpacing(16);
    auto shellRuntimeStatus = new QLabel("Run: idle");
    auto shellCameraStatus = new QLabel("Camera: startup pending");
    auto shellModelStatus = new QLabel("Model: not loaded");
    auto shellDaqStatus = new QLabel(initialDaqStatusText);
    nameWidget(shellRuntimeStatus, "OpenDssShellRunStatusLabel");
    nameWidget(shellCameraStatus, "OpenDssShellCameraStatusLabel");
    nameWidget(shellModelStatus, "OpenDssShellModelStatusLabel");
    nameWidget(shellDaqStatus, "OpenDssShellDaqStatusLabel");
    for (auto* item : {shellRuntimeStatus, shellCameraStatus, shellModelStatus, shellDaqStatus}) {
        item->setTextInteractionFlags(Qt::NoTextInteraction);
        shellStatusLayout->addWidget(item);
    }
    shellStatusLayout->addStretch(1);
    auto shellDiagnosticsStatus = new QLabel("Diagnostics");
    nameWidget(shellDiagnosticsStatus, "OpenDssShellDiagnosticsStatusLabel");
    shellDiagnosticsStatus->setTextInteractionFlags(Qt::NoTextInteraction);
    shellStatusLayout->addWidget(shellDiagnosticsStatus);
    auto shellLogPathStatus = new QLabel("Log: " + QFileInfo(logPath).fileName());
    nameWidget(shellLogPathStatus, "OpenDssShellLogPathLabel");
    shellLogPathStatus->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    shellLogPathStatus->setToolTip(logPath);
    shellStatusLayout->addWidget(shellLogPathStatus);
    shellStatusStrip->setLayout(shellStatusLayout);

    auto navRail = new QFrame;
    nameWidget(navRail, "OpenDssNavigationRail");
    navRail->setFrameShape(QFrame::NoFrame);
    navRail->setMinimumWidth(56);
    navRail->setMaximumWidth(56);
    auto navLayout = new QVBoxLayout;
    navLayout->setContentsMargins(8, 12, 8, 12);
    navLayout->setSpacing(4);
    auto railLogo = new QLabel("DS");
    nameWidget(railLogo, "OpenDssRailLogo");
    railLogo->setAlignment(Qt::AlignCenter);
    railLogo->setFixedSize(36, 36);
    railLogo->setText(QString());
    railLogo->setPixmap(QPixmap(":/branding/opendss-icon-512.png")
                            .scaled(QSize(26, 26), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    navLayout->addWidget(railLogo, 0, Qt::AlignHCenter);
    navLayout->addSpacing(8);
    auto navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    auto addNavButton = [&](const QString& text, const QString& iconKey, QWidget* page, const char* objectName) {
        auto* button = new QPushButton;
        nameWidget(button, objectName);
        button->setCheckable(true);
        button->setProperty("railButton", true);
        button->setIcon(makeBrandIcon(iconKey, QColor("#FFFFFF"), QColor("#14B8A6")));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(text);
        button->setAccessibleName(text);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        navGroup->addButton(button);
        navLayout->addWidget(button, 0, Qt::AlignHCenter);
        QObject::connect(button, &QPushButton::clicked, [=]() {
            workspaceStack->setCurrentWidget(page);
            headerTitleLabel->setText("/ " + text);
            headerStatusText->setText(text + " workspace");
        });
        return button;
    };
    auto liveNavButton = addNavButton("Live View", "play", liveWorkspacePage, "NavLiveButton");
    auto modelNavButton = addNavButton("Model", "model", modelWorkspacePage, "NavModelButton");
    auto datasetNavButton = addNavButton("Dataset", "dataset", datasetWorkspacePage, "NavDatasetButton");
    auto trainerNavButton = addNavButton("Trainer", "trainer", trainerWorkspacePage, "NavTrainerButton");
    auto validatorNavButton = addNavButton("Validator", "validator", validatorWorkspacePage, "NavValidatorButton");
    auto reportsNavButton = addNavButton("Reports", "reports", reportsWorkspacePage, "NavReportsButton");
    navLayout->addStretch(1);
    auto settingsNavButton = addNavButton("Settings", "settings", settingsWorkspacePage, "NavSettingsButton");
    auto wireHeaderChipNavigation = [&](QLabel* chip, QPushButton* destination, const QString& tooltip) {
        chip->setCursor(Qt::PointingHandCursor);
        chip->setToolTip(tooltip);
        chip->installEventFilter(new HeaderChipClickFilter(
            [destination]() {
                if (destination)
                    destination->click();
            },
            chip));
    };
    wireHeaderChipNavigation(headerStatusText, liveNavButton, "Open Live View");
    wireHeaderChipNavigation(headerCameraChip, liveNavButton, "Open Live View");
    wireHeaderChipNavigation(headerModelChip, modelNavButton, "Open Model");
    wireHeaderChipNavigation(headerDaqChip, settingsNavButton, "Open Settings hardware");
    wireHeaderChipNavigation(headerTriggerChip, liveNavButton, "Open Live View");
    liveNavButton->setChecked(true);
    if (options.initialWorkspace == "model") {
        workspaceStack->setCurrentWidget(modelWorkspacePage);
        headerTitleLabel->setText("/ Model");
        headerStatusText->setText("Model workspace");
        modelNavButton->setChecked(true);
    } else if (options.initialWorkspace == "dataset") {
        workspaceStack->setCurrentWidget(datasetWorkspacePage);
        headerTitleLabel->setText("/ Dataset");
        headerStatusText->setText("Dataset workspace");
        datasetNavButton->setChecked(true);
    } else if (options.initialWorkspace == "trainer") {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
    } else if (options.initialWorkspace == "validator") {
        workspaceStack->setCurrentWidget(validatorWorkspacePage);
        headerTitleLabel->setText("/ Validator");
        headerStatusText->setText("Validator workspace");
        validatorNavButton->setChecked(true);
    } else if (options.initialWorkspace == "reports") {
        workspaceStack->setCurrentWidget(reportsWorkspacePage);
        headerTitleLabel->setText("/ Reports");
        headerStatusText->setText("Reports workspace");
        reportsNavButton->setChecked(true);
    } else if (options.initialWorkspace == "settings") {
        workspaceStack->setCurrentWidget(settingsWorkspacePage);
        headerTitleLabel->setText("/ Settings");
        headerStatusText->setText("Settings workspace");
        settingsNavButton->setChecked(true);
    }
    navRail->setLayout(navLayout);

    auto shellContent = new QWidget;
    nameWidget(shellContent, "OpenDssShellContent");
    auto shellContentLayout = new QVBoxLayout;
    shellContentLayout->setContentsMargins(0, 0, 0, 0);
    shellContentLayout->setSpacing(0);
    shellContentLayout->addWidget(shellHeader);
    shellContentLayout->addWidget(workspaceStack, 1);
    shellContentLayout->addWidget(shellStatusStrip);
    shellContent->setLayout(shellContentLayout);

    auto centralWidget = new QWidget;
    nameWidget(centralWidget, "CentralWidget");
    auto shellLayout = new QHBoxLayout;
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(navRail);
    shellLayout->addWidget(shellContent, 1);
    centralWidget->setLayout(shellLayout);
    this->setCentralWidget(centralWidget);

    QObject::connect(trainerDockProxyButton, &QPushButton::clicked, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    auto logDock = new QDockWidget("Logs", this);
    logDock->setObjectName("LogsDock");
    auto logDockText = new QPlainTextEdit;
    nameWidget(logDockText, "LogsTextEdit");
    logDockText->setObjectName("LogsText");
    logDockText->setReadOnly(true);
    logDockText->setPlainText("Session log: " + logPath +
                              "\n\nLive log streaming remains handled by session_log.txt in this shell step.");
    logDock->setWidget(logDockText);
    logDock->setMinimumHeight(36);
    this->addDockWidget(Qt::BottomDockWidgetArea, logDock);
    this->resizeDocks({logDock}, {72}, Qt::Vertical);
    logDock->hide();

    auto diagnosticsDock = new QDockWidget("System Diagnostics", this);
    diagnosticsDock->setObjectName("SystemDiagnosticsDock");
    auto diagnosticsLabel =
        new QLabel("Application: shell loaded\n"
                   "Camera/DCAM: checked by existing startup path\n"
                   "Model: loaded through existing Pipeline controls\n"
                   "DAQ: configured in Devices > DAQ / Trigger\n"
                   "Python trainer: readiness checks available in Trainer tab\n"
                   "Image validator: launch available in Validation > Image Validation\n"
                   "Training launch, runner-wrapped sequence validation, and Model Promotion: disabled placeholders");
    nameWidget(diagnosticsLabel, "SystemDiagnosticsLabel");
    diagnosticsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    diagnosticsLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    diagnosticsLabel->setWordWrap(true);
    diagnosticsDock->setWidget(diagnosticsLabel);
    this->addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
    diagnosticsDock->hide();
    QObject::connect(diagnosticsHeaderButton, &QToolButton::clicked, diagnosticsDock, &QDockWidget::show);

    auto cameraStatusItem = new QLabel("Camera: startup pending");
    auto modelStatusItem = new QLabel("Model: not loaded");
    auto daqStatusItem = new QLabel(initialDaqStatusText);
    auto pythonStatusItem = new QLabel("Python: not configured");
    auto runStatusItem = new QLabel("Run: idle");
    nameWidget(cameraStatusItem, "CameraStatusBarLabel");
    nameWidget(modelStatusItem, "ModelStatusBarLabel");
    nameWidget(daqStatusItem, "DaqStatusBarLabel");
    nameWidget(pythonStatusItem, "PythonStatusBarLabel");
    nameWidget(runStatusItem, "RunStatusBarLabel");
    nameWidget(this->statusBar(), "StatusBar");
    for (auto* item : {cameraStatusItem, modelStatusItem, daqStatusItem, pythonStatusItem, runStatusItem}) {
        item->setFrameStyle(QFrame::NoFrame);
        this->statusBar()->addPermanentWidget(item);
    }
    this->statusBar()->showMessage("Shell ready");
    this->menuBar()->hide();
    this->statusBar()->hide();

    auto setHeaderChipText = [](QLabel* label, const QString& text, int maximumWidth = 220) {
        if (!label)
            return;
        const int horizontalPadding = 28;
        const int textWidth = qMax(24, maximumWidth - horizontalPadding);
        const QString visibleText = label->fontMetrics().elidedText(text, Qt::ElideRight, textWidth);
        const int targetWidth =
            qMin(maximumWidth, label->fontMetrics().horizontalAdvance(visibleText) + horizontalPadding);
        label->setMinimumWidth(qMax(72, targetWidth));
        label->setToolTip(text);
        label->setText(visibleText);
    };
    auto statusValue = [](QString text, const QString& prefix) {
        text = text.simplified();
        const QString marker = prefix + ":";
        if (text.startsWith(marker, Qt::CaseInsensitive)) {
            text = text.mid(marker.size()).trimmed();
        }
        return text;
    };
    auto titleCaseStatus = [](QString text) {
        text = text.simplified();
        if (text.isEmpty())
            return text;
        text[0] = text[0].toUpper();
        return text;
    };
    auto runHeaderText = [&](const QString& runText, const QString& statusText) {
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("live view"))
            return QStringLiteral("Live View");
        if (run.contains("capture"))
            return QStringLiteral("Camera capture");
        if (run.contains("viewer-only"))
            return QStringLiteral("Camera viewer-only");
        if (run == QStringLiteral("idle"))
            return QStringLiteral("Idle");
        const QString simplifiedStatus = statusText.simplified();
        return simplifiedStatus.isEmpty() ? QStringLiteral("Idle") : simplifiedStatus;
    };
    auto cameraHeaderText = [&](const QString& cameraText, const QString& runText) {
        const QString camera = statusValue(cameraText, "Camera").toLower();
        const QString run = statusValue(runText, "Run").toLower();
        if (run.contains("viewer-only") || camera.contains("unavailable"))
            return QStringLiteral("Camera viewer-only");
        if (camera.contains("acquiring"))
            return QStringLiteral("Camera acquiring");
        if (camera.contains("connected"))
            return QStringLiteral("Camera connected");
        if (camera.contains("error"))
            return QStringLiteral("Camera error");
        return QStringLiteral("Camera startup");
    };
    auto modelHeaderText = [&](const QString& modelText) {
        const QString model = statusValue(modelText, "Model").toLower();
        if (model.contains("loaded"))
            return QStringLiteral("Model SqueezeNet");
        if (model.contains("error"))
            return QStringLiteral("Model error");
        return QStringLiteral("Model not loaded");
    };
    auto daqHeaderText = [&](const QString& daqText) {
        const QString daq = statusValue(daqText, "DAQ").toLower();
        if (daq.contains("disabled") || daq.contains("unavailable"))
            return QStringLiteral("DAQ unavailable");
        if (daq.contains("available"))
            return QStringLiteral("DAQ available");
        return QStringLiteral("DAQ unchecked");
    };
    auto triggerHeaderText = [&](const QString& triggerText) {
        const QString trigger = triggerText.simplified().toLower();
        if (trigger.contains("queued"))
            return QStringLiteral("Trigger queued");
        if (trigger.contains("sent"))
            return QStringLiteral("Trigger sent");
        if (trigger.contains("failed"))
            return QStringLiteral("Trigger failed");
        if (appState.daqDisabled)
            return QStringLiteral("DAQ disabled");
        if (liveForceTriggerBtn->isEnabled())
            return QStringLiteral("Manual trigger ready");
        return QStringLiteral("Manual trigger blocked");
    };

    auto shellStatusMirrorTimer = new QTimer(this);
    shellStatusMirrorTimer->setInterval(500);
    QObject::connect(shellStatusMirrorTimer, &QTimer::timeout, [=, &appState]() {
        shellRuntimeStatus->setText(runStatusItem->text());
        shellCameraStatus->setText(cameraStatusItem->text());
        shellModelStatus->setText(modelStatusItem->text());
        shellDaqStatus->setText(daqStatusItem->text());
        appState.cameraStreaming = cameraStatusItem->text().contains("acquiring", Qt::CaseInsensitive);
        appState.daqStatusText = daqStatusItem->text();
        const bool daqTextUnavailable = daqStatusItem->text().contains("unavailable", Qt::CaseInsensitive);
        appState.daqAvailable = !daqTextUnavailable && daqStatusItem->text().contains("available", Qt::CaseInsensitive);
        appState.daqDisabled = daqStatusItem->text().contains("disabled", Qt::CaseInsensitive);
        appState.daqFault = daqTextUnavailable;
        updateForceTriggerState();
        const int headerWidth = shellHeader->width();
        const bool compactHeader = headerWidth > 0 && headerWidth < 1380;
        const bool narrowHeader = headerWidth > 0 && headerWidth < 1120;
        headerCameraChip->setVisible(!compactHeader);
        headerTriggerChip->setVisible(!compactHeader);
        headerModelChip->setVisible(!narrowHeader);
        headerDaqChip->setVisible(!narrowHeader);
        setHeaderChipText(headerCameraChip, cameraHeaderText(cameraStatusItem->text(), runStatusItem->text()), 170);
        setHeaderChipText(headerModelChip, modelHeaderText(modelStatusItem->text()), 160);
        setHeaderChipText(headerDaqChip, daqHeaderText(daqStatusItem->text()), 145);
        const QString triggerChipText = triggerHeaderText(statusLabel->text());
        setHeaderChipText(headerTriggerChip, triggerChipText, 170);
        shellHeaderLayout->invalidate();
        headerDaqChip->setProperty(
            "chipTone", (daqStatusItem->text().contains("disabled") || daqStatusItem->text().contains("unavailable"))
                            ? "disabled"
                            : (daqStatusItem->text().contains("available") ? "running" : "warn"));
        headerModelChip->setProperty("chipTone", modelStatusItem->text().contains("loaded")
                                                     ? "running"
                                                     : (modelStatusItem->text().contains("error") ? "error" : "warn"));
        headerCameraChip->setProperty(
            "chipTone", cameraStatusItem->text().contains("connected") || cameraStatusItem->text().contains("acquiring")
                            ? "running"
                            : (cameraStatusItem->text().contains("error") ? "error" : "warn"));
        headerTriggerChip->setProperty(
            "chipTone", triggerChipText.contains("sent", Qt::CaseInsensitive) ||
                                triggerChipText.contains("ready", Qt::CaseInsensitive)
                            ? "running"
                            : (triggerChipText.contains("failed", Qt::CaseInsensitive)
                                   ? "error"
                                   : (triggerChipText.contains("disabled", Qt::CaseInsensitive) ? "disabled" : "warn")));
        headerCameraChip->style()->unpolish(headerCameraChip);
        headerCameraChip->style()->polish(headerCameraChip);
        headerModelChip->style()->unpolish(headerModelChip);
        headerModelChip->style()->polish(headerModelChip);
        headerDaqChip->style()->unpolish(headerDaqChip);
        headerDaqChip->style()->polish(headerDaqChip);
        headerTriggerChip->style()->unpolish(headerTriggerChip);
        headerTriggerChip->style()->polish(headerTriggerChip);

        const QRegularExpression intRe("(\\d+)");
        auto firstNumberAfter = [&](const QString& text, const QString& marker, const QString& fallback) {
            const int markerIndex = text.indexOf(marker);
            if (markerIndex < 0)
                return fallback;
            auto match = intRe.match(text, markerIndex + marker.size());
            return match.hasMatch() ? match.captured(1) : fallback;
        };
        eventsMetricLabel->setText(firstNumberAfter(statsEventsLabel->text(), "Events:", "0"));
        classifiedHitMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Classified Hit:", "0"));
        classifiedWasteMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Classified Waste:", "0"));
        wentToHitMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Went to Hit:", "0"));
        wentToWasteMetricLabel->setText(firstNumberAfter(statsHitLabel->text(), "Went to Waste:", "0"));
        trigMetricLabel->setText(pipelineEnableCheck->isChecked() ? "live" : "--");
        lastDecisionValue->setText(statsLastLabel->text().contains("--") ? "--" : statsLastLabel->text().simplified());
    });
    shellStatusMirrorTimer->start();

    QObject::connect(fitAction, &QAction::triggered, [&]() {
        imageView->fitToView();
        cameraImageView->fitToView();
        this->statusBar()->showMessage("Preview images fit to view");
    });
    QObject::connect(oneToOneAction, &QAction::triggered, [=]() {
        imageView->resetScale();
        cameraImageView->resetScale();
    });
    QObject::connect(zoomInAction, &QAction::triggered, [=]() {
        imageView->zoomBySteps(1);
        cameraImageView->zoomBySteps(1);
    });
    QObject::connect(zoomOutAction, &QAction::triggered, [=]() {
        imageView->zoomBySteps(-1);
        cameraImageView->zoomBySteps(-1);
    });
    QObject::connect(leftLoadBtn, &QPushButton::clicked, viewerBtn, &QPushButton::click);
    QObject::connect(leftReconnectBtn, &QPushButton::clicked, reconnectBtn, &QPushButton::click);
    QObject::connect(openViewerAction, &QAction::triggered, viewerBtn, &QPushButton::click);
    QObject::connect(reconnectAction, &QAction::triggered, reconnectBtn, &QPushButton::click);
    QObject::connect(startPreviewAction, &QAction::triggered, startBtn, &QPushButton::click);
    QObject::connect(stopPreviewAction, &QAction::triggered, [&]() {
        if (appState.cameraStreaming) {
            startBtn->click();
        }
    });
    QObject::connect(captureStillAction, &QAction::triggered, captureBtn, &QPushButton::click);
    QObject::connect(startSortingAction, &QAction::triggered, pipelineStartBtn, &QPushButton::click);
    QObject::connect(stopSortingAction, &QAction::triggered, pipelineStopBtn, &QPushButton::click);
    QObject::connect(manualTriggerAction, &QAction::triggered, liveForceTriggerBtn, &QPushButton::click);
    QObject::connect(liveSnapshotBtn, &QPushButton::clicked, captureBtn, &QPushButton::click);
    QObject::connect(liveDetectorTuningBtn, &QPushButton::clicked, [&]() {
        liveDetectorDrawerOverlay->setVisible(!liveDetectorDrawerOverlay->isVisible());
        if (liveDetectorDrawerOverlay->isVisible())
            liveDetectorDrawerOverlay->raise();
    });
    QObject::connect(liveDetectorClose, &QToolButton::clicked, liveDetectorDrawerOverlay, &QWidget::hide);
    ReportsWorkspaceController::Dependencies reportsWorkspaceDeps;
    reportsWorkspaceDeps.openRunFolderAction = openRunFolderAction;
    reportsWorkspaceDeps.openOutputAction = openOutputAction;
    reportsWorkspaceDeps.liveOpenRunButton = liveOpenRunBtn;
    reportsWorkspaceDeps.showLogsAction = showLogsAction;
    reportsWorkspaceDeps.showDiagnosticsAction = showDiagnosticsAction;
    reportsWorkspaceDeps.systemDiagnosticsAction = systemDiagnosticsAction;
    reportsWorkspaceDeps.logDock = logDock;
    reportsWorkspaceDeps.diagnosticsDock = diagnosticsDock;
    reportsWorkspaceDeps.statusLabel = statusLabel;
    reportsWorkspaceDeps.statusBar = this->statusBar();
    ReportsWorkspaceController reportsWorkspaceController(reportsWorkspaceDeps, this);
    QObject::connect(resetLayoutAction, &QAction::triggered, [&]() {
        this->addDockWidget(Qt::LeftDockWidgetArea, operationDock);
        this->addDockWidget(Qt::BottomDockWidgetArea, logDock);
        this->addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
        operationDock->show();
        logDock->show();
        diagnosticsDock->hide();
        if (imageSubWindow) {
            imageSubWindow->show();
            imageSubWindow->resize(760, 620);
            imageSubWindow->move(16, 16);
        }
    });
    QObject::connect(exitAction, &QAction::triggered, this, &QWidget::close);
    QObject::connect(aboutAction, &QAction::triggered, [&]() {
        QMessageBox::about(this, "About Open Visual Droplet Sorter Suite",
                           "Open Visual Droplet Sorter Suite\n\n"
                           "Qt shell preview around the existing Hamamatsu live-view and droplet pipeline controls.\n\n"
                           "Image validation can be launched through the external Python validator. Trainer launch, "
                           "runner-wrapped sequence validation, and model promotion remain disabled placeholders.");
    });

    // Logging helper
    auto logLine = [&](const QString& msg) {
        if (!logCheck->isChecked())
            return;
        logMessage(msg);
    };

    auto buildRunOutputDir = [&](const QString& prefix) -> QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty())
            base = QCoreApplication::applicationDirPath();
        QDir baseDir(base);
        QString leaf = baseDir.dirName();
        if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
            baseDir.cdUp();
        }
        baseDir.mkpath(".");
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString runName = QString("%1_%2").arg(prefix, stamp);
        QString runDir = baseDir.filePath(runName);
        baseDir.mkpath(runName);
        return runDir;
    };
    reportsWorkspaceController.refreshOpenRunAvailability();

    auto buildDatasetBuilderDir = [&](QString* datasetIdOut) -> QString {
        QString base = outputEdit->text().trimmed();
        if (base.isEmpty())
            base = QCoreApplication::applicationDirPath();
        QDir baseDir(base);
        QString leaf = baseDir.dirName();
        if (leaf.startsWith("sequence_") || leaf.startsWith("live_") || leaf.startsWith("test_")) {
            baseDir.cdUp();
        }
        QString shortId = QUuid::createUuid().toString(QUuid::Id128).left(6).toLower();
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString datasetId = QString("builder_%1_live_%2").arg(stamp, shortId);
        if (datasetIdOut)
            *datasetIdOut = datasetId;
        QDir root(baseDir.filePath("datasets/builder"));
        root.mkpath(".");
        root.mkpath(datasetId);
        return root.filePath(datasetId);
    };

    auto pickExistingPath = [](const QStringList& candidates) -> QString {
        for (const auto& c : candidates) {
            if (QFileInfo::exists(c))
                return c;
        }
        return candidates.isEmpty() ? QString() : candidates.first();
    };

    QString appDir = QCoreApplication::applicationDirPath();
    auto findModelUpwards = [&](const QString& filename) -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 6; ++i) {
            QString candidate = dir.filePath("models/" + filename);
            if (QFileInfo::exists(candidate))
                return candidate;
            if (!dir.cdUp())
                break;
        }
        const QString projectRoot = findProjectRootFromApp();
        if (!projectRoot.isEmpty()) {
            const QString promotedArtifact = runtimeModelArtifactPath(projectRoot, "app/runtime/models/" + filename);
            if (!promotedArtifact.isEmpty())
                return promotedArtifact;
        }
        return QString();
    };
    auto findOutputsRootUpwards = [&]() -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 8; ++i) {
            QString outputsDir = dir.filePath("outputs");
            if (QFileInfo(outputsDir).isDir()) {
                return QDir(outputsDir).filePath("pipeline_output");
            }
            if (!dir.cdUp())
                break;
        }
        return QString();
    };
    auto findProjectRootUpwards = [&]() -> QString {
        QDir dir(appDir);
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(dir.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return dir.absolutePath();
            }
            if (!dir.cdUp())
                break;
        }
        QDir cwd(QDir::currentPath());
        for (int i = 0; i < 10; ++i) {
            if (QFileInfo(cwd.filePath("training/python/droplet_trainer/__main__.py")).exists()) {
                return cwd.absolutePath();
            }
            if (!cwd.cdUp())
                break;
        }
        return QString();
    };
    QString projectRoot = findProjectRootUpwards();
    auto projectPath = [&](const QString& rel) -> QString {
        return projectRoot.isEmpty() ? QString() : QDir(projectRoot).absoluteFilePath(rel);
    };
    QString defaultTrainerOutput = defaultWorkspacePaths.runs;
    QString defaultTrainerDataset = defaultWorkspacePaths.preparedDataset;
    auto* datasetController = new DatasetWorkspaceController(
        DatasetWorkspaceController::Dependencies{
            this,
            defaultTrainerDataset,
            defaultTrainerOutput,
            trainerPythonEdit,
            trainerDatasetEdit,
            trainerOutputEdit,
            trainerPythonBrowseBtn,
            trainerDatasetBrowseBtn,
            trainerOutputBrowseBtn,
            trainerEnvCheckBtn,
            trainerConfigurePathBtn,
            trainerCancelBtn,
            trainerStartTrainingBtn,
            trainerDryRunBtn,
            trainerStatusLabel,
            trainerResultText,
            trainerProgressBar,
            trainerArchitectureCombo,
            trainerPretrainedImageNetBtn,
            trainerPretrainedNoneBtn,
            trainerEpochsSpin,
            trainerBatchSpin,
            trainerLrSpin,
            trainerFlipCheck,
            trainerRotationCheck,
            trainerColorJitterCheck,
            trainerRandomCropCheck,
            trainerSchedulerCombo,
            datasetOpenAction,
            datasetBuildAction,
            datasetLabelDatasetAction,
        },
        this);
    auto openDatasetLabelerPath = [datasetController](const QString& preferredPath) {
        datasetController->openDatasetLabelerPath(preferredPath);
    };
    QProcess* trainerProcess = nullptr;
    bool trainerCommandWasTraining = false;
    bool trainerCommandWasDryRun = false;
    auto appendTrainerLog = [datasetController](const QString& text) { datasetController->appendTrainerLog(text); };
    auto trainerCommandPreview = [datasetController](const QString& program, const QStringList& args) {
        return datasetController->trainerCommandPreview(program, args);
    };
    auto saveTrainerSettings = [datasetController]() { datasetController->saveTrainerSettings(); };
    auto setTrainerBusy = [datasetController, &trainerCommandWasTraining](bool busy) {
        datasetController->setTrainerBusy(busy, trainerCommandWasTraining);
    };
    auto trainerTrainArgs = [datasetController](bool dryRun) { return datasetController->trainerTrainArgs(dryRun); };
    auto startTrainerCommand = [&](const QString& label, const QStringList& args, bool isTraining, bool isDryRun) {
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            trainerStatusLabel->setText("A trainer command is already running.");
            return;
        }
        QString python = trainerPythonEdit->text().trimmed();
        if (python.isEmpty()) {
            trainerStatusLabel->setText("Select a Python executable before running trainer commands.");
            return;
        }
        if ((isTraining || isDryRun) && (trainerDatasetEdit->text().trimmed().isEmpty() ||
                                         trainerOutputEdit->text().trimmed().isEmpty() || args.contains(QString()))) {
            trainerStatusLabel->setText(
                "Dataset, output directory, and generated config are required before training.");
            return;
        }
        saveTrainerSettings();
        trainerCommandWasTraining = isTraining;
        trainerCommandWasDryRun = isDryRun;
        trainerResultText->clear();
        appendTrainerLog(QString("Running %1\n%2\n\n").arg(label, trainerCommandPreview(python, args)));
        trainerStatusLabel->setText(QString("%1 running...").arg(label));
        pythonStatusItem->setText("Python: checking");
        setTrainerBusy(true);

        auto* process = new QProcess(this);
        trainerProcess = process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString trainingPython = projectPath("training/python");
        if (QFileInfo(trainingPython).isDir()) {
            QString existing = env.value("PYTHONPATH");
            env.insert("PYTHONPATH",
                       existing.isEmpty() ? trainingPython : trainingPython + QDir::listSeparator() + existing);
        }
        process->setProcessEnvironment(env);
        if (!projectRoot.isEmpty()) {
            process->setWorkingDirectory(projectRoot);
        }
        QObject::connect(process, &QProcess::readyReadStandardOutput, this, [&, process]() {
            if (trainerProcess != process)
                return;
            const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
            appendTrainerLog(chunk);
            int progressIndex = chunk.indexOf("\"percent\"");
            if (progressIndex >= 0) {
                QRegularExpression rx("\"percent\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
                auto match = rx.match(chunk, progressIndex);
                if (match.hasMatch()) {
                    trainerProgressBar->setRange(0, 100);
                    trainerProgressBar->setValue(qBound(0, static_cast<int>(match.captured(1).toDouble()), 100));
                    trainerProgressBar->setFormat(QString("%1%").arg(trainerProgressBar->value()));
                }
            }
        });
        QObject::connect(process, &QProcess::readyReadStandardError, this, [&, process]() {
            if (trainerProcess != process)
                return;
            appendTrainerLog(QString::fromLocal8Bit(process->readAllStandardError()));
        });
        QObject::connect(
            process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [&, process](int exitCode, QProcess::ExitStatus exitStatus) {
                if (trainerProcess != process)
                    return;
                const bool crashed = exitStatus == QProcess::CrashExit;
                appendTrainerLog(
                    QString("\nProcess finished: exit=%1%2\n").arg(exitCode).arg(crashed ? " crashed" : ""));
                const bool ok = exitCode == 0 && !crashed;
                if (trainerCommandWasDryRun) {
                    trainerStatusLabel->setText(ok ? "Dry run completed." : "Dry run failed. Review log output.");
                } else if (trainerCommandWasTraining) {
                    trainerStatusLabel->setText(ok ? "Training completed." : "Training failed. Review log output.");
                } else {
                    trainerStatusLabel->setText(ok ? "Python environment validated."
                                                   : "Environment validation failed.");
                }
                pythonStatusItem->setText(ok ? "Python: ready" : "Python: issue");
                setTrainerBusy(false);
                trainerProcess = nullptr;
                process->deleteLater();
            });
        QObject::connect(process, &QProcess::errorOccurred, this, [&, process](QProcess::ProcessError error) {
            if (trainerProcess != process)
                return;
            Q_UNUSED(error);
            trainerStatusLabel->setText("Failed to start trainer subprocess.");
            appendTrainerLog("Process error: " + process->errorString() + "\n");
            pythonStatusItem->setText("Python: start failed");
            setTrainerBusy(false);
            trainerProcess = nullptr;
            process->deleteLater();
        });
        process->start(python, args);
    };
    QObject::connect(trainerEnvCheckBtn, &QPushButton::clicked, [&]() {
        startTrainerCommand("Environment validation",
                            {"-m", "droplet_trainer", "env-check", "--device", "cpu", "--require-training",
                             "--require-onnx", "--json"},
                            false, false);
    });
    QObject::connect(trainerDryRunBtn, &QPushButton::clicked,
                     [&]() { startTrainerCommand("Training dry run", trainerTrainArgs(true), false, true); });
    QObject::connect(trainerStartTrainingBtn, &QPushButton::clicked,
                     [&]() { startTrainerCommand("Training", trainerTrainArgs(false), true, false); });
    QObject::connect(trainerCancelBtn, &QPushButton::clicked, [&]() {
        if (!trainerProcess || trainerProcess->state() == QProcess::NotRunning)
            return;
        trainerStatusLabel->setText("Canceling trainer subprocess...");
        trainerProcess->terminate();
        QPointer<QProcess> processPtr(trainerProcess);
        QTimer::singleShot(3000, this, [processPtr]() {
            if (!processPtr.isNull() && processPtr->state() != QProcess::NotRunning) {
                processPtr->kill();
            }
        });
    });
    QObject::connect(datasetReadinessAction, &QAction::triggered, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerDryRunBtn->setFocus();
    });
    QObject::connect(trainerConfigurePathBtn, &QPushButton::clicked, [&]() {
        workspaceStack->setCurrentWidget(settingsWorkspacePage);
        headerTitleLabel->setText("/ Settings");
        headerStatusText->setText("Settings workspace");
        settingsNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    QObject::connect(trainingEnvironmentSettingsAction, &QAction::triggered, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerPythonEdit->setFocus();
    });
    QObject::connect(trainingValidateEnvironmentAction, &QAction::triggered, [&]() {
        workspaceStack->setCurrentWidget(trainerWorkspacePage);
        headerTitleLabel->setText("/ Trainer");
        headerStatusText->setText("Trainer workspace");
        trainerNavButton->setChecked(true);
        trainerEnvCheckBtn->click();
    });

    QString defaultOnnxRel = "../../../models/pre_binary_promotion_backup.onnx";
    QString defaultMetaRel = "../../../models/pre_binary_promotion_backup_metadata.json";
    QString defaultOnnxAbs = QDir(appDir).absoluteFilePath(defaultOnnxRel);
    QString defaultMetaAbs = QDir(appDir).absoluteFilePath(defaultMetaRel);
    QString defaultOnnxFromProject = findModelUpwards("pre_binary_promotion_backup.onnx");
    QString defaultMetaFromProject = findModelUpwards("pre_binary_promotion_backup_metadata.json");
    QStringList onnxCandidates = {defaultWorkspacePaths.activeModel,
                                  defaultOnnxFromProject,
                                  defaultOnnxAbs,
                                  appDir + "/pre_binary_promotion_backup.onnx",
                                  appDir + "/models/pre_binary_promotion_backup.onnx",
                                  appDir + "/../models/pre_binary_promotion_backup.onnx",
                                  appDir + "/../../models/pre_binary_promotion_backup.onnx"};
    QStringList metaCandidates = {defaultWorkspacePaths.activeMetadata,
                                  defaultMetaFromProject,
                                  defaultMetaAbs,
                                  appDir + "/pre_binary_promotion_backup_metadata.json",
                                  appDir + "/models/pre_binary_promotion_backup_metadata.json",
                                  appDir + "/../models/pre_binary_promotion_backup_metadata.json",
                                  appDir + "/../../models/pre_binary_promotion_backup_metadata.json"};
    QString onnxPicked = pickExistingPath(onnxCandidates);
    if (onnxPicked.isEmpty()) {
        onnxPicked = defaultOnnxRel;
    } else {
        onnxPicked = QDir(appDir).relativeFilePath(onnxPicked);
    }
    QString metaPicked = pickExistingPath(metaCandidates);
    if (metaPicked.isEmpty()) {
        metaPicked = defaultMetaRel;
    } else {
        metaPicked = QDir(appDir).relativeFilePath(metaPicked);
    }
    onnxEdit->setText(onnxPicked);
    metaEdit->setText(metaPicked);
    if (outputEdit->text().isEmpty()) {
        outputEdit->setText(defaultWorkspacePaths.runs);
    }

    constexpr int kLiveModelIdRole = Qt::UserRole + 1;
    constexpr int kLiveModelOnnxRole = Qt::UserRole + 2;
    constexpr int kLiveModelMetadataRole = Qt::UserRole + 3;
    constexpr int kLiveModelStateRole = Qt::UserRole + 4;
    constexpr int kLiveModelModeRole = Qt::UserRole + 5;
    constexpr int kLiveModelTargetRole = Qt::UserRole + 6;
    constexpr int kLiveModelSummaryRole = Qt::UserRole + 7;
    constexpr int kLiveModelOnnxHashRole = Qt::UserRole + 8;
    constexpr int kLiveModelMetadataHashRole = Qt::UserRole + 9;

    auto addLiveModelRow = [&](const QString& label, const QString& id, const QString& onnxPath,
                               const QString& metadataPath, const QString& state, const QString& mode,
                               const QString& targetClassId, const QString& summary, const QString& onnxSha256,
                               const QString& metadataSha256, bool selectable) {
        liveModelCombo->addItem(label);
        const int index = liveModelCombo->count() - 1;
        liveModelCombo->setItemData(index, id, kLiveModelIdRole);
        liveModelCombo->setItemData(index, onnxPath, kLiveModelOnnxRole);
        liveModelCombo->setItemData(index, metadataPath, kLiveModelMetadataRole);
        liveModelCombo->setItemData(index, state, kLiveModelStateRole);
        liveModelCombo->setItemData(index, mode, kLiveModelModeRole);
        liveModelCombo->setItemData(index, targetClassId, kLiveModelTargetRole);
        liveModelCombo->setItemData(index, summary, kLiveModelSummaryRole);
        liveModelCombo->setItemData(index, onnxSha256, kLiveModelOnnxHashRole);
        liveModelCombo->setItemData(index, metadataSha256, kLiveModelMetadataHashRole);
        liveModelCombo->setItemData(index, summary, Qt::ToolTipRole);
        if (!selectable) {
            liveModelCombo->setItemData(index, QColor(Qt::gray), Qt::ForegroundRole);
            if (auto* itemModel = qobject_cast<QStandardItemModel*>(liveModelCombo->model())) {
                if (auto* item = itemModel->item(index)) {
                    item->setEnabled(false);
                }
            }
        }
    };

    for (const auto& value : registryEntries) {
        QJsonObject entry = value.toObject();
        const QString targetId = registryNestedString(entry, "target_policy", "target_class_id");
        const QString targetDisplay = registryNestedString(entry, "target_policy", "target_display_label");
        QString label = registryString(entry, "display_name");
        if (label.isEmpty())
            label = registryString(entry, "registry_entry_id");
        label += " - " + registryString(entry, "state");
        if (!targetId.isEmpty()) {
            label += " - " + (targetDisplay.isEmpty() ? targetId : QString("%1 (%2)").arg(targetDisplay, targetId));
        }
        const bool selectable = entry.value("selectable_for_normal_live_sorting").toBool(false) &&
                                registryString(entry, "live_use_mode") != "blocked";
        addLiveModelRow(label, registryString(entry, "registry_entry_id"),
                        runtimePathFromRegistryPath(registryString(entry, "model_path")),
                        runtimePathFromRegistryPath(registryString(entry, "metadata_path")),
                        registryString(entry, "state"), selectable ? registryString(entry, "live_use_mode") : "blocked",
                        targetId, registryEntrySummary(entry, registryFilePath, registryLoadWarning),
                        registryString(entry, "model_sha256"), registryString(entry, "metadata_sha256"), selectable);
    }
    if (liveModelCombo->count() == 0) {
        addLiveModelRow("Temporary static fallback - promoted/current binary runtime - Hits (1)",
                        "run_20260429_221500_wsl2_binary_linuxmirror_onnx", onnxPicked, metaPicked, "promoted_current",
                        "normal", "1", "Temporary static fallback row. Registry file was empty or unavailable.",
                        "34eec09f49ab4612a34e3a24ccf85eccc98516b388fbadbfb0736ecbf8fb1769",
                        "528ac091764c09cd9c2c6ad2a6ff1e38bb009184a26e7352b71b3a025c30902d", true);
    }
    int activeLiveModelIndex = 0;
    for (int i = 0; i < liveModelCombo->count(); ++i) {
        if (liveModelCombo->itemData(i, kLiveModelModeRole).toString() != "blocked") {
            activeLiveModelIndex = i;
            break;
        }
    }
    liveModelCombo->setCurrentIndex(activeLiveModelIndex);
    appState.activeModelId = liveModelCombo->currentData(kLiveModelIdRole).toString();
    liveModelSummaryText->setPlainText(liveModelCombo->currentData(kLiveModelSummaryRole).toString());

    QString pendingTargetClassId = appState.targetClassId;
    auto selectedTargetClassId = [&]() -> QString {
        if (!targetClassCombo)
            return QString();
        QVariant data = targetClassCombo->currentData();
        QString classId = data.isValid() ? data.toString().trimmed() : QString();
        if (!classId.isEmpty())
            return classId;
        classId = targetClassCombo->currentText().trimmed();
        return classId.isEmpty() ? appState.targetClassId : classId;
    };

    auto setSelectedTargetClassId = [&](const QString& classId) {
        pendingTargetClassId = classId.trimmed();
        if (pendingTargetClassId.isEmpty())
            return;
        appState.targetClassId = pendingTargetClassId;
        for (int i = 0; i < targetClassCombo->count(); ++i) {
            if (targetClassCombo->itemData(i).toString() == pendingTargetClassId) {
                targetClassCombo->setCurrentIndex(i);
                return;
            }
        }
    };

    auto saveRuntimeSettings = [&]() {
        runtimeSettings.setValue(kRuntimeSettingsSchemaVersionKey, kRuntimeSettingsSchemaVersion);
        runtimeSettings.setValue("runtime/v1/model/path", onnxEdit->text().trimmed());
        runtimeSettings.setValue("runtime/v1/model/metadataPath", metaEdit->text().trimmed());
        appState.targetClassId = selectedTargetClassId();
        runtimeSettings.setValue("runtime/v1/model/targetClassId", appState.targetClassId);
        runtimeSettings.setValue("runtime/v1/output/baseDir", runOutputBaseForSettings(outputEdit->text()));
        runtimeSettings.setValue("runtime/v1/output/saveCrops", saveCropCheck->isChecked());
        runtimeSettings.setValue("runtime/v1/output/saveOverlays", saveOverlayCheck->isChecked());

        runtimeSettings.setValue("runtime/v1/camera/presetText", presetCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/customWidth", customWidthSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/customHeight", customHeightSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/binning", binCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/bits", bitsCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/exposureMs", exposureSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/readoutSpeed", readoutCombo->currentText());
        runtimeSettings.setValue("runtime/v1/camera/displayEvery", displayEverySpin->value());
        runtimeSettings.setValue("runtime/v1/camera/lutMin", lutMinSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/lutMax", lutMaxSpin->value());
        runtimeSettings.setValue("runtime/v1/camera/savePath", savePathEdit->text().trimmed());

        runtimeSettings.setValue("runtime/v1/detector/frameSkip", frameSkipSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/bgFrames", bgFramesSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/bgUpdateFrames", bgUpdateSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/resetFrames", resetFramesSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minArea", minAreaSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minAreaFrac", minAreaFracSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/maxAreaFrac", maxAreaFracSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/minBbox", minBboxSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/margin", marginSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/diffThresh", diffThreshSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/blurRadius", blurRadiusSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/morphRadius", morphRadiusSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/scale", scaleSpin->value());
        runtimeSettings.setValue("runtime/v1/detector/gapFireShift", gapFireSpin->value());
        runtimeSettings.sync();
    };

    auto restoreRuntimeSettings = [&]() {
        if (!runtimeSettings.contains(kRuntimeSettingsSchemaVersionKey))
            return;
        const int schemaVersion = runtimeSettings.value(kRuntimeSettingsSchemaVersionKey, 0).toInt();
        if (schemaVersion < 1 || schemaVersion > kRuntimeSettingsSchemaVersion)
            return;

        onnxEdit->setText(runtimeSettings.value("runtime/v1/model/path", onnxEdit->text()).toString());
        metaEdit->setText(runtimeSettings.value("runtime/v1/model/metadataPath", metaEdit->text()).toString());
        setSelectedTargetClassId(
            runtimeSettings.value("runtime/v1/model/targetClassId", pendingTargetClassId).toString());
        outputEdit->setText(runtimeSettings.value("runtime/v1/output/baseDir", outputEdit->text()).toString());
        saveCropCheck->setChecked(
            runtimeSettings.value("runtime/v1/output/saveCrops", saveCropCheck->isChecked()).toBool());
        saveOverlayCheck->setChecked(
            runtimeSettings.value("runtime/v1/output/saveOverlays", saveOverlayCheck->isChecked()).toBool());

        setComboTextIfPresent(presetCombo, runtimeSettings.value("runtime/v1/camera/presetText").toString());
        customWidthSpin->setValue(
            runtimeSettings.value("runtime/v1/camera/customWidth", customWidthSpin->value()).toInt());
        customHeightSpin->setValue(
            runtimeSettings.value("runtime/v1/camera/customHeight", customHeightSpin->value()).toInt());
        setComboTextIfPresent(binCombo, runtimeSettings.value("runtime/v1/camera/binning").toString());
        setComboTextIfPresent(bitsCombo, runtimeSettings.value("runtime/v1/camera/bits").toString());
        exposureSpin->setValue(runtimeSettings.value("runtime/v1/camera/exposureMs", exposureSpin->value()).toDouble());
        setComboTextIfPresent(readoutCombo, runtimeSettings.value("runtime/v1/camera/readoutSpeed").toString());
        displayEverySpin->setValue(
            runtimeSettings.value("runtime/v1/camera/displayEvery", displayEverySpin->value()).toInt());
        lutMinSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMin", lutMinSpin->value()).toInt());
        lutMaxSpin->setValue(runtimeSettings.value("runtime/v1/camera/lutMax", lutMaxSpin->value()).toInt());
        savePathEdit->setText(runtimeSettings.value("runtime/v1/camera/savePath", savePathEdit->text()).toString());

        frameSkipSpin->setValue(runtimeSettings.value("runtime/v1/detector/frameSkip", frameSkipSpin->value()).toInt());
        bgFramesSpin->setValue(runtimeSettings.value("runtime/v1/detector/bgFrames", bgFramesSpin->value()).toInt());
        bgUpdateSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/bgUpdateFrames", bgUpdateSpin->value()).toInt());
        resetFramesSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/resetFrames", resetFramesSpin->value()).toInt());
        minAreaSpin->setValue(runtimeSettings.value("runtime/v1/detector/minArea", minAreaSpin->value()).toDouble());
        minAreaFracSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/minAreaFrac", minAreaFracSpin->value()).toDouble());
        maxAreaFracSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/maxAreaFrac", maxAreaFracSpin->value()).toDouble());
        minBboxSpin->setValue(runtimeSettings.value("runtime/v1/detector/minBbox", minBboxSpin->value()).toInt());
        marginSpin->setValue(runtimeSettings.value("runtime/v1/detector/margin", marginSpin->value()).toInt());
        diffThreshSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/diffThresh", diffThreshSpin->value()).toInt());
        blurRadiusSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/blurRadius", blurRadiusSpin->value()).toInt());
        morphRadiusSpin->setValue(
            runtimeSettings.value("runtime/v1/detector/morphRadius", morphRadiusSpin->value()).toInt());
        scaleSpin->setValue(runtimeSettings.value("runtime/v1/detector/scale", scaleSpin->value()).toDouble());
        gapFireSpin->setValue(runtimeSettings.value("runtime/v1/detector/gapFireShift", gapFireSpin->value()).toInt());
    };

    auto runtimeSettingsSnapshot = [&](const QString& runMode) -> QJsonObject {
        QJsonObject root;
        root["schema_version"] = kRuntimeSettingsSchemaVersion;
        root["run_mode"] = runMode;
        root["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        QJsonObject model;
        model["registry_entry_id"] = liveModelCombo->currentData(kLiveModelIdRole).toString();
        model["model_state_at_start"] = liveModelCombo->currentData(kLiveModelStateRole).toString();
        model["live_use_mode"] = liveModelCombo->currentData(kLiveModelModeRole).toString();
        model["path"] = onnxEdit->text().trimmed();
        model["metadata_path"] = metaEdit->text().trimmed();
        model["model_sha256"] = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString();
        model["metadata_sha256"] = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString();
        model["target_class_id"] = selectedTargetClassId();
        model["target_display_label"] = targetClassCombo->currentText().trimmed();
        model["selection_summary"] = liveModelCombo->currentData(kLiveModelSummaryRole).toString();
        root["model"] = model;

        QJsonObject output;
        output["run_dir"] = outputEdit->text().trimmed();
        output["base_dir"] = runOutputBaseForSettings(outputEdit->text());
        output["save_crops"] = saveCropCheck->isChecked();
        output["save_overlays"] = saveOverlayCheck->isChecked();
        root["output"] = output;

        QJsonObject camera;
        camera["preset"] = comboSnapshot(presetCombo);
        camera["custom_width"] = customWidthSpin->value();
        camera["custom_height"] = customHeightSpin->value();
        camera["binning"] = binCombo->currentText();
        camera["independent_binning"] = false;
        camera["bin_h"] = std::max(1, binCombo->currentText().toInt());
        camera["bin_v"] = std::max(1, binCombo->currentText().toInt());
        camera["bits"] = bitsCombo->currentText();
        camera["exposure_ms"] = exposureSpin->value();
        camera["readout_speed"] = readoutCombo->currentText();
        camera["display_every"] = displayEverySpin->value();
        camera["lut_min"] = lutMinSpin->value();
        camera["lut_max"] = lutMaxSpin->value();
        camera["save_path"] = savePathEdit->text().trimmed();
        root["camera"] = camera;

        QJsonObject detector;
        detector["frame_skip"] = frameSkipSpin->value();
        detector["bg_frames"] = bgFramesSpin->value();
        detector["bg_update_frames"] = bgUpdateSpin->value();
        detector["reset_frames"] = resetFramesSpin->value();
        detector["min_area"] = minAreaSpin->value();
        detector["min_area_frac"] = minAreaFracSpin->value();
        detector["max_area_frac"] = maxAreaFracSpin->value();
        detector["min_bbox"] = minBboxSpin->value();
        detector["margin"] = marginSpin->value();
        detector["diff_thresh"] = diffThreshSpin->value();
        detector["blur_radius"] = blurRadiusSpin->value();
        detector["morph_radius"] = morphRadiusSpin->value();
        detector["scale"] = scaleSpin->value();
        detector["gap_fire_shift"] = gapFireSpin->value();
        root["detector"] = detector;

        return root;
    };

    auto writeRuntimeSettingsSnapshot = [&](const QString& runDir, const QString& runMode) {
        if (runDir.trimmed().isEmpty())
            return;
        QDir dir(runDir);
        dir.mkpath(".");
        QFile file(dir.filePath("runtime_settings_snapshot.json"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            logMessage(QString("Failed to write runtime settings snapshot: %1").arg(file.errorString()));
            return;
        }
        file.write(QJsonDocument(runtimeSettingsSnapshot(runMode)).toJson(QJsonDocument::Indented));
    };

    auto resolveAppRelative = [&](const QString& path) -> QString {
        if (path.isEmpty())
            return path;
        QFileInfo info(path);
        if (info.isAbsolute())
            return info.absoluteFilePath();
        QString abs = QDir(appDir).absoluteFilePath(path);
        if (QFileInfo::exists(abs))
            return abs;
        QString fallback = findModelUpwards(QFileInfo(path).fileName());
        if (!fallback.isEmpty())
            return fallback;
        return abs;
    };

    auto populateTargetClassSelector = [&]() {
        QString requestedClassId = pendingTargetClassId;
        if (requestedClassId.isEmpty()) {
            requestedClassId = selectedTargetClassId();
        }

        Metadata metadata;
        std::string err;
        const QString metadataPath = resolveAppRelative(metaEdit->text().trimmed());
        const bool loaded = LoadMetadata(metadataPath.toStdString(), metadata, err);

        QSignalBlocker blocker(targetClassCombo);
        targetClassCombo->clear();

        if (!loaded || metadata.classes.empty()) {
            const QString fallbackId = requestedClassId.isEmpty() ? QStringLiteral("Single") : requestedClassId;
            targetClassCombo->addItem(fallbackId, fallbackId);
            pendingTargetClassId = fallbackId;
            logMessage(QString("Target selector using legacy fallback class id '%1': %2")
                           .arg(fallbackId, QString::fromStdString(err)));
            return;
        }

        for (const std::string& classIdStd : metadata.classes) {
            const std::string displayLabel = DisplayLabelForClassId(metadata, classIdStd);
            const QString classId = QString::fromStdString(classIdStd);
            const QString displayText = QString::fromStdString(FormatClassForDisplay(classIdStd, displayLabel));
            targetClassCombo->addItem(displayText, classId);
        }

        std::string resolvedClassId;
        std::string resolvedDisplayLabel;
        std::string resolveErr;
        if (!ResolveTargetClassId(metadata, requestedClassId.toStdString(), std::string(), resolvedClassId,
                                  resolvedDisplayLabel, resolveErr)) {
            ResolveTargetClassId(metadata, std::string(), std::string(), resolvedClassId, resolvedDisplayLabel,
                                 resolveErr);
        }

        if (!resolvedClassId.empty()) {
            pendingTargetClassId = QString::fromStdString(resolvedClassId);
            for (int i = 0; i < targetClassCombo->count(); ++i) {
                if (targetClassCombo->itemData(i).toString() == pendingTargetClassId) {
                    targetClassCombo->setCurrentIndex(i);
                    break;
                }
            }
        } else if (targetClassCombo->count() > 0) {
            targetClassCombo->setCurrentIndex(0);
            pendingTargetClassId = targetClassCombo->itemData(0).toString();
        }
    };

    auto applyLiveModelSelection = [&]() {
        const QString mode = liveModelCombo->currentData(kLiveModelModeRole).toString();
        const QString summary = liveModelCombo->currentData(kLiveModelSummaryRole).toString();
        appState.activeModelId = liveModelCombo->currentData(kLiveModelIdRole).toString();
        liveModelSummaryText->setPlainText(summary);
        if (mode == "blocked") {
            pipelineStatusLabel->setText(
                "Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            return;
        }
        const QString onnxPath = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString metadataPath = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        const QJsonObject packagedActiveEntry = activeRegistryEntry(registryEntries);
        const bool selectedPackagedActive =
            appState.activeModelId == registryString(packagedActiveEntry, "registry_entry_id");
        if (selectedPackagedActive && !defaultWorkspacePaths.activeModel.isEmpty()) {
            onnxEdit->setText(defaultWorkspacePaths.activeModel);
        } else if (!onnxPath.isEmpty()) {
            onnxEdit->setText(onnxPath);
        }
        if (selectedPackagedActive && !defaultWorkspacePaths.activeMetadata.isEmpty()) {
            metaEdit->setText(defaultWorkspacePaths.activeMetadata);
        } else if (!metadataPath.isEmpty()) {
            metaEdit->setText(metadataPath);
        }
        const QString targetClassId = liveModelCombo->currentData(kLiveModelTargetRole).toString();
        if (!targetClassId.isEmpty()) {
            pendingTargetClassId = targetClassId;
            appState.targetClassId = targetClassId;
        }
        populateTargetClassSelector();
        saveRuntimeSettings();
        pipelineStatusLabel->setText("Live model selected: " + liveModelCombo->currentText());
    };

    QTimer detectorTuningApplyTimer;
    detectorTuningApplyTimer.setSingleShot(true);
    detectorTuningApplyTimer.setInterval(250);

    std::shared_ptr<std::vector<SequenceFrame>> sequenceFrames;
    QMutex sequenceMutex;
    std::atomic<bool> sequenceRunning(false);
    std::atomic<bool> sequenceStarting(false);
    std::atomic<bool> sequenceStop(false);
    std::thread sequenceThread;
    BackgroundTaskRegistry backgroundTasks;
    std::atomic<bool> sequenceLoading(false);
    bool sequencePrevPipelineChecked = false;
    StatsTracker stats;
    QMutex statsMutex;
    QMutex liveLogMutex;
    std::vector<LiveLogRecord> liveLog;
    std::atomic<bool> liveLogging(false);
    QDateTime liveLogStart;
    std::function<void()> startLiveLogging;
    std::function<void()> stopLiveLogging;
    QMutex datasetCaptureMutex;
    DatasetCaptureSession datasetCaptureSession;
    std::atomic<bool> datasetCaptureActive(false);
    QString datasetCaptureDir;
    QString datasetCaptureManifestPath;

    imageView->setZoomChanged([=](double zoom) {
        zoomStatusLabel->setText(QString("%1%").arg(static_cast<int>(std::lround(zoom * 100.0))));
        scaleStatusLabel->setText(QString("SF: %1 Px").arg(zoom, 0, 'f', 3));
    });
    cameraImageView->setZoomChanged([=](double zoom) { Q_UNUSED(zoom); });

    QThread cameraThread;
    cameraThread.setObjectName("CameraWorkerThread");
    auto* cameraWorker = new CameraWorker();
    bool cameraOpened = false;
    PipelineRunner pipeline;
    QMutex pipelineMutex;
    std::atomic<bool> pipelineEnabled(false);
    ValidatorWorkspaceController::Dependencies validatorWorkspaceControllerDeps;
    validatorWorkspaceControllerDeps.parentWindow = this;
    validatorWorkspaceControllerDeps.imageValidationAction = imageValidationAction;
    validatorWorkspaceControllerDeps.onnxEdit = onnxEdit;
    validatorWorkspaceControllerDeps.metaEdit = metaEdit;
    validatorWorkspaceControllerDeps.pythonStatusItem = pythonStatusItem;
    validatorWorkspaceControllerDeps.preparedDatasetPath = defaultWorkspacePaths.preparedDataset;
    validatorWorkspaceControllerDeps.validationRunsRoot = defaultWorkspacePaths.runs;
    validatorWorkspaceControllerDeps.appDir = appDir;
    validatorWorkspaceControllerDeps.resolveAppRelative = resolveAppRelative;
    validatorWorkspaceControllerDeps.seqFolderEdit = seqFolderEdit;
    validatorWorkspaceControllerDeps.seqBrowseBtn = seqBrowseBtn;
    validatorWorkspaceControllerDeps.seqLoadBtn = seqLoadBtn;
    validatorWorkspaceControllerDeps.seqStartBtn = seqStartBtn;
    validatorWorkspaceControllerDeps.seqStopBtn = seqStopBtn;
    validatorWorkspaceControllerDeps.seqStatusLabel = seqStatusLabel;
    validatorWorkspaceControllerDeps.statusLabel = statusLabel;
    validatorWorkspaceControllerDeps.pipelineWidget = pipelineWidget;
    validatorWorkspaceControllerDeps.labviewWidget = labviewWidget;
    validatorWorkspaceControllerDeps.detectWidget = detectWidget;
    validatorWorkspaceControllerDeps.pipelineStartBtn = pipelineStartBtn;
    validatorWorkspaceControllerDeps.pipelineStopBtn = pipelineStopBtn;
    validatorWorkspaceControllerDeps.startBtn = startBtn;
    validatorWorkspaceControllerDeps.stopBtn = nullptr;
    validatorWorkspaceControllerDeps.reconnectBtn = reconnectBtn;
    validatorWorkspaceControllerDeps.applyBtn = applyBtn;
    validatorWorkspaceControllerDeps.viewerOnly = &viewerOnly;
    validatorWorkspaceControllerDeps.pipelineEnabled = &pipelineEnabled;
    validatorWorkspaceControllerDeps.sequenceFrames = &sequenceFrames;
    validatorWorkspaceControllerDeps.sequenceMutex = &sequenceMutex;
    validatorWorkspaceControllerDeps.sequenceRunning = &sequenceRunning;
    validatorWorkspaceControllerDeps.sequenceStop = &sequenceStop;
    validatorWorkspaceControllerDeps.sequenceLoading = &sequenceLoading;
    validatorWorkspaceControllerDeps.sequenceThread = &sequenceThread;
    validatorWorkspaceControllerDeps.backgroundTasks = &backgroundTasks;
    auto* validatorWorkspaceController = new ValidatorWorkspaceController(validatorWorkspaceControllerDeps, this);
    bool labviewTriggerReady = false;
    SettingsWorkspaceController::Dependencies settingsControllerDeps;
    settingsControllerDeps.appState = &appState;
    settingsControllerDeps.daqDeviceCombo = daqDeviceCombo;
    settingsControllerDeps.daqChannelEdit = daqChannelEdit;
    settingsControllerDeps.amplitudeSpin = amplitudeSpin;
    settingsControllerDeps.frequencySpin = freqSpin;
    settingsControllerDeps.durationSpin = durationSpin;
    settingsControllerDeps.delaySpin = delaySpin;
    settingsControllerDeps.labviewStatusDot = labviewStatusDot;
    settingsControllerDeps.labviewStatusText = labviewStatusText;
    settingsControllerDeps.labviewOutputLabel = labviewOutputLabel;
    settingsControllerDeps.daqStatusItem = daqStatusItem;
    settingsControllerDeps.pipelineEnableCheck = pipelineEnableCheck;
    settingsControllerDeps.viewerOnly = &viewerOnly;
    auto* settingsController = new SettingsWorkspaceController(settingsControllerDeps, this);

    CameraWorkspaceController::Dependencies cameraControllerDeps;
    cameraControllerDeps.app = &app;
    cameraControllerDeps.window = this;
    cameraControllerDeps.statusBar = this->statusBar();
    cameraControllerDeps.cameraWorker = cameraWorker;
    cameraControllerDeps.options = &options;
    cameraControllerDeps.appState = &appState;
    cameraControllerDeps.controls = cameraWorkspaceControls;
    cameraControllerDeps.viewerOnly = &viewerOnly;
    cameraControllerDeps.cameraOpened = &cameraOpened;
    cameraControllerDeps.daqBuildEnabled = kDaqBuildEnabled;
    cameraControllerDeps.initialDaqStatusText = initialDaqStatusText;
    cameraControllerDeps.statusLabel = statusLabel;
    cameraControllerDeps.cameraStatusItem = cameraStatusItem;
    cameraControllerDeps.modelStatusItem = modelStatusItem;
    cameraControllerDeps.daqStatusItem = daqStatusItem;
    cameraControllerDeps.runStatusItem = runStatusItem;
    cameraControllerDeps.pipelineStatusLabel = pipelineStatusLabel;
    cameraControllerDeps.statsLabel = statsLabel;
    cameraControllerDeps.liveImageView = imageView;
    cameraControllerDeps.cameraImageView = cameraImageView;
    cameraControllerDeps.liveViewerEmpty = liveViewerEmpty;
    cameraControllerDeps.cameraViewerEmpty = cameraViewerEmpty;
    cameraControllerDeps.liveHudResolution = liveHudResolution;
    cameraControllerDeps.cameraHudResolution = cameraHudResolution;
    cameraControllerDeps.liveHudFrameTime = liveHudFrameTime;
    cameraControllerDeps.cameraHudFrameTime = cameraHudFrameTime;
    cameraControllerDeps.liveHudFps = liveHudFps;
    cameraControllerDeps.cameraHudFps = cameraHudFps;
    cameraControllerDeps.startButton = startBtn;
    cameraControllerDeps.reconnectButton = reconnectBtn;
    cameraControllerDeps.applyButton = applyBtn;
    cameraControllerDeps.operationalTabs = operationalTabs;
    cameraControllerDeps.pipeline = &pipeline;
    cameraControllerDeps.pipelineMutex = &pipelineMutex;
    cameraControllerDeps.pipelineEnabled = &pipelineEnabled;
    cameraControllerDeps.logLine = logLine;
    cameraControllerDeps.systemLogLine = [](const QString& message) { logMessage(message); };
    auto* cameraController = new CameraWorkspaceController(cameraControllerDeps, this);

    cameraController->updateLutRange(cameraController->currentBits());
    restoreRuntimeSettings();
    auto repairRuntimeModelPaths = [&]() {
        auto rawPathExists = [&](const QString& path) {
            const QString trimmed = path.trimmed();
            if (trimmed.isEmpty())
                return false;
            QFileInfo info(trimmed);
            if (info.isAbsolute())
                return info.exists();
            return QFileInfo(QDir(appDir).absoluteFilePath(trimmed)).exists();
        };
        if (rawPathExists(onnxEdit->text()) && rawPathExists(metaEdit->text())) {
            return;
        }
        const QString registryOnnx = liveModelCombo->currentData(kLiveModelOnnxRole).toString();
        const QString registryMeta = liveModelCombo->currentData(kLiveModelMetadataRole).toString();
        if (!registryOnnx.isEmpty())
            onnxEdit->setText(registryOnnx);
        if (!registryMeta.isEmpty())
            metaEdit->setText(registryMeta);
        logMessage(QString("Runtime model paths repaired from selected registry entry: onnx=%1 meta=%2")
                       .arg(onnxEdit->text(), metaEdit->text()));
    };
    repairRuntimeModelPaths();
    populateTargetClassSelector();
    cameraController->updateLutRange(cameraController->currentBits());

    QPointer<ViewerWindow> viewerWindow;
    QPointer<StatsFigureWindow> statsFigureWindow;
    QObject::connect(viewerBtn, &QPushButton::clicked, [&]() {
        if (viewerWindow) {
            viewerWindow->raise();
            viewerWindow->activateWindow();
            return;
        }
        viewerWindow = new ViewerWindow(nullptr);
        viewerWindow->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(viewerWindow, &QObject::destroyed, [&]() { viewerWindow = nullptr; });
        viewerWindow->show();
    });

    // Save state
    auto saveBuffer = std::make_shared<std::vector<QImage>>();
    auto saveMutex = std::make_shared<QMutex>();
    std::atomic<bool> recording{false};
    std::atomic<bool> saving{false};
    QElapsedTimer recordTimer;
    QDateTime recordStartTime;
    std::atomic<int> recordedFrames{0};
    QTimer saveInfoTimer;
    saveInfoTimer.setInterval(200);

    QObject::connect(saveBrowseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select save directory", savePathEdit->text());
        if (!dir.isEmpty())
            savePathEdit->setText(dir);
    });
    QObject::connect(saveOpenBtn, &QPushButton::clicked, [&]() {
        QString dir = savePathEdit->text();
        if (dir.isEmpty())
            dir = QCoreApplication::applicationDirPath();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });

    QObject::connect(onnxBrowseBtn, &QPushButton::clicked, [&]() {
        QString file = QFileDialog::getOpenFileName(this, "Select ONNX model", onnxEdit->text(), "ONNX Model (*.onnx)");
        if (!file.isEmpty()) {
            onnxEdit->setText(file);
            saveRuntimeSettings();
        }
    });
    QObject::connect(metaBrowseBtn, &QPushButton::clicked, [&]() {
        QString file = QFileDialog::getOpenFileName(this, "Select metadata JSON", metaEdit->text(), "JSON (*.json)");
        if (!file.isEmpty()) {
            metaEdit->setText(file);
            populateTargetClassSelector();
            saveRuntimeSettings();
        }
    });
    QObject::connect(outputBrowseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select output directory", outputEdit->text());
        if (!dir.isEmpty()) {
            outputEdit->setText(dir);
            saveRuntimeSettings();
        }
    });
    QObject::connect(liveModelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                     [&]() { applyLiveModelSelection(); });
    QObject::connect(refreshLiveModelsBtn, &QPushButton::clicked, [&]() { applyLiveModelSelection(); });

    auto connectRuntimeSettingsPersistence = [&]() {
        QObject::connect(onnxEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(metaEdit, &QLineEdit::editingFinished, [&]() {
            populateTargetClassSelector();
            saveRuntimeSettings();
        });
        QObject::connect(outputEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(targetClassCombo, qOverload<int>(&QComboBox::currentIndexChanged), [&]() {
            pendingTargetClassId = selectedTargetClassId();
            appState.targetClassId = pendingTargetClassId;
            saveRuntimeSettings();
        });
        QObject::connect(savePathEdit, &QLineEdit::editingFinished, saveRuntimeSettings);
        QObject::connect(saveCropCheck, &QCheckBox::toggled, saveRuntimeSettings);
        QObject::connect(saveOverlayCheck, &QCheckBox::toggled, saveRuntimeSettings);

        QObject::connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(customWidthSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(customHeightSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(binCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(bitsCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(exposureSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(readoutCombo, qOverload<int>(&QComboBox::currentIndexChanged), saveRuntimeSettings);
        QObject::connect(displayEverySpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(lutMinSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);
        QObject::connect(lutMaxSpin, qOverload<int>(&QSpinBox::valueChanged), saveRuntimeSettings);

        auto persistAndScheduleDetectorApply = [&]() {
            saveRuntimeSettings();
            scheduleDetectorApply();
        };
        QObject::connect(frameSkipSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(bgFramesSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(bgUpdateSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(resetFramesSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(minAreaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(minAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(maxAreaFracSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         persistAndScheduleDetectorApply);
        QObject::connect(minBboxSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(marginSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(diffThreshSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(blurRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(morphRadiusSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(scaleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(gapFireSpin, qOverload<int>(&QSpinBox::valueChanged), persistAndScheduleDetectorApply);
        QObject::connect(&app, &QCoreApplication::aboutToQuit, saveRuntimeSettings);
    };
    connectRuntimeSettingsPersistence();
    saveRuntimeSettings();

    QObject::connect(pipelineEnableCheck, &QCheckBox::toggled, [&](bool enabled) {
        pipelineEnabled.store(enabled);
        updateForceTriggerState();
        updateLiveRunStartStopVisibility();
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        pipelineStartBtn->setEnabled(!enabled && !sequenceRunning.load() && ready);
        pipelineStopBtn->setEnabled(enabled && !sequenceRunning.load());
        if (!enabled) {
            pipelineStatusLabel->setText("Pipeline: paused");
            settingsController->setLabviewStatus("Disabled", "#666");
            if (!sequenceRunning.load() && !sequenceStarting.load()) {
                stopLiveLogging();
            }
        } else if (daqChannelEdit->text().trimmed().isEmpty()) {
            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
        } else {
            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
        }
        if (enabled && !sequenceRunning.load() && !sequenceStarting.load()) {
            if (ready && !liveLogging.load()) {
                startLiveLogging();
            }
        }
    });

    bool daqStartupStateLogged = false;
    auto logDaqStartupState = [&](const QString& stateText) {
        if (daqStartupStateLogged)
            return;
        daqStartupStateLogged = true;
        logMessage("DAQ startup state: " + stateText);
    };

    auto loadPipeline = [&](bool enableAfter, bool forceNoDaq) {
        logMessage("Pipeline init requested");
        settingsController->refreshDaqDeviceOptions(true);
        if (liveModelCombo->currentData(kLiveModelModeRole).toString() == "blocked") {
            pipelineStatusLabel->setText(
                "Live sorting blocked: selected model is not live-use eligible. Open Model Manager for gate evidence.");
            logMessage("Pipeline init blocked by live model selection gate: " + liveModelCombo->currentText());
            return;
        }
        PipelineConfig cfg;
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        cfg.onnxPath = onnxResolved.toStdString();
        cfg.metadataPath = metaResolved.toStdString();
        appState.targetClassId = selectedTargetClassId();
        cfg.targetClassId = appState.targetClassId.toStdString();
        cfg.outputDir = outputEdit->text().toStdString();
        cfg.saveCrop = saveCropCheck->isChecked();
        cfg.saveOverlay = saveOverlayCheck->isChecked();
        cfg.cropSize = 64;
        cfg.frameSkip = frameSkipSpin->value();
        pipelineDetectCfg.bgFrames = bgFramesSpin->value();
        pipelineDetectCfg.bgUpdateFrames = bgUpdateSpin->value();
        pipelineDetectCfg.resetFrames = resetFramesSpin->value();
        pipelineDetectCfg.minArea = minAreaSpin->value();
        pipelineDetectCfg.minAreaFrac = minAreaFracSpin->value();
        pipelineDetectCfg.maxAreaFrac = maxAreaFracSpin->value();
        pipelineDetectCfg.minBbox = minBboxSpin->value();
        pipelineDetectCfg.margin = marginSpin->value();
        pipelineDetectCfg.diffThresh = diffThreshSpin->value();
        pipelineDetectCfg.blurRadius = blurRadiusSpin->value();
        pipelineDetectCfg.morphRadius = morphRadiusSpin->value();
        pipelineDetectCfg.scale = scaleSpin->value();
        pipelineDetectCfg.gapFireShift = gapFireSpin->value();
        cfg.detect = pipelineDetectCfg;
        cfg.daq.channel = daqChannelEdit->text().trimmed().toStdString();
        cfg.daq.rangeMin = -10.0;
        cfg.daq.rangeMax = 10.0;
        cfg.daq.amplitude = amplitudeSpin->value();
        cfg.daq.frequencyHz = freqSpin->value() * 1000.0;
        cfg.daq.durationMs = durationSpin->value();
        cfg.daq.delayMs = delaySpin->value();
        if (forceNoDaq) {
            cfg.daq = DaqConfig{};
        }

        logMessage(QString("Pipeline init paths: onnx=%1 meta=%2").arg(onnxEdit->text(), metaEdit->text()));
        logMessage(QString("Pipeline init resolved paths: onnx=%1 meta=%2").arg(onnxResolved, metaResolved));
        if (forceNoDaq) {
            logMessage("DAQ config: disabled for recorded sequence replay");
        } else {
            logMessage(QString("DAQ config: channel=%1 range=[-10,10] amp=%2V freq=%3Hz duration=%4ms delay=%5ms")
                           .arg(daqChannelEdit->text().trimmed())
                           .arg(amplitudeSpin->value(), 0, 'f', 3)
                           .arg(freqSpin->value() * 1000.0, 0, 'f', 1)
                           .arg(durationSpin->value(), 0, 'f', 3)
                           .arg(delaySpin->value(), 0, 'f', 3));
        }
        if (!settingsController->discoveredDaqDevices().empty()) {
            logMessage(QString("DAQ discovery: %1").arg(settingsController->describeDiscoveredDaqDevices()));
        } else if (!settingsController->daqDiscoveryError().isEmpty()) {
            logMessage(QString("DAQ discovery: %1").arg(settingsController->daqDiscoveryError()));
        } else {
            logMessage("DAQ discovery: no NI-DAQmx devices detected");
        }

        std::string err;
        QString resolvedTargetText;
        {
            QMutexLocker locker(&pipelineMutex);
            try {
                if (!pipeline.init(cfg, err)) {
                    pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(QString::fromStdString(err)));
                    modelStatusItem->setText("Model: error");
                    this->statusBar()->showMessage("Pipeline initialization failed");
                    pipelineEnabled.store(false);
                    pipelineEnableCheck->setChecked(false);
                    pipelineStartBtn->setEnabled(false);
                    labviewTriggerReady = false;
                    settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                    updateForceTriggerState();
                    logMessage(QString("Pipeline init failed: %1").arg(QString::fromStdString(err)));
                    return;
                }
                pipeline.reset();
                labviewTriggerReady = pipeline.isTriggerReady();
                resolvedTargetText = QString::fromStdString(pipeline.targetDisplayText());
                cfg.targetClassId = pipeline.targetClassId();
            } catch (const std::exception& e) {
                const QString exceptionText = QString::fromLocal8Bit(e.what());
                pipelineStatusLabel->setText(QString("Pipeline error: %1").arg(exceptionText));
                modelStatusItem->setText("Model: error");
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QString("DAQ startup exception: %1").arg(exceptionText);
                this->statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage(QString("Pipeline init threw exception: %1").arg(exceptionText));
                return;
            } catch (...) {
                pipelineStatusLabel->setText("Pipeline error: unknown startup exception");
                modelStatusItem->setText("Model: error");
                daqStatusItem->setText("DAQ: unavailable");
                appState.daqAvailable = false;
                appState.daqDisabled = false;
                appState.daqFault = true;
                appState.daqStatusText = daqStatusItem->text();
                appState.daqFaultText = QStringLiteral("DAQ startup exception: unknown");
                this->statusBar()->showMessage("Pipeline initialization failed");
                pipelineEnabled.store(false);
                pipelineEnableCheck->setChecked(false);
                pipelineStartBtn->setEnabled(false);
                labviewTriggerReady = false;
                settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
                updateForceTriggerState();
                logDaqStartupState(daqStatusItem->text() + ": " + appState.daqFaultText);
                logMessage("Pipeline init threw unknown exception");
                return;
            }
        }

        if (!err.empty()) {
            pipelineStatusLabel->setText(QString("Pipeline ready (DAQ off), target %1: %2")
                                             .arg(resolvedTargetText, QString::fromStdString(err)));
            modelStatusItem->setText("Model: loaded");
            daqStatusItem->setText("DAQ: unavailable");
            appState.daqAvailable = false;
            appState.daqFault = true;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText = QString::fromStdString(err);
            logDaqStartupState(daqStatusItem->text() + ": " + QString::fromStdString(err));
            this->statusBar()->showMessage("Pipeline ready with DAQ warning");
            logMessage(QString("Pipeline init warning: %1").arg(QString::fromStdString(err)));
        } else {
            pipelineStatusLabel->setText(QString("Pipeline ready, target %1").arg(resolvedTargetText));
            modelStatusItem->setText("Model: loaded");
            daqStatusItem->setText("DAQ: available");
            appState.daqAvailable = true;
            appState.daqFault = false;
            appState.daqDisabled = false;
            appState.daqStatusText = daqStatusItem->text();
            appState.daqFaultText.clear();
            this->statusBar()->showMessage("Pipeline ready");
            logMessage("Pipeline init success");
        }
        setSelectedTargetClassId(QString::fromStdString(cfg.targetClassId));
        appState.targetClassId = QString::fromStdString(cfg.targetClassId);
        saveRuntimeSettings();
        pipelineStartBtn->setEnabled(!enableAfter && !sequenceRunning.load());
        pipelineEnabled.store(enableAfter);
        if (enableAfter) {
            pipelineEnableCheck->setChecked(true);
        }
        if (enableAfter && !sequenceRunning.load() && !sequenceStarting.load() && !liveLogging.load()) {
            startLiveLogging();
        }

        if (cfg.daq.channel.empty()) {
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            logDaqStartupState(daqStatusItem->text());
        } else {
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            if (!appState.daqAvailable) {
                logDaqStartupState(daqStatusItem->text() + (appState.daqFaultText.isEmpty()
                                                                ? QString()
                                                                : QStringLiteral(": ") + appState.daqFaultText));
            }
        }
        appState.daqWaveformValid = !cfg.daq.channel.empty() && cfg.daq.amplitude > 0.0 && cfg.daq.frequencyHz > 0.0 &&
                                    cfg.daq.durationMs > 0.0;
        appState.daqStatusText = daqStatusItem->text();
        settingsController->updateLabviewOutput();
        updateForceTriggerState();
    };

    QObject::connect(&detectorTuningApplyTimer, &QTimer::timeout, [&]() {
        if (viewerOnly)
            return;
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter, false);
        updateForceTriggerState();
    });
    settingsController->setReloadPipelineCallback([&](bool enableAfter) { loadPipeline(enableAfter, false); });
    settingsController->setUpdateForceTriggerCallback(updateForceTriggerState);
    scheduleDetectorApply = [&]() { detectorTuningApplyTimer.start(); };

    QObject::connect(loadPipelineBtn, &QPushButton::clicked,
                     [&]() { loadPipeline(pipelineEnableCheck->isChecked(), false); });

    QObject::connect(pipelineStartBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            loadPipeline(false, false);
            {
                QMutexLocker lock(&pipelineMutex);
                ready = pipeline.isReady();
            }
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: load a valid pipeline first.");
            runStatusItem->setText("Run: idle");
            this->statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked because pipeline is not ready.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        QString runDir = buildRunOutputDir("live");
        if (runDir.isEmpty()) {
            statusLabel->setText("Start Sorting blocked: failed to create run folder.");
            this->statusBar()->showMessage("Start Sorting blocked: no run folder");
            logMessage("Start Sorting blocked because run folder creation failed.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        outputEdit->setText(runDir);
        writeRuntimeSettingsSnapshot(runDir, "live");
        loadPipeline(true, false);
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            pipelineEnableCheck->setChecked(false);
            statusLabel->setText("Start Sorting blocked: pipeline failed after run setup.");
            runStatusItem->setText("Run: idle");
            this->statusBar()->showMessage("Start Sorting blocked: pipeline not loaded");
            logMessage("Start Sorting blocked after run setup because pipeline is not ready.");
            reportsWorkspaceController.refreshOpenRunAvailability();
            return;
        }
        reportsWorkspaceController.setCurrentRunDir(runDir);
        statusLabel->setText("Pipeline started.");
        updateForceTriggerState();
        runStatusItem->setText("Run: Live View");
        this->statusBar()->showMessage("Live View started");
    });

    QObject::connect(pipelineStopBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        pipelineEnableCheck->setChecked(false);
        updateForceTriggerState();
        statusLabel->setText("Pipeline stopped.");
        runStatusItem->setText("Run: idle");
        reportsWorkspaceController.refreshOpenRunAvailability();
        this->statusBar()->showMessage("Live sorting stopped");
    });

    auto stopDatasetCapture = [&](const QString& reason, bool openReview) {
        QString reviewPath;
        std::string err;
        {
            QMutexLocker lock(&datasetCaptureMutex);
            if (!datasetCaptureActive.load())
                return;
            datasetCaptureSession.setStopReason(reason.toStdString());
            if (!datasetCaptureSession.finalize(err)) {
                logMessage(QString("Dataset Builder capture finalize failed: %1").arg(QString::fromStdString(err)));
            }
            reviewPath = datasetCaptureManifestPath;
            datasetCaptureActive.store(false);
        }
        datasetStartCaptureBtn->setEnabled(true);
        datasetStopCaptureBtn->setEnabled(false);
        datasetCaptureStatusLabel->setText(
            QString("Dataset Builder capture stopped: %1\nManifest: %2").arg(reason, reviewPath));
        statusLabel->setText("Dataset Builder capture stopped. Review required before trainer handoff.");
        trainerDatasetEdit->setText(datasetCaptureDir);
        if (openReview && QFileInfo::exists(reviewPath)) {
            openDatasetLabelerPath(reviewPath);
        }
    };

    auto startDatasetCapture = [&]() {
        if (datasetCaptureActive.load())
            return;
        DatasetCollectionMode mode = DatasetCollectionMode::Mixed;
        std::string modeText = datasetCaptureModeCombo->currentText().toStdString();
        std::string err;
        if (!DatasetCaptureSession::parseCollectionMode(modeText, mode)) {
            datasetCaptureStatusLabel->setText("Invalid Dataset Builder collection mode.");
            return;
        }
        if (liveModelCombo->currentData(kLiveModelModeRole).toString() == "blocked") {
            datasetCaptureStatusLabel->setText("Dataset capture blocked: selected model is not live-use eligible.");
            return;
        }
        QString datasetId;
        QString sessionDir = buildDatasetBuilderDir(&datasetId);
        DatasetCaptureConfig cfg;
        cfg.sessionDir = std::filesystem::path(sessionDir.toStdWString());
        cfg.sessionId = datasetId.toStdString();
        cfg.sourceType = "live_stream";
        cfg.sourcePath = "live_camera";
        cfg.collectionMode = mode;
        cfg.batchTarget = static_cast<std::size_t>(datasetBatchTargetSpin->value());
        cfg.modelPath = resolveAppRelative(onnxEdit->text()).toStdString();
        cfg.metadataPath = resolveAppRelative(metaEdit->text()).toStdString();
        cfg.modelId = liveModelCombo->currentData(kLiveModelIdRole).toString().toStdString();
        cfg.modelSha256 = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString().toStdString();
        cfg.metadataSha256 = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString().toStdString();
        {
            QMutexLocker lock(&datasetCaptureMutex);
            if (!datasetCaptureSession.start(cfg, err)) {
                datasetCaptureStatusLabel->setText("Dataset Builder capture failed: " + QString::fromStdString(err));
                return;
            }
            datasetCaptureDir = sessionDir;
            datasetCaptureManifestPath = QDir(sessionDir).filePath("metadata/dataset_manifest.json");
            datasetCaptureActive.store(true);
        }
        saveCropCheck->setChecked(true);
        if (!pipelineEnableCheck->isChecked()) {
            pipelineEnableCheck->setChecked(true);
        }
        bool ready = false;
        {
            QMutexLocker lock(&pipelineMutex);
            ready = pipeline.isReady();
        }
        if (!ready) {
            loadPipeline(true, false);
        }
        datasetStartCaptureBtn->setEnabled(false);
        datasetStopCaptureBtn->setEnabled(true);
        datasetCaptureStatusLabel->setText(QString("Dataset Builder capture active: 0 / %1 crops\n%2")
                                               .arg(datasetBatchTargetSpin->value())
                                               .arg(sessionDir));
        trainerDatasetEdit->setText(sessionDir);
        statusLabel->setText("Dataset Builder capture active. Crops remain unreviewed until manual review.");
        logMessage("Dataset Builder live capture started: " + sessionDir);
    };

    QObject::connect(datasetStartCaptureBtn, &QPushButton::clicked, startDatasetCapture);
    QObject::connect(datasetCaptureFromCameraAction, &QAction::triggered, startDatasetCapture);
    QObject::connect(datasetStopCaptureBtn, &QPushButton::clicked, [&]() { stopDatasetCapture("cancelled", true); });

    QObject::connect(labviewReconnectBtn, &QPushButton::clicked, [&]() {
        bool enableAfter = pipelineEnableCheck->isChecked();
        loadPipeline(enableAfter, false);
    });

    auto runManualDaqTrigger = [&](const QString& triggerSource) {
        updateForceTriggerState();
        const bool waveformValid = !daqChannelEdit->text().trimmed().isEmpty() && amplitudeSpin->value() > 0.0 &&
                                   freqSpin->value() > 0.0 && durationSpin->value() > 0.0;
        QStringList blockers;
        if (!appState.daqAvailable || appState.daqDisabled)
            blockers << QStringLiteral("DAQ is not available");
        if (appState.daqFault)
            blockers << (appState.daqFaultText.isEmpty() ? QStringLiteral("DAQ fault is active")
                                                         : appState.daqFaultText);
        if (!waveformValid)
            blockers << QStringLiteral("waveform settings are incomplete");
        if (!blockers.isEmpty()) {
            const QString message =
                QStringLiteral("%1 blocked: %2.").arg(triggerSource, blockers.join("; "));
            statusLabel->setText(message);
            this->statusBar()->showMessage("Manual trigger blocked");
            logMessage(message);
            updateForceTriggerState();
            return;
        }
        DaqConfig cfg;
        cfg.channel = daqChannelEdit->text().trimmed().toStdString();
        cfg.rangeMin = -10.0;
        cfg.rangeMax = 10.0;
        cfg.amplitude = amplitudeSpin->value();
        cfg.frequencyHz = freqSpin->value() * 1000.0;
        cfg.durationMs = durationSpin->value();
        cfg.delayMs = delaySpin->value();

        statusLabel->setText("DAQ trigger queued...");
        logMessage(QString("%1 queued direct DAQ output on %2.").arg(triggerSource, daqChannelEdit->text().trimmed()));
        QPointer<QWidget> windowPtr(this);
        QPointer<QLabel> statusLabelPtr(statusLabel);
        backgroundTasks.launch("daq-manual-trigger", [&, cfg, windowPtr,
                                                      statusLabelPtr](const BackgroundTaskRegistry::StopFlag& stop) {
            std::string trigErr;
            bool ok = false;
            if (!stop->load()) {
                DaqTrigger manualTrigger;
                if (!manualTrigger.init(cfg, trigErr)) {
                    ok = false;
                } else {
                    ok = manualTrigger.fire(trigErr);
                }
            }
            if (stop->load() || windowPtr.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                windowPtr,
                [&, ok, trigErr, statusLabelPtr]() {
                    if (statusLabelPtr.isNull())
                        return;
                    if (ok) {
                        statusLabelPtr->setText("DAQ trigger sent.");
                        appState.daqAvailable = true;
                        appState.daqDisabled = false;
                        appState.daqFault = false;
                        appState.daqStatusText = "DAQ: available";
                        settingsController->setLabviewStatus("Connected", "#2ecc71");
                        updateForceTriggerState();
                    } else {
                        statusLabelPtr->setText("DAQ trigger failed: " + QString::fromStdString(trigErr));
                        appState.daqAvailable = false;
                        appState.daqFault = true;
                        appState.daqStatusText = "DAQ: unavailable";
                        appState.daqFaultText = QString::fromStdString(trigErr);
                        settingsController->setLabviewStatus("Disconnected", "#c0392b");
                        updateForceTriggerState();
                        logMessage(QString("Manual DAQ trigger failed: %1").arg(QString::fromStdString(trigErr)));
                    }
                },
                Qt::QueuedConnection);
        });
    };

    QObject::connect(labviewTestBtn, &QPushButton::clicked,
                     [&]() { runManualDaqTrigger(QStringLiteral("Internal manual DAQ trigger")); });
    QObject::connect(liveForceTriggerBtn, &QPushButton::clicked, [&]() {
        updateForceTriggerState();
        if (!liveForceTriggerBtn->isEnabled())
            return;
        runManualDaqTrigger(QStringLiteral("Live View Manual Trigger"));
    });

    QObject::connect(captureBtn, &QPushButton::clicked, [&]() {
        const QImage lastFrame = cameraController->lastFrame();
        if (lastFrame.isNull()) {
            statusLabel->setText("No frame to capture");
            return;
        }
        QString baseDir = savePathEdit->text();
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath();
        QDir dir(baseDir);
        dir.mkpath(".");
        QString fname = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz") + ".tiff";
        QString outPath = dir.filePath(fname);
        if (lastFrame.save(outPath, "TIFF")) {
            statusLabel->setText("Captured: " + fname);
            logLine("Captured frame to " + outPath);
        } else {
            statusLabel->setText("Capture failed");
        }
    });

    auto startSaving = [&]() {
        if (saving.load()) {
            statusLabel->setText("Already saving to disk");
            return;
        }
        recording = true;
        {
            QMutexLocker lk(saveMutex.get());
            saveBuffer->clear();
        }
        recordedFrames = 0;
        recordTimer.restart();
        recordStartTime = QDateTime::currentDateTime();
        saveStartBtn->setEnabled(false);
        saveStopBtn->setEnabled(true);
        logLine("Recording started");
        statusLabel->setText("Recording...");
        saveInfoLabel->setText("Elapsed: 0.0 s\nFrames: 0");
        saveInfoTimer.start();
    };

    auto stopSaving = [&]() {
        if (!recording.load())
            return;
        recording = false;
        saveStartBtn->setEnabled(true);
        saveStopBtn->setEnabled(false);
        saveInfoTimer.stop();

        std::shared_ptr<std::vector<QImage>> frames = std::make_shared<std::vector<QImage>>();
        {
            QMutexLocker lk(saveMutex.get());
            frames->swap(*saveBuffer);
        }
        if (frames->empty()) {
            statusLabel->setText("No frames to save");
            return;
        }

        QString baseDir = savePathEdit->text();
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath();
        QDir dir(baseDir);
        dir.mkpath(".");
        QString sub = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString outDir = dir.filePath(sub);
        dir.mkpath(outDir);

        saving = true;
        statusLabel->setText("Saving to disk...");
        logLine(QString("Saving %1 frames to %2").arg(frames->size()).arg(outDir));
        if (!savingDialog) {
            savingDialog = new QDialog(this);
            savingDialog->setWindowTitle("Saving...");
            savingDialog->setModal(true);
            auto layout = new QVBoxLayout(savingDialog);
            savingDialogLabel = new QLabel(savingDialog);
            savingProgress = new QProgressBar(savingDialog);
            savingProgress->setMinimum(0);
            layout->addWidget(savingDialogLabel);
            layout->addWidget(savingProgress);
            savingDialog->setLayout(layout);
        }
        int totalFrames = static_cast<int>(frames->size());
        savingDialogLabel->setText(QString("Saving %1 frames...").arg(totalFrames));
        savingProgress->setRange(0, totalFrames);
        savingProgress->setValue(0);
        savingDialog->show();

        FrameMeta metaCopy = cameraController->lastMeta();
        double expMsCopy = exposureSpin->value();
        QString recordStartStr = recordStartTime.toString("yyyy-MM-dd hh:mm:ss.zzz");
        QPointer<QLabel> statusLabelPtr(statusLabel);
        QPointer<QDialog> savingDialogPtr(savingDialog);
        QPointer<QProgressBar> savingProgressPtr(savingProgress);

        backgroundTasks.launch("capture-save-export", [frames, outDir, logLine, statusLabelPtr, savingDialogPtr,
                                                       savingProgressPtr, totalFrames, metaCopy, expMsCopy,
                                                       recordStartStr,
                                                       &saving](const BackgroundTaskRegistry::StopFlag& stop) {
            int width = std::max(6, static_cast<int>(std::ceil(std::log10(std::max<size_t>(1, frames->size())))));
            bool canceled = false;
            for (size_t i = 0; i < frames->size(); ++i) {
                if (stop->load()) {
                    canceled = true;
                    break;
                }
                const QImage& im = frames->at(i);
                QString fname = QString("%1.tiff").arg(static_cast<int>(i), width, 10, QChar('0'));
                QString path = outDir + "/" + fname;
                im.save(path, "TIFF");
                if (!savingProgressPtr.isNull() && (i % 100 == 0 || i + 1 == frames->size())) {
                    int v = static_cast<int>(i + 1);
                    QMetaObject::invokeMethod(
                        savingProgressPtr,
                        [savingProgressPtr, v]() {
                            if (!savingProgressPtr.isNull()) {
                                savingProgressPtr->setValue(v);
                            }
                        },
                        Qt::QueuedConnection);
                }
            }
            // Write metadata file
            QFile infoFile(outDir + "/capture_info.txt");
            if (!canceled && infoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts(&infoFile);
                ts << "Start: " << recordStartStr << "\n";
                ts << "Frames: " << frames->size() << "\n";
                ts << "Resolution: " << metaCopy.width << " x " << metaCopy.height << "\n";
                ts << "Binning: " << metaCopy.binning << "\n";
                ts << "Bits: " << metaCopy.bits << "\n";
                ts << "Exposure(ms): " << expMsCopy << "\n";
                ts << "Internal FPS: " << metaCopy.internalFps << "\n";
                ts << "Readout speed: " << metaCopy.readoutSpeed << "\n";
                ts.flush();
                infoFile.close();
            }
            logLine(canceled ? QString("Save canceled after partial export to %1").arg(outDir)
                             : QString("Saved %1 frames to %2").arg(frames->size()).arg(outDir));
            if (!statusLabelPtr.isNull()) {
                QMetaObject::invokeMethod(
                    statusLabelPtr,
                    [statusLabelPtr, canceled]() {
                        if (!statusLabelPtr.isNull()) {
                            statusLabelPtr->setText(canceled ? "Save canceled" : "Save complete");
                        }
                    },
                    Qt::QueuedConnection);
            }
            if (!savingDialogPtr.isNull()) {
                QMetaObject::invokeMethod(
                    savingDialogPtr,
                    [savingDialogPtr]() {
                        if (!savingDialogPtr.isNull()) {
                            savingDialogPtr->hide();
                        }
                    },
                    Qt::QueuedConnection);
            }
            saving = false;
        });
    };

    QObject::connect(saveStartBtn, &QPushButton::clicked, startSaving);
    QObject::connect(saveStopBtn, &QPushButton::clicked, stopSaving);

    QObject::connect(&saveInfoTimer, &QTimer::timeout, [&]() {
        if (!recording.load())
            return;
        double elapsed = recordTimer.isValid() ? recordTimer.elapsed() / 1000.0 : 0.0;
        saveInfoLabel->setText(QString("Elapsed: %1 s\nFrames: %2").arg(elapsed, 0, 'f', 1).arg(recordedFrames.load()));
    });

    auto updatePipelineStatus = [&](const PipelineEvent& evt, int bgRemaining, bool pipelineReady) {
        QMetaObject::invokeMethod(
            pipelineStatusLabel,
            [pipelineStatusLabel, &pipelineEnabled, evt, bgRemaining, pipelineReady]() {
                if (!pipelineEnabled.load()) {
                    pipelineStatusLabel->setText("Pipeline: paused");
                    return;
                }
                if (!pipelineReady) {
                    pipelineStatusLabel->setText("Pipeline: not loaded");
                    return;
                }
                if (bgRemaining > 0) {
                    pipelineStatusLabel->setText(QString("Pipeline: warming (%1 frames)").arg(bgRemaining));
                    return;
                }
                if (evt.fired) {
                    pipelineStatusLabel->setText(QString("Event: %1 (score %2) area=%3")
                                                     .arg(QString::fromStdString(evt.label))
                                                     .arg(evt.score, 0, 'f', 3)
                                                     .arg(evt.area, 0, 'f', 0));
                } else {
                    pipelineStatusLabel->setText("Pipeline: running");
                }
            },
            Qt::QueuedConnection);
    };

    auto buildClassText = [&](const QMap<QString, int>& counts) -> QString {
        if (counts.isEmpty())
            return "Classes:\n(none)";
        QStringList order = {"Empty", "Single", "MoreThanTwo", ">2", "2"};
        QSet<QString> used;
        QString text = "Classes:";
        for (const QString& name : order) {
            if (counts.contains(name)) {
                text += QString("\n%1: %2").arg(name).arg(counts.value(name));
                used.insert(name);
            }
        }
        for (auto it = counts.begin(); it != counts.end(); ++it) {
            if (used.contains(it.key()))
                continue;
            text += QString("\n%1: %2").arg(it.key()).arg(it.value());
        }
        return text;
    };

    auto makeStatsSnapshot = [&](const StatsTracker& s) -> StatsSnapshot {
        StatsSnapshot snap;
        snap.totalEvents = s.totalEvents;
        snap.classifiedHitCount = s.classifiedHitCount;
        snap.classifiedWasteCount = s.classifiedWasteCount;
        snap.wentToHitCount = s.wentToHitCount;
        snap.wentToWasteCount = s.wentToWasteCount;
        snap.eventActive = s.eventActive;
        snap.classText = buildClassText(s.classCounts);
        snap.classCounts = s.classCounts;
        snap.lastEventDir = s.lastEventDir;
        snap.lastEventLabel = s.lastEventLabel;
        snap.lastDecisionFrame = s.lastDecisionFrame;
        snap.lastDecisionEventId = s.lastDecisionEventId;
        if (!s.lastEventLabel.isEmpty()) {
            snap.lastText = QString("Last event: %1 (%2)").arg(s.lastEventLabel, s.lastEventDir);
        } else {
            snap.lastText = QString("Last event: --");
        }
        return snap;
    };

    auto getStatsSnapshot = [&]() -> StatsSnapshot {
        QMutexLocker lock(&statsMutex);
        return makeStatsSnapshot(stats);
    };

    auto buildStatsFigures = [&](const StatsSnapshot& snap) {
        int hit = snap.wentToHitCount;
        int waste = snap.wentToWasteCount;
        QImage hitWaste = renderPieChart("Went to Hit vs Waste", {"Went to Hit", "Went to Waste"},
                                         {static_cast<double>(hit), static_cast<double>(waste)},
                                         {QColor(46, 204, 113), QColor(192, 57, 43)});

        int empty = 0;
        int single = 0;
        int more = 0;
        for (auto it = snap.classCounts.begin(); it != snap.classCounts.end(); ++it) {
            QString label = it.key().trimmed().toLower();
            int count = it.value();
            if (label.contains("empty")) {
                empty += count;
            } else if (label.contains("single")) {
                single += count;
            } else if (label.contains("more") || label.contains(">") || label == "2") {
                more += count;
            } else if (!label.isEmpty() && label != "(unclassified)") {
                more += count;
            }
        }
        QImage classImg =
            renderPieChart("Class Distribution", {"0", "1", ">2"},
                           {static_cast<double>(empty), static_cast<double>(single), static_cast<double>(more)},
                           {QColor(52, 152, 219), QColor(241, 196, 15), QColor(155, 89, 182)});
        return std::pair<QImage, QImage>(hitWaste, classImg);
    };

    auto saveStatsFigures = [&](const QString& outDir, const QString& prefix, const StatsSnapshot& snap) -> bool {
        if (outDir.isEmpty())
            return false;
        auto figures = buildStatsFigures(snap);
        QDir out(outDir);
        out.mkpath(".");
        QString hitPath = out.filePath(prefix + "_hit_waste.png");
        QString clsPath = out.filePath(prefix + "_class_dist.png");
        bool ok1 = !figures.first.isNull() && figures.first.save(hitPath);
        bool ok2 = !figures.second.isNull() && figures.second.save(clsPath);
        return ok1 && ok2;
    };

    auto updateStatsFigureWindow = [&](const StatsSnapshot& snap) {
        if (!statsFigureWindow)
            return;
        auto figures = buildStatsFigures(snap);
        statsFigureWindow->setImages(figures.first, figures.second);
    };

    auto applyStatsSnapshot = [&](const StatsSnapshot& snap) {
        QMetaObject::invokeMethod(
            statsEventsLabel,
            [=]() {
                statsEventsLabel->setText(
                    QString("Events: %1  Active: %2").arg(snap.totalEvents).arg(snap.eventActive ? "Yes" : "No"));
                statsHitLabel->setText(QString("Classified Hit: %1\nClassified Waste: %2\nWent to Hit: %3\nWent to "
                                               "Waste: %4")
                                           .arg(snap.classifiedHitCount)
                                           .arg(snap.classifiedWasteCount)
                                           .arg(snap.wentToHitCount)
                                           .arg(snap.wentToWasteCount));
                statsClassLabel->setText(snap.classText);
                statsLastLabel->setText(snap.lastText);
            },
            Qt::QueuedConnection);
    };

    auto resetStats = [&]() {
        StatsSnapshot snap;
        {
            QMutexLocker lock(&statsMutex);
            stats = StatsTracker{};
            snap = makeStatsSnapshot(stats);
        }
        applyStatsSnapshot(snap);
    };

    auto showStatsFigures = [&]() {
        StatsSnapshot snap = getStatsSnapshot();
        auto figures = buildStatsFigures(snap);
        if (!statsFigureWindow) {
            statsFigureWindow = new StatsFigureWindow(this);
            statsFigureWindow->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(statsFigureWindow, &QObject::destroyed, [&]() { statsFigureWindow = nullptr; });
            QObject::connect(statsFigureWindow->saveButton(), &QPushButton::clicked, [&]() {
                QString outDir = outputEdit->text().trimmed();
                if (outDir.isEmpty())
                    outDir = QCoreApplication::applicationDirPath();
                QString dir = QFileDialog::getExistingDirectory(statsFigureWindow, "Select output directory", outDir);
                if (dir.isEmpty())
                    return;
                QString prefix = QDateTime::currentDateTime().toString("stats_yyyyMMdd_hhmmss");
                if (statsFigureWindow->saveImages(dir, prefix)) {
                    statusLabel->setText("Saved stats figures to " + dir);
                    logLine("Saved stats figures to " + dir);
                } else {
                    statusLabel->setText("Failed to save stats figures.");
                }
            });
        }
        statsFigureWindow->setImages(figures.first, figures.second);
        statsFigureWindow->show();
        statsFigureWindow->raise();
        statsFigureWindow->activateWindow();
    };

    auto endEventLocked = [&](StatsTracker& s, int decisionFrame) {
        if (!s.eventActive)
            return;
        QString dir = decideEventDirection(s.cumulativeDy, s.lastY, s.frameHeight, s.hasCentroid);
        if (dir == "Waste") {
            s.wentToWasteCount++;
        } else if (dir == "Hit") {
            s.wentToHitCount++;
        }
        s.lastEventDir = dir;
        s.lastEventLabel = s.currentLabel;
        s.lastDecisionFrame = decisionFrame;
        s.lastDecisionEventId = s.currentEventId;
        s.eventActive = false;
        s.hasCentroid = false;
        s.missCount = 0;
        s.currentLabel.clear();
        s.cumulativeDy = 0.0;
    };

    auto updateStatsFromEvent = [&](const PipelineEvent& evt, bool processed) {
        if (!processed)
            return;
        StatsSnapshot snap;
        {
            QMutexLocker lock(&statsMutex);
            if (evt.fired) {
                if (stats.eventActive) {
                    endEventLocked(stats, evt.frameNumber);
                }
                stats.eventActive = true;
                stats.missCount = 0;
                stats.currentEventId++;
                stats.startCentroid = evt.centroid;
                stats.lastCentroid = evt.centroid;
                stats.hasCentroid = true;
                stats.cumulativeDy = 0.0;
                stats.lastY = evt.centroid.y;
                stats.minY = evt.centroid.y;
                stats.maxY = evt.centroid.y;
                if (evt.frameHeight > 0)
                    stats.frameHeight = evt.frameHeight;
                stats.totalEvents++;
                QString label = QString::fromStdString(evt.label);
                if (label.isEmpty())
                    label = "(unclassified)";
                stats.currentLabel = label;
                if (evt.classified) {
                    stats.classCounts[label] = stats.classCounts.value(label) + 1;
                    if (evt.shouldTrigger) {
                        stats.classifiedHitCount++;
                    } else {
                        stats.classifiedWasteCount++;
                    }
                }
            } else if (evt.detected) {
                if (!stats.eventActive) {
                    stats.eventActive = true;
                    stats.missCount = 0;
                    stats.currentEventId++;
                    stats.startCentroid = evt.centroid;
                    stats.lastCentroid = evt.centroid;
                    stats.hasCentroid = true;
                    stats.cumulativeDy = 0.0;
                    stats.lastY = evt.centroid.y;
                    stats.minY = evt.centroid.y;
                    stats.maxY = evt.centroid.y;
                    if (evt.frameHeight > 0)
                        stats.frameHeight = evt.frameHeight;
                    stats.totalEvents++;
                    QString label = QString::fromStdString(evt.label);
                    if (label.isEmpty())
                        label = "(unclassified)";
                    stats.currentLabel = label;
                    if (evt.classified) {
                        stats.classCounts[label] = stats.classCounts.value(label) + 1;
                        if (evt.shouldTrigger) {
                            stats.classifiedHitCount++;
                        } else {
                            stats.classifiedWasteCount++;
                        }
                    }
                } else {
                    stats.cumulativeDy += static_cast<double>(evt.centroid.y - stats.lastCentroid.y);
                    stats.lastCentroid = evt.centroid;
                    stats.hasCentroid = true;
                    stats.lastY = evt.centroid.y;
                    stats.minY = std::min(stats.minY, static_cast<double>(evt.centroid.y));
                    stats.maxY = std::max(stats.maxY, static_cast<double>(evt.centroid.y));
                    if (evt.frameHeight > 0)
                        stats.frameHeight = evt.frameHeight;
                    stats.missCount = 0;
                }
            } else if (stats.eventActive) {
                stats.missCount++;
                if (stats.missCount >= pipelineDetectCfg.resetFrames) {
                    endEventLocked(stats, evt.frameNumber);
                }
            }
            snap = makeStatsSnapshot(stats);
        }
        applyStatsSnapshot(snap);
    };

    auto processPipelineFrame = [&](const QImage& img, PipelineEvent& evt, int& bgRemaining, bool& pipelineReady,
                                    double* procMsOut) -> bool {
        bgRemaining = 0;
        pipelineReady = false;
        if (!pipelineEnabled.load() || img.isNull())
            return false;

        QImage lutImg = cameraController->applyLutToImage(img);
        cv::Mat gray(lutImg.height(), lutImg.width(), CV_8UC1, const_cast<uchar*>(lutImg.bits()),
                     lutImg.bytesPerLine());
        cv::Mat grayCopy = gray.clone();

        auto t0 = std::chrono::steady_clock::now();
        bool processed = false;
        {
            QMutexLocker lock(&pipelineMutex);
            pipelineReady = pipeline.isReady();
            if (pipelineReady) {
                processed = pipeline.processFrame(grayCopy, evt);
                bgRemaining = pipeline.backgroundFramesRemaining();
            }
        }
        auto t1 = std::chrono::steady_clock::now();
        if (procMsOut) {
            *procMsOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
        }

        updatePipelineStatus(evt, bgRemaining, pipelineReady);
        updateStatsFromEvent(evt, processed);
        return processed;
    };

    auto currentModelLogFields = [&]() -> RuntimeModelLogFields {
        RuntimeModelLogFields fields;
        fields.registryEntryId = liveModelCombo->currentData(kLiveModelIdRole).toString();
        fields.modelStateAtStart = liveModelCombo->currentData(kLiveModelStateRole).toString();
        fields.liveUseMode = liveModelCombo->currentData(kLiveModelModeRole).toString();
        fields.modelSha256 = liveModelCombo->currentData(kLiveModelOnnxHashRole).toString();
        fields.metadataSha256 = liveModelCombo->currentData(kLiveModelMetadataHashRole).toString();
        return fields;
    };

    startLiveLogging = [&]() {
        QMutexLocker lock(&liveLogMutex);
        liveLog.clear();
        liveLogStart = QDateTime::currentDateTime();
        {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.reset(resetFramesSpin->value());
        }
        liveLogging.store(true);
    };

    stopLiveLogging = [&]() {
        if (!liveLogging.exchange(false))
            return;
        std::vector<LiveLogRecord> records;
        {
            QMutexLocker lock(&liveLogMutex);
            records = liveLog;
        }
        std::vector<SequenceEventRecord> liveEvents;
        {
            QMutexLocker eventLock(&liveEventMutex);
            liveEventTracker.finalize();
            liveEvents = liveEventTracker.events;
        }
        StatsSnapshot snap = getStatsSnapshot();
        QString outDir = outputEdit->text().trimmed();
        if (outDir.isEmpty())
            outDir = QCoreApplication::applicationDirPath();
        QString timestamp = liveLogStart.isValid() ? liveLogStart.toString("yyyyMMdd_hhmmss")
                                                   : QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString prefix = "live_" + timestamp;
        QString logPath = writeLiveLogCsv(outDir, prefix, records);
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        QString targetLabel = selectedTargetClassId();
        QString daqChannel = daqChannelEdit->text().trimmed();
        double daqAmp = amplitudeSpin->value();
        double daqFreqHz = freqSpin->value() * 1000.0;
        double daqDuration = durationSpin->value();
        double daqDelay = delaySpin->value();
        int frameSkip = frameSkipSpin->value();
        int bgFrames = bgFramesSpin->value();
        int bgUpdate = bgUpdateSpin->value();
        int resetFrames = resetFramesSpin->value();
        double minArea = minAreaSpin->value();
        double minAreaFrac = minAreaFracSpin->value();
        double maxAreaFrac = maxAreaFracSpin->value();
        int minBbox = minBboxSpin->value();
        int margin = marginSpin->value();
        int diffThresh = diffThreshSpin->value();
        int blurRadius = blurRadiusSpin->value();
        int morphRadius = morphRadiusSpin->value();
        double scale = scaleSpin->value();
        int gapFireShift = gapFireSpin->value();
        int displayEvery = std::max(1, displayEverySpin->value());
        double avgFps = 0.0;
        int fpsCount = 0;
        for (const auto& rec : records) {
            if (rec.fps > 0.0) {
                avgFps += rec.fps;
                fpsCount++;
            }
        }
        if (fpsCount > 0) {
            avgFps /= fpsCount;
        }
        SequenceLogMetadata liveSequenceLogMetadata;
        liveSequenceLogMetadata.displayEvery = displayEvery;
        liveSequenceLogMetadata.onnxResolved = onnxResolved;
        liveSequenceLogMetadata.metadataResolved = metaResolved;
        liveSequenceLogMetadata.targetLabel = targetLabel;
        liveSequenceLogMetadata.model = currentModelLogFields();
        liveSequenceLogMetadata.frameSkip = frameSkip;
        liveSequenceLogMetadata.bgFrames = bgFrames;
        liveSequenceLogMetadata.bgUpdate = bgUpdate;
        liveSequenceLogMetadata.resetFrames = resetFrames;
        liveSequenceLogMetadata.minArea = minArea;
        liveSequenceLogMetadata.minAreaFrac = minAreaFrac;
        liveSequenceLogMetadata.maxAreaFrac = maxAreaFrac;
        liveSequenceLogMetadata.minBbox = minBbox;
        liveSequenceLogMetadata.margin = margin;
        liveSequenceLogMetadata.diffThresh = diffThresh;
        liveSequenceLogMetadata.blurRadius = blurRadius;
        liveSequenceLogMetadata.morphRadius = morphRadius;
        liveSequenceLogMetadata.scale = scale;
        liveSequenceLogMetadata.gapFireShift = gapFireShift;
        liveSequenceLogMetadata.daqChannel = daqChannel;
        liveSequenceLogMetadata.daqAmplitude = daqAmp;
        liveSequenceLogMetadata.daqFrequencyHz = daqFreqHz;
        liveSequenceLogMetadata.daqDurationMs = daqDuration;
        liveSequenceLogMetadata.daqDelayMs = daqDelay;
        QString seqLogPath = writeLiveSequenceLog(outDir, timestamp, records, liveSequenceLogMetadata);
        QString trajPath =
            writeEventTrajectoryCsv(outDir, "sequence_event_trajectory_live_" + timestamp + ".csv", liveEvents);
        SequenceSummaryMetadata liveSummaryMetadata;
        liveSummaryMetadata.targetLabel = targetLabel;
        liveSummaryMetadata.totalFrames = static_cast<int>(records.size());
        liveSummaryMetadata.fps = avgFps;
        liveSummaryMetadata.outputDir = outDir;
        liveSummaryMetadata.onnxResolved = onnxResolved;
        liveSummaryMetadata.metadataResolved = metaResolved;
        liveSummaryMetadata.model = liveSequenceLogMetadata.model;
        QString summaryPath = writeSequenceSummaryCsv(outDir, "sequence_summary_live_" + timestamp + ".csv", liveEvents,
                                                      liveSummaryMetadata);
        saveStatsFigures(outDir, prefix, snap);
        updateStatsFigureWindow(snap);
        if (!logPath.isEmpty()) {
            QString status = "Pipeline stopped. Log: " + logPath;
            if (!seqLogPath.isEmpty()) {
                status += "\nSequence log: " + seqLogPath;
            }
            if (!summaryPath.isEmpty()) {
                status += "\nSummary: " + summaryPath;
            }
            statusLabel->setText(status);
            logLine("Saved live pipeline log to " + logPath);
            if (!seqLogPath.isEmpty()) {
                logLine("Saved live sequence log to " + seqLogPath);
            }
            if (!trajPath.isEmpty()) {
                logLine("Saved live event trajectory to " + trajPath);
            }
            if (!summaryPath.isEmpty()) {
                logLine("Saved live sequence summary to " + summaryPath);
            }
        } else {
            statusLabel->setText("Pipeline stopped. Failed to write log.");
        }
    };

    QObject::connect(statsResetBtn, &QPushButton::clicked, resetStats);
    QObject::connect(statsShowBtn, &QPushButton::clicked, showStatsFigures);
    QObject::connect(runStateResetButton, &QPushButton::clicked, resetStats);

    QObject::connect(seqStartBtn, &QPushButton::clicked, [&]() {
        if (sequenceRunning.load())
            return;
        std::shared_ptr<std::vector<SequenceFrame>> frames;
        {
            QMutexLocker lock(&sequenceMutex);
            frames = sequenceFrames;
        }
        if (!frames || frames->empty()) {
            seqStatusLabel->setText("No sequence loaded.");
            return;
        }
        double fps = seqFpsSpin->value();
        if (fps <= 0.0) {
            seqStatusLabel->setText("FPS must be greater than 0.");
            return;
        }

        if (liveLogging.load()) {
            stopLiveLogging();
        }
        QString runDir = buildRunOutputDir("sequence");
        if (!runDir.isEmpty()) {
            outputEdit->setText(runDir);
            writeRuntimeSettingsSnapshot(runDir, "sequence");
            reportsWorkspaceController.setCurrentRunDir(runDir);
        }
        sequencePrevPipelineChecked = pipelineEnableCheck->isChecked();
        sequenceStarting.store(true);
        loadPipeline(true, true);
        bool pipelineReady = false;
        bool triggerReady = false;
        {
            QMutexLocker lock(&pipelineMutex);
            pipelineReady = pipeline.isReady();
            triggerReady = pipeline.isTriggerReady();
        }
        sequenceStarting.store(false);
        if (!pipelineReady) {
            seqStatusLabel->setText("Pipeline not ready. Fix settings and load pipeline.");
            return;
        }
        if (triggerReady) {
            pipelineEnabled.store(false);
            pipelineEnableCheck->setChecked(false);
            pipelineStartBtn->setEnabled(false);
            pipelineStopBtn->setEnabled(false);
            updateLiveRunStartStopVisibility();
            updateForceTriggerState();
            runStatusItem->setText("Run: idle");
            pipelineStatusLabel->setText("Pipeline: paused");
            statusLabel->setText("Sequence replay blocked: DAQ trigger path is still armed.");
            seqStatusLabel->setText("Sequence replay blocked: DAQ trigger path is still armed.");
            this->statusBar()->showMessage("Sequence replay blocked: DAQ trigger path armed");
            reportsWorkspaceController.refreshOpenRunAvailability();
            logMessage("Sequence replay blocked because replay pipeline reported DAQ trigger-ready.");
            return;
        }
        if (sequenceThread.joinable()) {
            sequenceThread.join();
        }
        sequenceStop.store(false);
        sequenceRunning.store(true);
        validatorWorkspaceController->setSequenceUiRunning(true);

        if (!viewerOnly) {
            QMetaObject::invokeMethod(
                cameraWorker, [cameraWorker]() { cameraWorker->stopCapture(); }, Qt::BlockingQueuedConnection);
        }
        statusLabel->setText("Sequence test running.");
        if (pipeline.isReady()) {
            QMutexLocker lock(&pipelineMutex);
            pipeline.reset();
            pipelineStatusLabel->setText("Pipeline: warming (sequence start)");
        }

        QString outDir = outputEdit->text().trimmed();
        if (outDir.isEmpty()) {
            outDir = QCoreApplication::applicationDirPath();
        }
        QString onnxResolved = resolveAppRelative(onnxEdit->text());
        QString metaResolved = resolveAppRelative(metaEdit->text());
        QString targetLabel = selectedTargetClassId();
        QString seqFolder = seqFolderEdit->text().trimmed();
        QString daqChannel = daqChannelEdit->text().trimmed();
        double daqAmp = amplitudeSpin->value();
        double daqFreqHz = freqSpin->value() * 1000.0;
        double daqDuration = durationSpin->value();
        double daqDelay = delaySpin->value();
        QDir out(outDir);
        out.mkpath(".");
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString logPath = out.filePath("sequence_test_log_" + timestamp + ".csv");
        seqLogLabel->setText("Log: " + logPath);
        seqStatusLabel->setText(QString("Running %1 frames at %2 fps...").arg(frames->size()).arg(fps, 0, 'f', 2));

        int displayEvery = std::max(1, displayEverySpin->value());

        int frameSkip = frameSkipSpin->value();
        int bgFrames = bgFramesSpin->value();
        int bgUpdate = bgUpdateSpin->value();
        int resetFrames = resetFramesSpin->value();
        double minArea = minAreaSpin->value();
        double minAreaFrac = minAreaFracSpin->value();
        double maxAreaFrac = maxAreaFracSpin->value();
        int minBbox = minBboxSpin->value();
        int margin = marginSpin->value();
        int diffThresh = diffThreshSpin->value();
        int blurRadius = blurRadiusSpin->value();
        int morphRadius = morphRadiusSpin->value();
        double scale = scaleSpin->value();
        int gapFireShift = gapFireSpin->value();

        SequenceLogMetadata sequenceLogMetadata;
        sequenceLogMetadata.sequenceFolder = seqFolder;
        sequenceLogMetadata.fps = fps;
        sequenceLogMetadata.frameCount = static_cast<int>(frames->size());
        sequenceLogMetadata.displayEvery = displayEvery;
        sequenceLogMetadata.outputDir = outDir;
        sequenceLogMetadata.onnxResolved = onnxResolved;
        sequenceLogMetadata.metadataResolved = metaResolved;
        sequenceLogMetadata.targetLabel = targetLabel;
        sequenceLogMetadata.pipelineEnabledBefore = sequencePrevPipelineChecked;
        sequenceLogMetadata.pipelineForced = !sequencePrevPipelineChecked;
        sequenceLogMetadata.frameSkip = frameSkip;
        sequenceLogMetadata.bgFrames = bgFrames;
        sequenceLogMetadata.bgUpdate = bgUpdate;
        sequenceLogMetadata.resetFrames = resetFrames;
        sequenceLogMetadata.minArea = minArea;
        sequenceLogMetadata.minAreaFrac = minAreaFrac;
        sequenceLogMetadata.maxAreaFrac = maxAreaFrac;
        sequenceLogMetadata.minBbox = minBbox;
        sequenceLogMetadata.margin = margin;
        sequenceLogMetadata.diffThresh = diffThresh;
        sequenceLogMetadata.blurRadius = blurRadius;
        sequenceLogMetadata.morphRadius = morphRadius;
        sequenceLogMetadata.scale = scale;
        sequenceLogMetadata.gapFireShift = gapFireShift;
        sequenceLogMetadata.daqChannel = daqChannel;
        sequenceLogMetadata.daqAmplitude = daqAmp;
        sequenceLogMetadata.daqFrequencyHz = daqFreqHz;
        sequenceLogMetadata.daqDurationMs = daqDuration;
        sequenceLogMetadata.daqDelayMs = daqDelay;

        SequenceSummaryMetadata sequenceSummaryMetadata;
        sequenceSummaryMetadata.targetLabel = targetLabel;
        sequenceSummaryMetadata.totalFrames = static_cast<int>(frames->size());
        sequenceSummaryMetadata.fps = fps;
        sequenceSummaryMetadata.sequenceFolder = seqFolder;
        sequenceSummaryMetadata.outputDir = outDir;
        sequenceSummaryMetadata.onnxResolved = onnxResolved;
        sequenceSummaryMetadata.metadataResolved = metaResolved;
        sequenceSummaryMetadata.model = currentModelLogFields();

        sequenceThread = std::thread([&, frames, fps, displayEvery, logPath, outDir, timestamp, sequenceLogMetadata,
                                      sequenceSummaryMetadata, resetFrames]() {
            SequenceLogWriter sequenceLogWriter;
            if (!sequenceLogWriter.open(logPath, sequenceLogMetadata)) {
                validatorWorkspaceController->updateSequenceStatus("Failed to open sequence log.");
                sequenceRunning.store(false);
                QMetaObject::invokeMethod(
                    this,
                    [&, logPath]() {
                        validatorWorkspaceController->setSequenceUiRunning(false);
                        statusLabel->setText("Sequence test failed (log open).");
                        seqLogLabel->setText("Log: " + logPath);
                    },
                    Qt::QueuedConnection);
                return;
            }

            SequenceEventTracker tracker;
            tracker.reset(resetFrames);

            using clock = std::chrono::steady_clock;
            auto start = clock::now();
            std::chrono::duration<double> period(1.0 / fps);

            for (size_t i = 0; i < frames->size(); ++i) {
                if (sequenceStop.load())
                    break;
                auto target = start + period * static_cast<double>(i);
                while (!sequenceStop.load()) {
                    auto now = clock::now();
                    if (now >= target)
                        break;
                    auto remaining = target - now;
                    if (remaining > std::chrono::milliseconds(2)) {
                        std::this_thread::sleep_for(remaining - std::chrono::milliseconds(1));
                    } else {
                        std::this_thread::yield();
                    }
                }
                if (sequenceStop.load())
                    break;

                const SequenceFrame& frame = frames->at(i);
                double scheduledMs = std::chrono::duration<double, std::milli>(period * static_cast<double>(i)).count();
                double actualMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
                double jitterMs = actualMs - scheduledMs;
                QString wallTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

                FrameMeta meta;
                meta.width = frame.image.width();
                meta.height = frame.image.height();
                meta.bits = 8;
                meta.binning = 1.0;
                meta.frameIndex = static_cast<qint64>(i);
                meta.delivered = static_cast<qint64>(i + 1);
                meta.dropped = 0;
                meta.internalFps = fps;

                PipelineEvent evt;
                int bgRemaining = 0;
                bool pipelineReady = false;
                double procMs = 0.0;
                bool processed = processPipelineFrame(frame.image, evt, bgRemaining, pipelineReady, &procMs);
                bool enabledNow = pipelineEnabled.load();
                QString skipReason;
                if (!enabledNow) {
                    skipReason = "pipeline_disabled";
                } else if (!pipelineReady) {
                    skipReason = "pipeline_not_ready";
                } else if (!processed) {
                    skipReason = "frame_skipped";
                }

                tracker.update(evt, processed);

                if (displayEvery > 0 && (static_cast<int>(i) % displayEvery == 0)) {
                    QImage imgCopy = cameraController->applyLutToImage(frame.image);
                    QMetaObject::invokeMethod(
                        this,
                        [&, imgCopy, meta, i, fps, frames]() {
                            imageView->setImage(imgCopy);
                            cameraController->storeLastFrame(imgCopy, meta);
                            statsLabel->setText(
                                QString("Source: Sequence\nResolution: %1 x %2\nBits: %3\nFPS: %4\nFrame: %5 / %6")
                                    .arg(meta.width)
                                    .arg(meta.height)
                                    .arg(meta.bits)
                                    .arg(fps, 0, 'f', 2)
                                    .arg(i + 1)
                                    .arg(frames->size()));
                        },
                        Qt::QueuedConnection);
                }

                QString cropPath = QString::fromStdString(evt.cropPath);
                QString label = QString::fromStdString(evt.label);
                SequenceLogFrameRow row;
                row.index = static_cast<int>(i);
                row.filename = QFileInfo(frame.path).fileName();
                row.scheduledMs = scheduledMs;
                row.actualMs = actualMs;
                row.jitterMs = jitterMs;
                row.wallTime = wallTime;
                row.procMs = procMs;
                row.processed = processed;
                row.pipelineEnabled = enabledNow;
                row.pipelineReady = pipelineReady;
                row.bgRemaining = bgRemaining;
                row.skipReason = skipReason;
                row.detected = evt.detected;
                row.fired = evt.fired;
                row.area = evt.area;
                row.bboxX = evt.bbox.x;
                row.bboxY = evt.bbox.y;
                row.bboxW = evt.bbox.width;
                row.bboxH = evt.bbox.height;
                row.cropX = evt.cropRect.x;
                row.cropY = evt.cropRect.y;
                row.cropW = evt.cropRect.width;
                row.cropH = evt.cropRect.height;
                row.cropPath = cropPath;
                row.label = label;
                row.score = evt.score;
                row.triggered = evt.triggered;
                row.triggerOk = evt.triggerOk;
                row.frameNumber = evt.frameNumber;
                row.eventDir = tracker.lastEventDir;
                row.decisionFrame = tracker.lastDecisionFrame;
                row.decisionEventId = tracker.lastDecisionEventId;
                sequenceLogWriter.writeFrame(row);
                if (i % 50 == 0) {
                    sequenceLogWriter.flush();
                }
            }

            tracker.finalize();
            QString trajPath =
                writeEventTrajectoryCsv(outDir, "sequence_event_trajectory_" + timestamp + ".csv", tracker.events);
            QString summaryPath = writeSequenceSummaryCsv(outDir, "sequence_summary_" + timestamp + ".csv",
                                                          tracker.events, sequenceSummaryMetadata);

            sequenceLogWriter.close();

            sequenceRunning.store(false);
            QMetaObject::invokeMethod(
                this,
                [&, logPath, trajPath, summaryPath]() {
                    validatorWorkspaceController->setSequenceUiRunning(false);
                    seqStatusLabel->setText("Sequence finished.");
                    statusLabel->setText("Sequence test finished.");
                    QString logText = "Log: " + logPath;
                    if (!trajPath.isEmpty()) {
                        logText += "\nTrajectory: " + trajPath;
                    }
                    if (!summaryPath.isEmpty()) {
                        logText += "\nSummary: " + summaryPath;
                    }
                    seqLogLabel->setText(logText);
                },
                Qt::QueuedConnection);
        });
    });

    cameraWorker->setRecordHook([saveMutex, saveBuffer, &recording, &recordedFrames, &pipelineEnabled, &sequenceRunning,
                                 &processPipelineFrame, &liveLogging, &liveLogMutex, &liveLog, &getStatsSnapshot,
                                 &liveLogStart, &datasetCaptureActive, &datasetCaptureMutex, &datasetCaptureSession,
                                 &datasetCaptureDir, &datasetCaptureManifestPath, datasetStartCaptureBtn,
                                 datasetStopCaptureBtn, datasetCaptureStatusLabel, statusLabel, trainerDatasetEdit,
                                 &openDatasetLabelerPath, this](const QImage& img, const FrameMeta& meta, double fps) {
        if (recording.load()) {
            QMutexLocker lk(saveMutex.get());
            saveBuffer->push_back(img.copy());
            recordedFrames++;
        }

        if (sequenceRunning.load())
            return;

        PipelineEvent evt;
        int bgRemaining = 0;
        bool pipelineReady = false;
        double procMs = 0.0;
        bool processed = processPipelineFrame(img, evt, bgRemaining, pipelineReady, &procMs);

        if (datasetCaptureActive.load() && processed && evt.fired && evt.classified && !evt.cropPath.empty()) {
            bool reachedTarget = false;
            std::size_t collected = 0;
            std::size_t target = 0;
            QString addError;
            {
                QMutexLocker captureLock(&datasetCaptureMutex);
                if (datasetCaptureActive.load()) {
                    DatasetCropCandidate candidate;
                    candidate.sourceType = "live_stream";
                    candidate.sourceSequenceId = "live_camera";
                    candidate.sourceFrameIndex = static_cast<int>(meta.frameIndex);
                    candidate.eventId = liveEventTracker.currentEventId;
                    candidate.classificationFrame = static_cast<int>(evt.frameNumber);
                    candidate.cropX = evt.cropRect.x;
                    candidate.cropY = evt.cropRect.y;
                    candidate.cropW = evt.cropRect.width;
                    candidate.cropH = evt.cropRect.height;
                    candidate.bboxX = evt.bbox.x;
                    candidate.bboxY = evt.bbox.y;
                    candidate.bboxW = evt.bbox.width;
                    candidate.bboxH = evt.bbox.height;
                    candidate.predictedClassId = evt.label;
                    candidate.predictedLabel = evt.label;
                    candidate.confidence = evt.score;
                    candidate.sourceCropPath = evt.cropPath;
                    std::string err;
                    if (!datasetCaptureSession.addCrop(candidate, err)) {
                        addError = QString::fromStdString(err);
                        datasetCaptureSession.setStopReason("error");
                        datasetCaptureSession.finalize(err);
                        datasetCaptureActive.store(false);
                    } else {
                        reachedTarget = datasetCaptureSession.targetReached();
                        collected = datasetCaptureSession.collectedCount();
                        target = datasetCaptureSession.currentBatchTarget();
                    }
                }
            }
            if (!addError.isEmpty()) {
                QMetaObject::invokeMethod(
                    this,
                    [&, addError]() {
                        datasetStartCaptureBtn->setEnabled(true);
                        datasetStopCaptureBtn->setEnabled(false);
                        datasetCaptureStatusLabel->setText("Dataset Builder capture stopped after error: " + addError);
                        statusLabel->setText("Dataset Builder capture stopped after error.");
                    },
                    Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(
                    this,
                    [&, collected, target]() {
                        datasetCaptureStatusLabel->setText(QString("Dataset Builder capture active: %1 / %2 crops\n%3")
                                                               .arg(static_cast<qulonglong>(collected))
                                                               .arg(static_cast<qulonglong>(target))
                                                               .arg(datasetCaptureDir));
                    },
                    Qt::QueuedConnection);
            }
            if (reachedTarget) {
                QMetaObject::invokeMethod(
                    this,
                    [&, collected]() {
                        QMessageBox prompt(this);
                        prompt.setWindowTitle("Dataset Builder Batch Target Reached");
                        prompt.setText(
                            QString("Dataset Builder collected %1 crops. Continue collecting or stop and review?")
                                .arg(static_cast<qulonglong>(collected)));
                        QPushButton* continueButton = prompt.addButton("Continue Collecting", QMessageBox::AcceptRole);
                        QPushButton* reviewButton = prompt.addButton("Stop and Review", QMessageBox::RejectRole);
                        prompt.exec();
                        bool continueCollecting = (prompt.clickedButton() == continueButton);
                        {
                            QMutexLocker captureLock(&datasetCaptureMutex);
                            if (datasetCaptureActive.load()) {
                                if (continueCollecting) {
                                    datasetCaptureSession.extendBatchTarget();
                                } else {
                                    datasetCaptureSession.recordBatchPrompt("stop_for_review");
                                    datasetCaptureSession.setStopReason("user_stop_after_batch_prompt");
                                    std::string err;
                                    datasetCaptureSession.finalize(err);
                                    datasetCaptureActive.store(false);
                                }
                            }
                        }
                        if (continueCollecting) {
                            datasetCaptureStatusLabel->setText(
                                QString("Dataset Builder capture continuing to %1 crops\n%2")
                                    .arg(static_cast<qulonglong>(datasetCaptureSession.currentBatchTarget()))
                                    .arg(datasetCaptureDir));
                        } else {
                            datasetStartCaptureBtn->setEnabled(true);
                            datasetStopCaptureBtn->setEnabled(false);
                            datasetCaptureStatusLabel->setText(
                                "Dataset Builder capture stopped for review.\nManifest: " + datasetCaptureManifestPath);
                            trainerDatasetEdit->setText(datasetCaptureDir);
                            if (QFileInfo::exists(datasetCaptureManifestPath)) {
                                openDatasetLabelerPath(datasetCaptureManifestPath);
                            }
                        }
                    },
                    Qt::BlockingQueuedConnection);
            }
        }

        if (liveLogging.load()) {
            QString lastEventDir;
            int lastDecisionFrame = -1;
            int lastDecisionEventId = 0;
            {
                QMutexLocker eventLock(&liveEventMutex);
                liveEventTracker.update(evt, processed);
                lastEventDir = liveEventTracker.lastEventDir;
                lastDecisionFrame = liveEventTracker.lastDecisionFrame;
                lastDecisionEventId = liveEventTracker.lastDecisionEventId;
            }
            bool enabledNow = pipelineEnabled.load();
            QString skipReason;
            if (!enabledNow) {
                skipReason = "pipeline_disabled";
            } else if (!pipelineReady) {
                skipReason = "pipeline_not_ready";
            } else if (!processed) {
                skipReason = "frame_skipped";
            }

            LiveLogRecord rec;
            rec.wallTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
            rec.elapsedMs = liveLogStart.isValid() ? liveLogStart.msecsTo(QDateTime::currentDateTime()) : 0;
            rec.frameIndex = meta.frameIndex;
            rec.delivered = meta.delivered;
            rec.dropped = meta.dropped;
            rec.fps = fps;
            rec.camFps = meta.internalFps;
            rec.procMs = procMs;
            rec.processed = processed;
            rec.pipelineEnabled = enabledNow;
            rec.pipelineReady = pipelineReady;
            rec.skipReason = skipReason;
            rec.bgRemaining = bgRemaining;
            rec.detected = evt.detected;
            rec.fired = evt.fired;
            rec.area = evt.area;
            rec.bboxX = evt.bbox.x;
            rec.bboxY = evt.bbox.y;
            rec.bboxW = evt.bbox.width;
            rec.bboxH = evt.bbox.height;
            rec.cropX = evt.cropRect.x;
            rec.cropY = evt.cropRect.y;
            rec.cropW = evt.cropRect.width;
            rec.cropH = evt.cropRect.height;
            rec.cropPath = QString::fromStdString(evt.cropPath);
            rec.label = QString::fromStdString(evt.label);
            rec.score = evt.score;
            rec.triggered = evt.triggered;
            rec.triggerOk = evt.triggerOk;
            StatsSnapshot snap = getStatsSnapshot();
            rec.eventDir = lastEventDir;
            rec.decisionFrame = lastDecisionFrame;
            rec.decisionEventId = lastDecisionEventId;
            rec.hitCount = snap.wentToHitCount;
            rec.wasteCount = snap.wentToWasteCount;
            rec.classifiedHitCount = snap.classifiedHitCount;
            rec.classifiedWasteCount = snap.classifiedWasteCount;
            rec.wentToHitCount = snap.wentToHitCount;
            rec.wentToWasteCount = snap.wentToWasteCount;
            QMutexLocker lk(&liveLogMutex);
            liveLog.push_back(rec);
        }
    });

    QObject::connect(
        cameraWorker, &CameraWorker::frameReady, this,
        [&](const QImage& img, FrameMeta meta, double fps) {
            cameraController->applyFrameToPreviewWorkspaces(img, meta, fps);
        },
        Qt::QueuedConnection);

    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        backgroundTasks.requestStop();
        sequenceStop.store(true);
        recording.store(false);
        if (datasetCaptureActive.load()) {
            QMutexLocker lock(&datasetCaptureMutex);
            datasetCaptureSession.setStopReason("cancelled");
            std::string err;
            datasetCaptureSession.finalize(err);
            datasetCaptureActive.store(false);
        }
        if (trainerProcess && trainerProcess->state() != QProcess::NotRunning) {
            trainerProcess->terminate();
            if (!trainerProcess->waitForFinished(2500)) {
                trainerProcess->kill();
                trainerProcess->waitForFinished(1000);
            }
        }
        validatorWorkspaceController->stopSequenceTest();
        backgroundTasks.waitAll();
        stopLiveLogging();
        cameraController->shutdownCameraThread(cameraThread);
        logMessage("Exiting application");
    });

    cameraWorker->moveToThread(&cameraThread);
    QObject::connect(&cameraThread, &QThread::finished, cameraWorker, &QObject::deleteLater);
    cameraThread.start();

    Q_UNUSED(splashTimer);
    app.processEvents();
    QThread::msleep(700);
    app.processEvents();
    this->showMaximized();
    splash.finish(this);
    if (options.verifyDirectDaqManualTrigger || options.verifyLiveViewManualTrigger) {
        logMessage("Manual DAQ verifier: camera startup skipped while preserving hardware-required DAQ checks.");
    } else {
        cameraController->initializeCamera();
    }
    if (!options.verifyDirectDaqManualTrigger && !options.verifyLiveViewManualTrigger) {
        QTimer::singleShot(0, [&]() { loadPipeline(false, false); });
    }
    if (!options.datasetBuilderReviewPath.trimmed().isEmpty()) {
        QTimer::singleShot(0, [&]() {
            logMessage("Opening Dataset Builder review manifest from command line: " +
                       options.datasetBuilderReviewPath);
            openDatasetLabelerPath(options.datasetBuilderReviewPath);
        });
    }
    if (options.verifyCameraWorkspace) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto waitUntil = [&](int timeoutMs, const std::function<bool()>& predicate) {
                QElapsedTimer timer;
                timer.start();
                while (timer.elapsed() < timeoutMs) {
                    app.processEvents();
                    if (predicate()) {
                        return true;
                    }
                    waitForUi(100);
                }
                app.processEvents();
                return predicate();
            };

            workspaceStack->setCurrentWidget(liveWorkspacePage);
            liveNavButton->setChecked(true);
            headerTitleLabel->setText("/ Live View");
            headerStatusText->setText("Live View workspace");
            rightScroll->setMinimumWidth(336);
            rightScroll->setMaximumWidth(336);
            mainSplitter->setSizes({qMax(760, this->width() - 336), 336});
            app.processEvents();

            require(workspaceStack->currentWidget() == liveWorkspacePage,
                    "Live View workspace opens as the active page");
            require(liveNavButton && liveNavButton->toolTip() == "Live View",
                    "Live navigation tooltip reads Live View");
            require(headerTitleLabel && headerTitleLabel->text() == "/ Live View", "Header title reads / Live View");
            require(headerStatusText && headerStatusText->text() == "Live View workspace",
                    "Header status reads Live View workspace");
            require(!liveNavButton->text().contains("Live Sorting"),
                    "Live navigation label no longer shows Live Sorting");
            require(!headerTitleLabel->text().contains("Live Sorting"), "Header title no longer shows Live Sorting");
            require(!headerStatusText->text().contains("Live Sorting"), "Header status no longer shows Live Sorting");

            auto* cameraWorkspace = this->findChild<QWidget*>("CameraWorkspace");
            auto* cameraControlsStack = this->findChild<QWidget*>("CameraControlsStack");
            auto* cameraFormatPanel = this->findChild<QWidget*>("CameraFormatSpeedPanel");
            auto* cameraFormatPanelFrame = this->findChild<QWidget*>("CameraFormatSpeedPanelFrame");
            auto* cameraLutPanel = this->findChild<QWidget*>("CameraLutDisplayPanel");
            auto* cameraLutPanelFrame = this->findChild<QWidget*>("CameraLutDisplayPanelFrame");
            auto* cameraRecordingPanel = this->findChild<QWidget*>("CameraRecordingPanel");
            auto* cameraRecordingPanelFrame = this->findChild<QWidget*>("CameraRecordingPanelFrame");
            auto* cameraSequencePanel = this->findChild<QWidget*>("CameraSequenceTestPanel");
            auto* cameraSequencePanelFrame = this->findChild<QWidget*>("CameraSequenceTestPanelFrame");
            auto* sequenceTestWidget = this->findChild<QWidget*>("SequenceTestTab");
            auto* operationalSequenceTab = this->findChild<QWidget*>("OperationalSequenceTab");
            auto* cameraAdvancedPanel = this->findChild<QWidget*>("CameraAdvancedFrameStatsPanel");
            auto* cameraLutRangeBar = this->findChild<QWidget*>("CameraLutRangeBar");
            auto* cameraNavButton = this->findChild<QPushButton*>("NavCameraButton");
            auto* liveHardwarePanel = this->findChild<QWidget*>("LiveHardwarePanel");
            auto* liveClassDistributionPanel = this->findChild<QWidget*>("LiveClassDistributionPanel");
            auto* cameraIndependentBinningCheck = this->findChild<QCheckBox*>("CameraIndependentBinningCheckBox");
            auto* cameraBinHSpin = this->findChild<QSpinBox*>("CameraBinHSpinBox");
            auto* cameraBinVSpin = this->findChild<QSpinBox*>("CameraBinVSpinBox");
            auto* cameraLutMinSlider = this->findChild<QSlider*>("CameraLutMinSlider");
            auto* cameraLutMaxSlider = this->findChild<QSlider*>("CameraLutMaxSlider");
            auto* cameraPresetCombo = this->findChild<QComboBox*>("CameraPresetComboBox");
            auto* cameraBitsCombo = this->findChild<QComboBox*>("CameraBitsComboBox");
            auto* cameraWidthSpin = this->findChild<QSpinBox*>("CameraCustomWidthSpinBox");
            auto* cameraHeightSpin = this->findChild<QSpinBox*>("CameraCustomHeightSpinBox");
            auto* cameraExposureSpin = this->findChild<QDoubleSpinBox*>("CameraExposureSpinBox");
            auto* cameraReadoutCombo = this->findChild<QComboBox*>("CameraReadoutSpeedComboBox");
            auto* cameraBinningCombo = this->findChild<QComboBox*>("CameraBinningComboBox");
            auto* cameraLutModeControl = this->findChild<QWidget*>("CameraLutModeSegmentedControl");
            auto* cameraLutMinSpin = this->findChild<QSpinBox*>("CameraLutMinSpinBox");
            auto* cameraLutMaxSpin = this->findChild<QSpinBox*>("CameraLutMaxSpinBox");
            auto* cameraDisplayEverySpin = this->findChild<QSpinBox*>("CameraDisplayEverySpinBox");
            auto* cameraStartButton = this->findChild<QPushButton*>("CameraStartButton");
            auto* cameraReconnectButton = this->findChild<QPushButton*>("CameraReconnectButton");
            auto* cameraApplyButton = this->findChild<QPushButton*>("CameraApplySettingsButton");
            auto* cameraStopButton = this->findChild<QPushButton*>("CameraStopButton");
            if (!cameraReconnectButton) {
                cameraReconnectButton = reconnectBtn;
            }
            if (!cameraApplyButton) {
                cameraApplyButton = applyBtn;
            }
            auto* pipelineStartButton = this->findChild<QPushButton*>("PipelineStartButton");
            auto* pipelineStopButton = this->findChild<QPushButton*>("PipelineStopButton");
            auto* savePathLineEdit = this->findChild<QLineEdit*>("SavePathEdit");
            auto* saveBrowseButton = this->findChild<QPushButton*>("SaveBrowseButton");
            auto* saveOpenFolderButton = this->findChild<QPushButton*>("SaveOpenFolderButton");
            auto* cameraRecordingFormatControl = this->findChild<QWidget*>("CameraRecordingFormatSegmentedControl");
            auto* saveStartButton = this->findChild<QPushButton*>("SaveStartButton");
            auto* saveStopButton = this->findChild<QPushButton*>("SaveStopButton");
            auto* sequenceFolderEdit = this->findChild<QLineEdit*>("SequenceFolderEdit");
            auto* sequenceBrowseButton = this->findChild<QPushButton*>("SequenceBrowseButton");
            auto* sequenceLoadButton = this->findChild<QPushButton*>("SequenceLoadButton");
            auto* sequenceStartButton = this->findChild<QPushButton*>("SequenceStartTestButton");
            auto* sequenceStopButton = this->findChild<QPushButton*>("SequenceStopButton");
            auto* sequenceFpsSpin = this->findChild<QDoubleSpinBox*>("SequenceFpsSpinBox");
            auto* sequenceStatusLabel = this->findChild<QLabel*>("SequenceStatusLabel");
            auto* sequenceLogLabel = this->findChild<QLabel*>("SequenceLogLabel");
            auto* liveRunEventsMetricLabel = this->findChild<QLabel*>("LiveRunEventsMetricLabel");
            auto* liveRunClassifiedHitMetricLabel = this->findChild<QLabel*>("LiveRunClassifiedHitMetricLabel");
            auto* liveRunClassifiedWasteMetricLabel =
                this->findChild<QLabel*>("LiveRunClassifiedWasteMetricLabel");
            auto* liveRunWentToHitMetricLabel = this->findChild<QLabel*>("LiveRunWentToHitMetricLabel");
            auto* liveRunWentToWasteMetricLabel = this->findChild<QLabel*>("LiveRunWentToWasteMetricLabel");
            auto* liveLastDecisionValueLabel = this->findChild<QLabel*>("LiveLastDecisionValueLabel");
            auto* statsClassTextLabel = this->findChild<QLabel*>("StatsClassCountsLabel");
            auto* statsLastEventLabel = this->findChild<QLabel*>("StatsLastEventLabel");
            auto* navRailFrame = this->findChild<QFrame*>("OpenDssNavigationRail");
            auto* headerFrame = this->findChild<QFrame*>("OpenDssHeader");
            auto* statusStripFrame = this->findChild<QFrame*>("OpenDssStatusStrip");
            auto* displayOverlayAction = this->findChild<QAction*>("DisplayOverlayAction");
            auto* displayClearOverlayAction = this->findChild<QAction*>("DisplayClearOverlayAction");
            auto* analysisOverlayCheck = this->findChild<QCheckBox*>("AnalysisOverlayCheckBox");
            auto* liveViewerOverlayToggle = this->findChild<QToolButton*>("LiveViewerOverlayToggle");
            auto* liveViewerDetectionOverlay = this->findChild<QWidget*>("LiveViewerDetectionOverlay");
            auto* liveDetectorSettingsButton = this->findChild<QPushButton*>("LiveDetectorTuningButton");
            auto* liveDetectorSettingsDrawer = this->findChild<QFrame*>("LiveDetectorTuningDrawer");
            auto* liveDetectorMinRectangleSpin = this->findChild<QSpinBox*>("LiveDetectorMinRectangleSizeSpinBox");
            auto* rightViewport = rightScroll ? rightScroll->viewport() : nullptr;

            auto requireContained = [&](QWidget* child, QWidget* parent, const QString& message) {
                if (!child || !parent) {
                    require(false, message + " (missing widget)");
                    return;
                }
                const QRect childRect(child->mapTo(parent, QPoint(0, 0)), child->size());
                const QRect bounds = parent->contentsRect();
                require(bounds.contains(childRect), message);
            };
            auto requireHorizontallyContained = [&](QWidget* child, QWidget* parent, const QString& message) {
                if (!child || !parent) {
                    require(false, message + " (missing widget)");
                    return;
                }
                const QRect childRect(child->mapTo(parent, QPoint(0, 0)), child->size());
                const QRect bounds = parent->contentsRect();
                require(childRect.left() >= bounds.left() && childRect.right() <= bounds.right(), message);
            };
            auto hasLabelText = [](QWidget* root, const QString& text) {
                if (!root)
                    return false;
                for (auto* label : root->findChildren<QLabel*>()) {
                    if (label->text() == text)
                        return true;
                }
                return false;
            };

            require(cameraNavButton == nullptr, "NavCameraButton is absent");
            require(cameraWorkspace == nullptr, "CameraWorkspace is absent");
            require(workspaceStack->currentWidget() == liveWorkspacePage, "LiveWorkspace is selected");
            require(rightViewport != nullptr, "LiveRightMetricsScrollArea viewport exists");
            require(cameraControlsStack != nullptr, "CameraControlsStack exists");
            require(cameraFormatPanel != nullptr, "CameraFormatSpeedPanel exists");
            require(cameraFormatPanel && cameraFormatPanel->isVisibleTo(this), "CameraFormatSpeedPanel is visible");
            require(cameraFormatPanelFrame != nullptr, "CameraFormatSpeedPanelFrame exists");
            require(cameraLutPanel == nullptr, "CameraLutDisplayPanel is absent");
            require(cameraLutPanelFrame == nullptr, "CameraLutDisplayPanelFrame is absent");
            require(cameraRecordingPanel != nullptr, "CameraRecordingPanel exists");
            require(cameraRecordingPanel && cameraRecordingPanel->isVisibleTo(this), "CameraRecordingPanel is visible");
            require(cameraRecordingPanelFrame != nullptr, "CameraRecordingPanelFrame exists");
            require(cameraSequencePanel != nullptr, "CameraSequenceTestPanel exists");
            require(cameraSequencePanel && cameraSequencePanel->isVisibleTo(this), "CameraSequenceTestPanel is visible");
            require(cameraSequencePanelFrame != nullptr, "CameraSequenceTestPanelFrame exists");
            require(sequenceTestWidget && cameraSequencePanelFrame && cameraSequencePanelFrame->isAncestorOf(sequenceTestWidget),
                    "SequenceTestTab controls are parented inside the Live Sequence Test section");
            require(operationalSequenceTab == nullptr, "Operational Sequence tab is absent after moving controls to Live");
            require(cameraLutRangeBar != nullptr, "CameraLutRangeBar exists");
            require(cameraLutRangeBar && cameraLutRangeBar->isVisibleTo(this), "CameraLutRangeBar is visible");
            require(cameraAdvancedPanel == nullptr, "CameraAdvancedFrameStatsPanel is absent");
            require(liveHardwarePanel == nullptr, "LiveHardwarePanel is absent");
            require(liveClassDistributionPanel == nullptr, "LiveClassDistributionPanel is absent");
            require(cameraIndependentBinningCheck == nullptr, "CameraIndependentBinningCheckBox is absent");
            require(cameraBinHSpin == nullptr, "CameraBinHSpinBox is absent");
            require(cameraBinVSpin == nullptr, "CameraBinVSpinBox is absent");
            require(cameraLutMinSlider == nullptr, "CameraLutMinSlider is absent from the visible workspace tree");
            require(cameraLutMaxSlider == nullptr, "CameraLutMaxSlider is absent from the visible workspace tree");
            require(cameraLutModeControl == nullptr, "CameraLutModeSegmentedControl is absent");
            require(cameraDisplayEverySpin == nullptr, "CameraDisplayEverySpinBox is absent from the visible workspace tree");
            require(cameraStopButton == nullptr, "CameraStopButton is absent");
            require(cameraStartButton && cameraStartButton->text() == "Start Camera",
                    "Camera action starts as Start Camera");
            require(cameraStartButton && cameraStartButton->toolTip() == "Start camera acquisition.",
                    "Camera action tooltip starts as Start camera acquisition.");
            require(cameraReconnectButton && cameraReconnectButton->text().contains("Reconnect", Qt::CaseInsensitive),
                    QString("Camera reconnect button keeps expected visible wording (text=%1)")
                        .arg(cameraReconnectButton ? cameraReconnectButton->text() : QStringLiteral("<missing>")));
            require(cameraApplyButton && cameraApplyButton->text().contains("Apply", Qt::CaseInsensitive) &&
                        cameraApplyButton->text().contains("Settings", Qt::CaseInsensitive),
                    QString("Camera apply button keeps expected visible wording (text=%1)")
                        .arg(cameraApplyButton ? cameraApplyButton->text() : QStringLiteral("<missing>")));
            const bool realCameraVerifier = !options.noStartupPrompts;
            require(realCameraVerifier, "Camera workspace verifier requires normal camera startup");
            if (!realCameraVerifier) {
                require(pipelineStartButton && !pipelineStartButton->isEnabled(),
                        "Start Sorting stays disabled until a valid pipeline is ready");
                require(pipelineStartButton && pipelineStartButton->isVisibleTo(this),
                        "Sorting action starts as Start Sorting");
                require(pipelineStopButton && !pipelineStopButton->isVisibleTo(this),
                        "Stop Sorting action starts hidden until sorting is active");
            } else {
                require(pipelineStartButton != nullptr, "Start Sorting control exists in real camera verifier");
                require(pipelineStopButton != nullptr, "Stop Sorting control exists in real camera verifier");
            }
            require(navRailFrame != nullptr, "OpenDssNavigationRail exists");
            require(headerFrame != nullptr, "OpenDssHeader exists");
            require(statusStripFrame != nullptr, "OpenDssStatusStrip exists");
            require(displayOverlayAction == nullptr, "DisplayOverlayAction is absent");
            require(displayClearOverlayAction == nullptr, "DisplayClearOverlayAction is absent");
            require(analysisOverlayCheck == nullptr, "AnalysisOverlayCheckBox is absent");
            require(liveViewerOverlayToggle == nullptr, "LiveViewerOverlayToggle is absent");
            require(liveViewerDetectionOverlay == nullptr, "LiveViewerDetectionOverlay is absent");
            require(liveDetectorSettingsButton && liveDetectorSettingsButton->text() == "Detector",
                    "Live detector settings button is discoverable");
            require(liveDetectorSettingsButton && liveDetectorSettingsButton->toolTip() == "Open detector settings.",
                    "Live detector settings button tooltip explains the action");
            require(liveDetectorSettingsDrawer != nullptr, "Live detector settings drawer exists");
            require(hasLabelText(liveDetectorSettingsDrawer, "Detector settings"),
                    "Live detector drawer title reads Detector settings");
            require(hasLabelText(liveDetectorSettingsDrawer, "Min rectangle size"),
                    "Live detector minimum bbox label uses rectangle wording");
            require(!hasLabelText(liveDetectorSettingsDrawer, "Min contour points"),
                    "Live detector drawer no longer labels the bbox filter as contour points");
            require(liveDetectorMinRectangleSpin && liveDetectorMinRectangleSpin->suffix().trimmed() == "px",
                    "Live detector minimum rectangle control shows pixel units");
            require(liveDetectorMinRectangleSpin &&
                        liveDetectorMinRectangleSpin->toolTip().contains("Minimum bounding rectangle width and height"),
                    "Live detector minimum rectangle tooltip explains width and height");
            require(cameraControlsStack && rightViewport && cameraControlsStack->width() <= rightViewport->width(),
                    "CameraControlsStack width does not exceed the Live View right-panel viewport");
            require(rightScroll->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
                    "Live View right-panel horizontal scrollbar remains disabled");

            requireHorizontallyContained(cameraFormatPanelFrame, rightViewport,
                                         "CameraFormatSpeedPanelFrame fits within the viewport width");
            requireHorizontallyContained(cameraRecordingPanelFrame, rightViewport,
                                         "CameraRecordingPanelFrame fits within the viewport width");
            requireContained(cameraPresetCombo, cameraFormatPanelFrame,
                             "CameraPresetComboBox fits within Format & Speed");
            requireContained(cameraBitsCombo, cameraFormatPanelFrame, "CameraBitsComboBox fits within Format & Speed");
            requireContained(cameraWidthSpin, cameraFormatPanelFrame,
                             "CameraCustomWidthSpinBox fits within Format & Speed");
            requireContained(cameraHeightSpin, cameraFormatPanelFrame,
                             "CameraCustomHeightSpinBox fits within Format & Speed");
            requireContained(cameraExposureSpin, cameraFormatPanelFrame,
                             "CameraExposureSpinBox fits within Format & Speed");
            requireContained(cameraReadoutCombo, cameraFormatPanelFrame,
                             "CameraReadoutSpeedComboBox fits within Format & Speed");
            requireContained(cameraBinningCombo, cameraFormatPanelFrame,
                             "CameraBinningComboBox fits within Format & Speed");
            require(cameraPresetCombo && cameraPresetCombo->width() >= cameraPresetCombo->sizeHint().width(),
                    "CameraPresetComboBox expands enough for the active preset text");
            require(cameraReadoutCombo && cameraReadoutCombo->width() >= cameraReadoutCombo->sizeHint().width(),
                    "CameraReadoutSpeedComboBox expands enough for the active readout text");
            requireContained(cameraLutMinSpin, cameraFormatPanelFrame, "CameraLutMinSpinBox fits within Format & Speed");
            requireContained(cameraLutMaxSpin, cameraFormatPanelFrame, "CameraLutMaxSpinBox fits within Format & Speed");
            requireContained(cameraLutRangeBar, cameraFormatPanelFrame, "CameraLutRangeBar fits within Format & Speed");
            requireContained(savePathLineEdit, cameraRecordingPanelFrame, "SavePathEdit fits within Recording");
            requireContained(saveBrowseButton, cameraRecordingPanelFrame, "SaveBrowseButton fits within Recording");
            requireContained(saveOpenFolderButton, cameraRecordingPanelFrame,
                             "SaveOpenFolderButton fits within Recording");
            requireContained(cameraRecordingFormatControl, cameraRecordingPanelFrame,
                             "CameraRecordingFormatSegmentedControl fits within Recording");
            requireContained(saveStartButton, cameraRecordingPanelFrame, "SaveStartButton fits within Recording");
            requireContained(saveStopButton, cameraRecordingPanelFrame, "SaveStopButton fits within Recording");
            requireHorizontallyContained(cameraSequencePanelFrame, rightViewport,
                                         "CameraSequenceTestPanelFrame fits within the viewport width");
            requireContained(sequenceFolderEdit, cameraSequencePanelFrame, "SequenceFolderEdit fits within Sequence Test");
            requireContained(sequenceBrowseButton, cameraSequencePanelFrame,
                             "SequenceBrowseButton fits within Sequence Test");
            requireContained(sequenceLoadButton, cameraSequencePanelFrame, "SequenceLoadButton fits within Sequence Test");
            requireContained(sequenceStartButton, cameraSequencePanelFrame,
                             "SequenceStartTestButton fits within Sequence Test");
            requireContained(sequenceStopButton, cameraSequencePanelFrame, "SequenceStopButton fits within Sequence Test");
            requireContained(sequenceFpsSpin, cameraSequencePanelFrame, "SequenceFpsSpinBox fits within Sequence Test");
            requireContained(sequenceStatusLabel, cameraSequencePanelFrame,
                             "SequenceStatusLabel fits within Sequence Test");
            requireContained(sequenceLogLabel, cameraSequencePanelFrame, "SequenceLogLabel fits within Sequence Test");
            require(sequenceStartButton && sequenceStartButton->text().contains("Recorded Sequence"),
                    "Sequence run button makes recorded-sequence simulation explicit");
            require(sequenceStartButton &&
                        sequenceStartButton->toolTip().contains("DAQ output disabled", Qt::CaseInsensitive),
                    "Sequence run button tooltip states DAQ output is disabled");
            require(sequenceStartButton && sequenceStartButton->property("daqOutputMode").toString() ==
                                               QStringLiteral("disabled-for-replay"),
                    "Sequence run button exposes the disabled-for-replay DAQ output guard");
            require(sequenceStartButton && !sequenceStartButton->isEnabled(),
                    "Sequence run button remains disabled until a sequence is loaded");
            require(runStateResetButton != nullptr, "RunStateResetCountersButton exists");
            require(runStateResetButton && runStateResetButton->objectName() == "RunStateResetCountersButton",
                    "RunStateResetCountersButton keeps a stable object name");
            require(runStateResetButton && runStateResetButton->isVisibleTo(this),
                    "RunStateResetCountersButton is visible in Live View");

            const QString initialRunStatusText = runStatusItem ? runStatusItem->text() : QString();
            if (realCameraVerifier) {
                const bool initSettled = waitUntil(12000, [&]() {
                    const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                    return cameraOpened || cameraStatus.contains("error", Qt::CaseInsensitive) ||
                           cameraStatus.contains("unavailable", Qt::CaseInsensitive);
                });
                require(initSettled, "Real camera initialization reached a terminal app-owned status");
                require(cameraOpened && cameraStatusItem && cameraStatusItem->text() == "Camera: connected",
                        QString("Real camera initialized and status chip is connected (status=%1)")
                            .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>")));
                if (cameraOpened && cameraStartButton) {
                    cameraStartButton->click();
                    const bool captureStarted = waitUntil(8000, [&]() {
                        const QString cameraStatus = cameraStatusItem ? cameraStatusItem->text() : QString();
                        return appState.cameraStreaming || cameraStatus.contains("acquiring", Qt::CaseInsensitive) ||
                               cameraStatus.contains("error", Qt::CaseInsensitive);
                    });
                    require(captureStarted, "Real camera capture start completed through CameraStartButton");
                    require(appState.cameraStreaming && cameraStatusItem &&
                                cameraStatusItem->text().contains("acquiring", Qt::CaseInsensitive),
                            QString("Real camera status chip reports acquiring (status=%1)")
                                .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>")));
                    const bool frameArrived = waitUntil(12000, [&]() { return !cameraController->lastFrame().isNull(); });
                    const FrameMeta realMeta = cameraController->lastMeta();
                    require(frameArrived, "Real camera delivered at least one frame");
                    require(realMeta.width > 0 && realMeta.height > 0 && realMeta.frameIndex > 0,
                            QString("Real camera frame metadata is populated (width=%1 height=%2 frame=%3 delivered=%4)")
                                .arg(realMeta.width)
                                .arg(realMeta.height)
                                .arg(realMeta.frameIndex)
                                .arg(realMeta.delivered));
                    qInfo().noquote()
                        << QString("VERIFY INFO: real camera status=%1 frame=%2 size=%3x%4 delivered=%5 dropped=%6")
                               .arg(cameraStatusItem ? cameraStatusItem->text() : QStringLiteral("<missing>"))
                               .arg(realMeta.frameIndex)
                               .arg(realMeta.width)
                               .arg(realMeta.height)
                               .arg(realMeta.delivered)
                               .arg(realMeta.dropped);
                    cameraStartButton->click();
                    const bool captureStopped = waitUntil(8000, [&]() { return !appState.cameraStreaming; });
                    require(captureStopped, "Real camera capture stops through CameraStartButton");
                }
            }
            require(runStatusItem && runStatusItem->text() == initialRunStatusText,
                    "Real camera verifier leaves sorting run state unchanged");

            {
                StatsSnapshot seededSnap;
                {
                    QMutexLocker lock(&statsMutex);
                    stats.totalEvents = 9;
                    stats.classifiedHitCount = 4;
                    stats.classifiedWasteCount = 3;
                    stats.wentToHitCount = 2;
                    stats.wentToWasteCount = 7;
                    stats.classCounts.clear();
                    stats.classCounts.insert("Single", 4);
                    stats.classCounts.insert("Empty", 3);
                    stats.lastEventDir = "Hit";
                    stats.lastEventLabel = "Single";
                    stats.lastDecisionFrame = 42;
                    stats.lastDecisionEventId = 9;
                    stats.eventActive = false;
                    seededSnap = makeStatsSnapshot(stats);
                }
                applyStatsSnapshot(seededSnap);
                waitForUi(650);
                require(liveRunEventsMetricLabel && liveRunEventsMetricLabel->text() == "9",
                        "LiveRunEventsMetricLabel reflects seeded event count before reset");
                require(liveRunClassifiedHitMetricLabel && liveRunClassifiedHitMetricLabel->text() == "4",
                        "LiveRunClassifiedHitMetricLabel reflects seeded hit count before reset");
                require(liveRunClassifiedWasteMetricLabel && liveRunClassifiedWasteMetricLabel->text() == "3",
                        "LiveRunClassifiedWasteMetricLabel reflects seeded waste count before reset");
                require(liveRunWentToHitMetricLabel && liveRunWentToHitMetricLabel->text() == "2",
                        "LiveRunWentToHitMetricLabel reflects seeded went-to-hit count before reset");
                require(liveRunWentToWasteMetricLabel && liveRunWentToWasteMetricLabel->text() == "7",
                        "LiveRunWentToWasteMetricLabel reflects seeded went-to-waste count before reset");
                require(statsClassTextLabel && statsClassTextLabel->text().contains("Single: 4"),
                        "StatsClassCountsLabel reflects seeded class counts before reset");
                require(statsLastEventLabel && statsLastEventLabel->text().contains("Single (Hit)"),
                        "StatsLastEventLabel reflects seeded last event before reset");

                if (runStateResetButton)
                    runStateResetButton->click();
                waitForUi(650);
                require(liveRunEventsMetricLabel && liveRunEventsMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets event count to zero");
                require(liveRunClassifiedHitMetricLabel && liveRunClassifiedHitMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets classified hit count to zero");
                require(liveRunClassifiedWasteMetricLabel && liveRunClassifiedWasteMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets classified waste count to zero");
                require(liveRunWentToHitMetricLabel && liveRunWentToHitMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets went-to-hit count to zero");
                require(liveRunWentToWasteMetricLabel && liveRunWentToWasteMetricLabel->text() == "0",
                        "RunStateResetCountersButton resets went-to-waste count to zero");
                require(statsClassTextLabel && statsClassTextLabel->text() == "Classes:\n(none)",
                        "RunStateResetCountersButton clears class count text back to default");
                require(statsLastEventLabel && statsLastEventLabel->text() == "Last event: --",
                        "RunStateResetCountersButton clears last-event text");
                require(liveLastDecisionValueLabel && liveLastDecisionValueLabel->text() == "--",
                        "RunStateResetCountersButton clears the last decision summary");
            }

            const auto shellColors = desktop_app::theme::colors(currentThemeMode);
            const QString shellCss = shellColors.shellBackground.name(QColor::HexRgb);
            const QString appCss = shellColors.appBackground.name(QColor::HexRgb);
            const QString styleSheet = this->styleSheet();
            require(styleSheet.contains(shellCss, Qt::CaseInsensitive),
                    QString("shell stylesheet contains neutral shell color %1").arg(shellCss));
            require(shellCss.compare(QStringLiteral("#0B1F5E"), Qt::CaseInsensitive) != 0,
                    "shell color is no longer dark blue");
            require(shellCss.compare(appCss, Qt::CaseInsensitive) != 0,
                    "shell color remains distinct from app background");

            lutMinSpin->setValue(32);
            lutMaxSpin->setValue(180);
            app.processEvents();
            require(cameraController->lutMinValue() == 32, "LUT min runtime value updates from consolidated controls");
            require(cameraController->lutMaxValue() == 180, "LUT max runtime value updates from consolidated controls");

            QImage sample(320, 240, QImage::Format_Grayscale8);
            for (int y = 0; y < sample.height(); ++y) {
                uchar* row = sample.scanLine(y);
                for (int x = 0; x < sample.width(); ++x) {
                    row[x] = static_cast<uchar>((x + y) % 256);
                }
            }
            FrameMeta verifyMeta;
            verifyMeta.width = sample.width();
            verifyMeta.height = sample.height();
            verifyMeta.bits = 8;
            verifyMeta.binning = 1.0;
            verifyMeta.frameIndex = 42;
            verifyMeta.delivered = 42;
            verifyMeta.dropped = 0;
            verifyMeta.internalFps = 37.5;
            verifyMeta.readoutSpeed = 1.0;
            cameraController->applyFrameToPreviewWorkspaces(sample, verifyMeta, 37.5);
            app.processEvents();

            auto* liveImageLabel = imageView->findChild<QLabel*>("LiveImageLabel");
            const QPixmap livePixmap = liveImageLabel ? liveImageLabel->pixmap(Qt::ReturnByValue) : QPixmap();
            require(liveImageLabel && !livePixmap.isNull(), "LiveImageLabel received a rendered frame");
            require(!liveViewerEmpty->isVisible(), "LiveViewerEmptyState hides after frame update");
            require(liveHudResolution->text().contains("320 x 240"),
                    "LiveViewerHudResolutionLabel updated from frame data");
            require(liveHudFps->text().contains("42"), "LiveViewerHudFpsLabel updated from frame data");

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (!failures.isEmpty()) {
                logMessage("Live camera consolidation verifier failed: " + failures.join("; "));
            } else {
                logMessage("Live camera consolidation verifier passed.");
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyDaqSettings) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << "VERIFY FAIL:" << message;
                } else {
                    qInfo().noquote() << "VERIFY PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };

            settingsController->refreshDaqDeviceOptions(true);
            workspaceStack->setCurrentWidget(settingsWorkspacePage);
            settingsNavButton->setChecked(true);
            headerTitleLabel->setText("/ Settings");
            headerStatusText->setText("Settings workspace");
            app.processEvents();
            waitForUi(350);

            auto* settingsHardwarePanel = this->findChild<QWidget*>("SettingsHardwarePanel");
            auto* deviceCombo = this->findChild<QComboBox*>("DaqDeviceComboBox");
            auto* channelEdit = this->findChild<QLineEdit*>("DaqChannelEdit");
            auto* amplitudeSpin = this->findChild<QDoubleSpinBox*>("DaqAmplitudeSpinBox");
            auto* frequencySpin = this->findChild<QDoubleSpinBox*>("DaqFrequencySpinBox");
            auto* durationSpin = this->findChild<QDoubleSpinBox*>("DaqDurationSpinBox");
            auto* delaySpin = this->findChild<QDoubleSpinBox*>("DaqDelaySpinBox");
            auto* reconnectButton = this->findChild<QPushButton*>("DaqReconnectButton");
            auto* manualTriggerButton = this->findChild<QPushButton*>("DaqManualTriggerButton");
            auto* statusIndicatorWidget = this->findChild<QLabel*>("DaqStatusTextLabel");
            auto* statusBarDaqWidget = this->findChild<QLabel*>("DaqStatusBarLabel");
            auto* shellDaqStatusWidget = this->findChild<QLabel*>("OpenDssShellDaqStatusLabel");
            auto* headerDaqChipWidget = this->findChild<QLabel*>("OpenDssHeaderDaqChip");
            auto* liveTriggerSafeButton = this->findChild<QPushButton*>("LiveTriggerSafeButton");
            auto* forceTriggerButton = this->findChild<QPushButton*>("LiveForceTriggerButton");
            auto* manualTriggerMenuAction = this->findChild<QAction*>("SortingForceTriggerAction");
            auto hasPanelLabelText = [](QWidget* root, const QString& text) {
                if (!root) {
                    return false;
                }
                for (auto* label : root->findChildren<QLabel*>()) {
                    if (label->text() == text) {
                        return true;
                    }
                }
                return false;
            };
            const QString daqStatusText = statusBarDaqWidget ? statusBarDaqWidget->text().trimmed().toLower() : QString();
            const bool manualTriggerReady =
                daqStatusText.contains("available") && !daqStatusText.contains("disabled") &&
                !daqStatusText.contains("unavailable");

            require(settingsHardwarePanel != nullptr, "Settings hardware panel exists");
            require(deviceCombo != nullptr, "DAQ device combo exists");
            require(channelEdit != nullptr, "DAQ channel edit exists");
            require(deviceCombo && deviceCombo->objectName() == "DaqDeviceComboBox",
                    "DAQ device combo uses the direct-lookup object name");
            require(deviceCombo && channelEdit &&
                        deviceCombo->mapTo(settingsHardwarePanel, QPoint(0, 0)).y() <
                            channelEdit->mapTo(settingsHardwarePanel, QPoint(0, 0)).y(),
                    "DAQ device combo is above the DAQ channel field in Settings > Hardware");
            require(reconnectButton && reconnectButton->text() == "Reconnect DAQ",
                    "DAQ reconnect button wording matches the current DAQ path");
            require(manualTriggerButton && !manualTriggerButton->isVisibleTo(this),
                    "Settings Manual Trigger is hidden from users");
            require(liveTriggerSafeButton == nullptr, "LiveTriggerSafeButton is absent from Live View");
            require(forceTriggerButton && forceTriggerButton->text() == "Manual Trigger",
                    "Live View uses Manual Trigger wording below the camera frame");
            require(manualTriggerMenuAction != nullptr, "Manual Trigger menu action exists");
            require(manualTriggerMenuAction && forceTriggerButton &&
                        manualTriggerMenuAction->isEnabled() == forceTriggerButton->isEnabled(),
                    "Manual Trigger menu action shares the Live View button gate");
            if (manualTriggerReady) {
                require(forceTriggerButton && forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger enables automatically when DAQ is available");
                require(manualTriggerMenuAction && manualTriggerMenuAction->isEnabled(),
                        "Manual Trigger menu action enables automatically when DAQ is available");
            } else {
                require(forceTriggerButton && !forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger stays disabled when DAQ is unavailable");
                require(manualTriggerMenuAction && !manualTriggerMenuAction->isEnabled(),
                        "Manual Trigger menu action stays disabled when DAQ is unavailable");
            }
            require(!hasPanelLabelText(settingsHardwarePanel, "Test mode preference"),
                    "Settings hardware no longer exposes a test mode preference");

            QStringList comboEntries;
            for (int i = 0; i < deviceCombo->count(); ++i) {
                comboEntries
                    << QStringLiteral("%1 => %2").arg(deviceCombo->itemText(i), deviceCombo->itemData(i).toString());
            }
            qInfo().noquote() << "VERIFY INFO: DAQ combo entries:" << comboEntries.join(" | ");
            qInfo().noquote() << "VERIFY INFO: Selected DAQ device:" << deviceCombo->currentData().toString();
            qInfo().noquote() << "VERIFY INFO: Selected DAQ channel:" << channelEdit->text().trimmed();
            qInfo().noquote() << "VERIFY INFO: Manual Trigger enabled:"
                              << (manualTriggerButton && manualTriggerButton->isEnabled());
            qInfo().noquote() << "VERIFY INFO: Discovered DAQ summary:"
                              << (settingsController->describeDiscoveredDaqDevices().isEmpty()
                                      ? QStringLiteral("<none>")
                                      : settingsController->describeDiscoveredDaqDevices());
            if (!settingsController->daqDiscoveryError().isEmpty()) {
                qInfo().noquote() << "VERIFY INFO: DAQ discovery status:" << settingsController->daqDiscoveryError();
            }

            if (!settingsController->discoveredDaqDevices().empty()) {
                require(deviceCombo->count() == static_cast<int>(settingsController->discoveredDaqDevices().size()),
                        "DAQ combo count matches the discovered device list");
            }

            const int compatibleCount = settingsController->discoveredCompatibleDeviceCount();
            QString onlyCompatibleDevice;
            for (const auto& device : settingsController->discoveredDaqDevices()) {
                if (device.isCompatible()) {
                    onlyCompatibleDevice = QString::fromStdString(device.name);
                    break;
                }
            }
            if (compatibleCount == 1) {
                require(deviceCombo->currentData().toString().compare(onlyCompatibleDevice, Qt::CaseInsensitive) == 0,
                        QStringLiteral("Single compatible DAQ auto-selects %1").arg(onlyCompatibleDevice));
            } else {
                qInfo().noquote() << "VERIFY INFO: Compatible DAQ count =" << compatibleCount;
            }

            const bool hasRealDiscoveredSelection =
                !settingsController->discoveredDaqDevices().empty() &&
                !deviceCombo->currentData().toString().trimmed().isEmpty();

            if (deviceCombo->count() > 1) {
                const int originalIndex = deviceCombo->currentIndex();
                const int nextIndex = (originalIndex + 1) % deviceCombo->count();
                deviceCombo->setCurrentIndex(nextIndex);
                waitForUi(350);
                QSettings settings;
                const QString selectedDevice = deviceCombo->currentData().toString().trimmed();
                require(settings.value("settings/daqSelectedDevice")
                                .toString()
                                .trimmed()
                                .compare(selectedDevice, Qt::CaseInsensitive) == 0,
                        "Changing the DAQ combo persists the selected device in QSettings");
                const DaqDeviceInfo* selectedInfo = nullptr;
                for (const auto& device : settingsController->discoveredDaqDevices()) {
                    if (QString::fromStdString(device.name).compare(selectedDevice, Qt::CaseInsensitive) == 0) {
                        selectedInfo = &device;
                        break;
                    }
                }
                if (selectedInfo && selectedInfo->isCompatible()) {
                    const QString channelText = channelEdit->text().trimmed();
                    const int slash = channelText.indexOf('/');
                    const QString channelDevice = slash > 0 ? channelText.left(slash) : channelText;
                    require(channelDevice.compare(selectedDevice, Qt::CaseInsensitive) == 0,
                            "Changing the DAQ combo updates the active DAQ channel device prefix");
                } else {
                    require(channelEdit->text().trimmed().isEmpty(),
                            "Selecting a DAQ without AO output clears the active channel");
                }
                if (reconnectButton) {
                    reconnectButton->click();
                    waitForUi(500);
                }
                require(statusIndicatorWidget != nullptr && !statusIndicatorWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the DAQ indicator populated");
                require(statusBarDaqWidget != nullptr && !statusBarDaqWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the DAQ status-bar label populated");
                require(headerDaqChipWidget != nullptr && !headerDaqChipWidget->text().trimmed().isEmpty(),
                        "DAQ reconnect keeps the header DAQ chip populated");
                require(forceTriggerButton != nullptr && !forceTriggerButton->isEnabled(),
                        "Live View Manual Trigger stays disabled during verification when DAQ is unavailable");
            } else if (deviceCombo->count() == 1 && hasRealDiscoveredSelection) {
                QSettings settings;
                require(settings.value("settings/daqSelectedDevice")
                                .toString()
                                .trimmed()
                                .compare(deviceCombo->currentData().toString().trimmed(), Qt::CaseInsensitive) == 0,
                        "Single discovered DAQ selection is persisted in QSettings");
                if (reconnectButton) {
                    reconnectButton->click();
                    waitForUi(500);
                }
            } else {
                require(!deviceCombo->isEnabled(), "DAQ combo disables when no devices are available");
                require(deviceCombo->currentData().toString().trimmed().isEmpty(),
                        "No-device DAQ combo placeholder does not expose a real device selection");
            }

            if (statusBarDaqWidget && shellDaqStatusWidget && headerDaqChipWidget) {
                require(statusBarDaqWidget->text() == shellDaqStatusWidget->text(),
                        "Shell DAQ status mirrors the DAQ status-bar label");
                const QString statusText = statusBarDaqWidget->text().toLower();
                const QString headerText = headerDaqChipWidget->text().toLower();
                qInfo().noquote() << "VERIFY INFO: DAQ status-bar text:" << statusBarDaqWidget->text();
                qInfo().noquote() << "VERIFY INFO: Header DAQ chip text:" << headerDaqChipWidget->text();
                if (statusText.contains("unavailable")) {
                    require(headerText.contains("unavailable"),
                            "Header DAQ chip reports unavailable when DAQ status is unavailable");
                } else if (statusText.contains("disabled")) {
                    require(headerText.contains("unavailable"),
                            "Header DAQ chip reports unavailable when DAQ status is disabled");
                } else if (statusText.contains("available")) {
                    require(headerText.contains("available"),
                            "Header DAQ chip reports available when DAQ status is available");
                } else {
                    require(headerText.contains("unavailable") || headerText.contains("unchecked"),
                            "Header DAQ chip remains coherent when DAQ status is unavailable");
                }
            }

            const int exitCode = failures.isEmpty() ? 0 : 2;
            if (!failures.isEmpty()) {
                logMessage("DAQ settings verifier failed: " + failures.join("; "));
            } else {
                logMessage("DAQ settings verifier passed.");
            }
            QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
        });
    }
    if (options.verifyDirectDaqManualTrigger || options.verifyLiveViewManualTrigger) {
        QTimer::singleShot(0, [&]() {
            QStringList failures;
            bool triggerInvoked = false;
            const bool verifyLiveViewTrigger = options.verifyLiveViewManualTrigger;
            const QString verifierPrefix = verifyLiveViewTrigger ? QStringLiteral("LIVE VIEW DAQ VERIFY")
                                                                 : QStringLiteral("DIRECT DAQ VERIFY");
            const QString triggerObjectName = verifyLiveViewTrigger ? QStringLiteral("LiveForceTriggerButton")
                                                                    : QStringLiteral("DaqManualTriggerButton");
            const QString triggerSource = verifyLiveViewTrigger ? QStringLiteral("LiveForceTriggerButton")
                                                                : QStringLiteral("DaqManualTriggerButton");
            auto require = [&](bool condition, const QString& message) {
                if (!condition) {
                    failures.push_back(message);
                    qCritical().noquote() << verifierPrefix << "FAIL:" << message;
                } else {
                    qInfo().noquote() << verifierPrefix << "PASS:" << message;
                }
            };
            auto waitForUi = [&](int ms) {
                QEventLoop loop;
                QTimer::singleShot(ms, &loop, &QEventLoop::quit);
                loop.exec();
                app.processEvents();
            };
            auto finish = [&](int exitCode) {
                qInfo().noquote() << verifierPrefix << "INFO: TriggerSource=" << triggerSource;
                qInfo().noquote() << verifierPrefix << "INFO: TriggerInvoked=" << (triggerInvoked ? 1 : 0);
                QTimer::singleShot(0, &app, [exitCode]() { QCoreApplication::exit(exitCode); });
            };
            auto nearlyEqual = [](double actual, double expected) {
                return std::abs(actual - expected) <= 0.0005;
            };

            settingsController->refreshDaqDeviceOptions(true);
            settingsController->applyDaqAvailability(settingsController->probeDaqAvailability());
            workspaceStack->setCurrentWidget(settingsWorkspacePage);
            settingsNavButton->setChecked(true);
            headerTitleLabel->setText("/ Settings");
            headerStatusText->setText("Settings workspace");
            app.processEvents();
            waitForUi(350);

            auto* deviceCombo = this->findChild<QComboBox*>("DaqDeviceComboBox");
            auto* channelEdit = this->findChild<QLineEdit*>("DaqChannelEdit");
            auto* amplitudeSpin = this->findChild<QDoubleSpinBox*>("DaqAmplitudeSpinBox");
            auto* frequencySpin = this->findChild<QDoubleSpinBox*>("DaqFrequencySpinBox");
            auto* durationSpin = this->findChild<QDoubleSpinBox*>("DaqDurationSpinBox");
            auto* delaySpin = this->findChild<QDoubleSpinBox*>("DaqDelaySpinBox");
            auto* manualTriggerButton = this->findChild<QPushButton*>("DaqManualTriggerButton");
            auto* statusIndicatorWidget = this->findChild<QLabel*>("DaqStatusTextLabel");
            auto* statusBarDaqWidget = this->findChild<QLabel*>("DaqStatusBarLabel");
            auto* forceTriggerButton = this->findChild<QPushButton*>("LiveForceTriggerButton");
            auto* triggerButton = verifyLiveViewTrigger ? forceTriggerButton : manualTriggerButton;

            const QString selectedDevice = deviceCombo ? deviceCombo->currentData().toString().trimmed() : QString();
            const QString selectedChannel = channelEdit ? channelEdit->text().trimmed() : QString();
            const QString statusIndicatorText =
                statusIndicatorWidget ? statusIndicatorWidget->text().trimmed() : QString();
            const QString statusBarText = statusBarDaqWidget ? statusBarDaqWidget->text().trimmed() : QString();
            bool pipelineTriggerReady = false;
            {
                QMutexLocker lock(&pipelineMutex);
                pipelineTriggerReady = pipeline.isTriggerReady();
            }

            qInfo().noquote() << verifierPrefix << "INFO: SelectedDevice=" << selectedDevice;
            qInfo().noquote() << verifierPrefix << "INFO: SelectedChannel=" << selectedChannel;
            qInfo().noquote() << verifierPrefix << "INFO: AmplitudeV="
                              << (amplitudeSpin ? amplitudeSpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: FrequencyKHz="
                              << (frequencySpin ? frequencySpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: DurationMs="
                              << (durationSpin ? durationSpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: DelayMs="
                              << (delaySpin ? delaySpin->value() : -1.0);
            qInfo().noquote() << verifierPrefix << "INFO: StatusIndicator=" << statusIndicatorText;
            qInfo().noquote() << verifierPrefix << "INFO: StatusBar=" << statusBarText;
            qInfo().noquote() << verifierPrefix << "INFO: PipelineTriggerReady=" << pipelineTriggerReady;
            qInfo().noquote() << verifierPrefix << "INFO: TriggerCount=1";

            require(!appState.daqDisabled, "DAQ state is not disabled");
            require(!appState.daqFault, "DAQ state is not faulted");
            require(appState.daqAvailable, "DAQ state is available");
            require(appState.daqStatusText.compare(QStringLiteral("DAQ: available"), Qt::CaseInsensitive) == 0,
                    "DAQ status text is available");
            require(deviceCombo != nullptr, "DaqDeviceComboBox exists");
            require(channelEdit != nullptr, "DaqChannelEdit exists");
            require(amplitudeSpin != nullptr, "DaqAmplitudeSpinBox exists");
            require(frequencySpin != nullptr, "DaqFrequencySpinBox exists");
            require(durationSpin != nullptr, "DaqDurationSpinBox exists");
            require(delaySpin != nullptr, "DaqDelaySpinBox exists");
            require(manualTriggerButton != nullptr, "DaqManualTriggerButton exists");
            require(manualTriggerButton && !manualTriggerButton->isVisibleTo(this),
                    "DaqManualTriggerButton is hidden from users");
            require(forceTriggerButton != nullptr, "LiveForceTriggerButton exists");
            require(forceTriggerButton && forceTriggerButton->text() == "Manual Trigger",
                    "Live View Manual Trigger button wording matches the direct DAQ path");
            require(forceTriggerButton && forceTriggerButton->isEnabled(), "LiveForceTriggerButton is enabled");
            require(triggerButton != nullptr, triggerObjectName + QStringLiteral(" exists"));
            require(triggerButton && triggerButton->isEnabled(), triggerObjectName + QStringLiteral(" is enabled"));
            require(selectedDevice == QStringLiteral("Dev2"), "Selected DAQ device is Dev2");
            require(selectedChannel == QStringLiteral("Dev2/ao0"), "Selected DAQ channel is Dev2/ao0");
            require(amplitudeSpin && nearlyEqual(amplitudeSpin->value(), 5.0), "Amplitude is 5.000 V");
            require(frequencySpin && nearlyEqual(frequencySpin->value(), 10.0), "Frequency is 10.000 kHz");
            require(durationSpin && nearlyEqual(durationSpin->value(), 5.0), "Duration is 5.000 ms");
            require(delaySpin && nearlyEqual(delaySpin->value(), 0.0), "Delay is 0.000 ms");
            require(!forceTriggerButton || !forceTriggerButton->isDown(),
                    "Live View Manual Trigger button is not active");

            if (!failures.isEmpty()) {
                const QString messagePrefix =
                    verifyLiveViewTrigger ? QStringLiteral("Live View manual trigger verifier")
                                          : QStringLiteral("Direct DAQ manual trigger verifier");
                logMessage(messagePrefix + " aborted before output: " + failures.join("; "));
                finish(2);
                return;
            }

            if (verifyLiveViewTrigger) {
                workspaceStack->setCurrentWidget(liveWorkspacePage);
                liveNavButton->setChecked(true);
                headerTitleLabel->setText("/ Live View");
                headerStatusText->setText("Live View workspace");
                app.processEvents();
                waitForUi(350);
                require(forceTriggerButton && forceTriggerButton->isVisibleTo(this),
                        "LiveForceTriggerButton is visible before the verifier click");
                if (!failures.isEmpty()) {
                    logMessage("Live View manual trigger verifier aborted before output: " + failures.join("; "));
                    finish(2);
                    return;
                }
            }

            qInfo().noquote() << verifierPrefix << "INFO: Invoking" << triggerSource << "exactly once.";
            triggerInvoked = true;
            triggerButton->click();
            waitForUi(1200);

            const QString resultText = statusLabel->text().trimmed();
            qInfo().noquote() << verifierPrefix << "INFO: ResultStatusLabel=" << resultText;
            require(resultText == QStringLiteral("DAQ trigger sent."), "Manual trigger reports DAQ trigger sent");
            require(appState.daqAvailable && !appState.daqDisabled && !appState.daqFault,
                    "DAQ remains available after manual trigger");

            if (!failures.isEmpty()) {
                const QString messagePrefix =
                    verifyLiveViewTrigger ? QStringLiteral("Live View manual trigger verifier")
                                          : QStringLiteral("Direct DAQ manual trigger verifier");
                logMessage(messagePrefix + " failed after one approved output attempt: " + failures.join("; "));
                finish(3);
                return;
            }

            if (verifyLiveViewTrigger) {
                logMessage("Live View manual trigger verifier sent one approved Dev2/ao0 output.");
            } else {
                logMessage("Direct DAQ manual trigger verifier sent one approved Dev2/ao0 output.");
            }
            finish(0);
        });
    }
    int rc = 0;
    try {
        rc = app.exec();
    } catch (const std::exception& e) {
        logMessage(QString("Fatal exception: %1").arg(e.what()));
        rc = 1;
    } catch (...) {
        logMessage("Fatal unknown exception");
        rc = 1;
    }
    logMessage(QString("Event loop exited with code %1").arg(rc));
    return rc;
}
