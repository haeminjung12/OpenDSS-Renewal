#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QWidget;

namespace desktop_app::workspace {

struct CameraWorkspaceControls {
    QComboBox* presetCombo = nullptr;
    QComboBox* bitsCombo = nullptr;
    QSpinBox* customWidthSpin = nullptr;
    QSpinBox* customHeightSpin = nullptr;
    QDoubleSpinBox* exposureSpin = nullptr;
    QComboBox* readoutCombo = nullptr;
    QComboBox* binCombo = nullptr;

    QSpinBox* lutMinSpin = nullptr;
    QSpinBox* lutMaxSpin = nullptr;
    QSlider* lutMinSlider = nullptr;
    QSlider* lutMaxSlider = nullptr;
    QSpinBox* displayEverySpin = nullptr;
    QLabel* lutRangeLabel = nullptr;

    QLineEdit* savePathEdit = nullptr;
    QPushButton* saveBrowseButton = nullptr;
    QPushButton* saveOpenButton = nullptr;
    QPushButton* saveStartButton = nullptr;
    QPushButton* saveStopButton = nullptr;
    QLabel* saveInfoLabel = nullptr;
};

QWidget* buildCameraControlsStack(const CameraWorkspaceControls& controls);
QString refreshCameraFormatOptions(QComboBox* presetCombo,
                                   QComboBox* bitsCombo,
                                   QComboBox* readoutCombo,
                                   QSpinBox* customWidthSpin,
                                   QSpinBox* customHeightSpin,
                                   QDoubleSpinBox* exposureSpin,
                                   const QVariantMap& options);

}  // namespace desktop_app::workspace
