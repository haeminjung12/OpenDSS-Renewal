#pragma once

#include <QString>

class QCheckBox;
class QAction;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QLineEdit;
class QTabWidget;
class QWidget;

namespace desktop_app::workspace {

struct SettingsWorkspaceControls {
    QString outputRoot;
    QString modelPath;
    QString metadataPath;
    QString datasetsRoot;
    QString logPath;

    QLineEdit* cameraSavePathEdit = nullptr;
    QComboBox* cameraPresetCombo = nullptr;
    QComboBox* computeDeviceCombo = nullptr;
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
    QAction* resetLayoutAction = nullptr;
};

QWidget* buildSettingsWorkspace(const SettingsWorkspaceControls& controls);

} // namespace desktop_app::workspace
