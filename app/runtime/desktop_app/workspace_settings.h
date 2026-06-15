#pragma once

#include <QString>

class QCheckBox;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLineEdit;
class QTabWidget;
class QWidget;

namespace desktop_app {
struct AppState;
}

namespace desktop_app::workspace {

struct SettingsWorkspaceControls {
    QString outputRoot;
    QString modelPath;
    QString metadataPath;
    QString datasetsRoot;
    QString logPath;
    bool hardwareFreeMode = false;

    QLineEdit* cameraSavePathEdit = nullptr;
    QComboBox* cameraPresetCombo = nullptr;
    QDoubleSpinBox* exposureSpin = nullptr;
    QComboBox* daqDeviceCombo = nullptr;
    QLineEdit* daqChannelEdit = nullptr;
    QDoubleSpinBox* amplitudeSpin = nullptr;
    QDoubleSpinBox* frequencySpin = nullptr;
    QDoubleSpinBox* durationSpin = nullptr;
    QDoubleSpinBox* delaySpin = nullptr;
    QCheckBox* logCheck = nullptr;
    QLineEdit* outputRootEdit = nullptr;
    QLineEdit* trainerPythonEdit = nullptr;
    QLineEdit* trainerDatasetRootEdit = nullptr;

    QDockWidget* operationDock = nullptr;
    QTabWidget* operationalTabs = nullptr;
    QWidget* analysisTab = nullptr;
    QWidget* devicesTab = nullptr;
    desktop_app::AppState* appState = nullptr;
};

QWidget* buildSettingsWorkspace(const SettingsWorkspaceControls& controls);

} // namespace desktop_app::workspace
