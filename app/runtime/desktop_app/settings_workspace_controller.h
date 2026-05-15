#pragma once

#include <functional>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include "../daq_trigger.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTimer;

namespace desktop_app {
struct AppState;
}

class SettingsWorkspaceController : public QObject {
    Q_OBJECT

public:
    struct DaqAvailabilityState {
        bool available = false;
        bool disabled = false;
        bool fault = false;
        QString statusText;
        QString faultText;
        QString indicatorText;
        QString indicatorColor;
    };

    using ReloadPipelineCallback = std::function<void(bool)>;
    using UpdateForceTriggerCallback = std::function<void()>;

    struct Dependencies {
        desktop_app::AppState* appState = nullptr;
        bool noDaq = false;
        QComboBox* daqDeviceCombo = nullptr;
        QLineEdit* daqChannelEdit = nullptr;
        QDoubleSpinBox* amplitudeSpin = nullptr;
        QDoubleSpinBox* frequencySpin = nullptr;
        QDoubleSpinBox* durationSpin = nullptr;
        QDoubleSpinBox* delaySpin = nullptr;
        QLabel* labviewStatusDot = nullptr;
        QLabel* labviewStatusText = nullptr;
        QLabel* labviewOutputLabel = nullptr;
        QLabel* daqStatusItem = nullptr;
        QCheckBox* pipelineEnableCheck = nullptr;
        bool* viewerOnly = nullptr;
    };

    explicit SettingsWorkspaceController(const Dependencies& dependencies, QObject* parent = nullptr);

    void setReloadPipelineCallback(ReloadPipelineCallback callback);
    void setUpdateForceTriggerCallback(UpdateForceTriggerCallback callback);

    void refreshDaqDeviceOptions(bool allowChannelRewrite);
    DaqConfig currentDaqConfig() const;
    DaqAvailabilityState probeDaqAvailability() const;
    void applyDaqAvailability(const DaqAvailabilityState& state);
    void setLabviewStatus(const QString& text, const QString& color);
    void updateLabviewOutput();
    void scheduleLabviewApply();

    QString describeDiscoveredDaqDevices() const;
    QString daqDiscoveryError() const;
    int discoveredCompatibleDeviceCount() const;
    const std::vector<DaqDeviceInfo>& discoveredDaqDevices() const;

private:
    static bool sameText(const QString& left, const QString& right);
    static QString channelDeviceName(const QString& channel);
    static QString channelSuffix(const QString& channel);
    static std::vector<DaqDeviceInfo> parseSimulatedDaqDevices();

    const DaqDeviceInfo* findDiscoveredDaqDevice(const QString& deviceName) const;
    QString formatDaqDeviceLabel(const DaqDeviceInfo& device) const;
    QString chooseChannelForDevice(const DaqDeviceInfo& device,
                                   const QString& currentChannel,
                                   const QString& previousDeviceName) const;
    void syncDaqDeviceComboToChannel();
    void wireControls();
    void handleLabviewApplyTimeout();

    Dependencies deps_;
    QTimer* labviewApplyTimer_ = nullptr;
    bool autoApplyLabview_ = true;
    std::vector<DaqDeviceInfo> discoveredDaqDevices_;
    QString daqDiscoveryError_;
    ReloadPipelineCallback reloadPipeline_;
    UpdateForceTriggerCallback updateForceTriggerState_;
};
