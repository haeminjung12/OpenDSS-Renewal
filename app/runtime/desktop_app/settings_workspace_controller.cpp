#include "settings_workspace_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include <QtCore/QSettings>
#include <QtCore/QSignalBlocker>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/qtenvironmentvariables.h>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

#include "app_state.h"

SettingsWorkspaceController::SettingsWorkspaceController(const Dependencies& dependencies, QObject* parent)
    : QObject(parent), deps_(dependencies), labviewApplyTimer_(new QTimer(this)) {
    labviewApplyTimer_->setSingleShot(true);
    labviewApplyTimer_->setInterval(300);
    connect(labviewApplyTimer_, &QTimer::timeout, this, &SettingsWorkspaceController::handleLabviewApplyTimeout);

    refreshDaqDeviceOptions(false);
    updateLabviewOutput();
    setLabviewStatus("Disconnected", "#666");
    wireControls();
}

void SettingsWorkspaceController::setReloadPipelineCallback(ReloadPipelineCallback callback) {
    reloadPipeline_ = std::move(callback);
}

void SettingsWorkspaceController::setUpdateForceTriggerCallback(UpdateForceTriggerCallback callback) {
    updateForceTriggerState_ = std::move(callback);
}

bool SettingsWorkspaceController::sameText(const QString& left, const QString& right) {
    return left.compare(right, Qt::CaseInsensitive) == 0;
}

QString SettingsWorkspaceController::channelDeviceName(const QString& channel) {
    const QString trimmed = channel.trimmed();
    const int slash = trimmed.indexOf('/');
    return slash > 0 ? trimmed.left(slash) : trimmed;
}

QString SettingsWorkspaceController::channelSuffix(const QString& channel) {
    const QString trimmed = channel.trimmed();
    const int slash = trimmed.indexOf('/');
    return (slash >= 0 && slash + 1 < trimmed.size()) ? trimmed.mid(slash + 1) : QString();
}

std::vector<DaqDeviceInfo> SettingsWorkspaceController::parseSimulatedDaqDevices() {
    std::vector<DaqDeviceInfo> devices;
    const QString raw = qEnvironmentVariable("OVDS_DAQ_SIMULATED_DEVICES").trimmed();
    if (raw.isEmpty()) {
        return devices;
    }

    const QStringList deviceSpecs = raw.split(';', Qt::SkipEmptyParts);
    for (const QString& deviceSpec : deviceSpecs) {
        const QStringList parts = deviceSpec.split('|');
        if (parts.isEmpty()) {
            continue;
        }

        DaqDeviceInfo info;
        info.name = parts.value(0).trimmed().toStdString();
        info.productType = parts.value(1).trimmed().toStdString();
        const QStringList channels = parts.value(2).split(',', Qt::SkipEmptyParts);
        for (const QString& channel : channels) {
            const QString trimmedChannel = channel.trimmed();
            if (!trimmedChannel.isEmpty()) {
                info.aoChannels.push_back(trimmedChannel.toStdString());
            }
        }
        if (!info.name.empty()) {
            devices.push_back(std::move(info));
        }
    }
    std::sort(devices.begin(), devices.end(),
              [](const DaqDeviceInfo& left, const DaqDeviceInfo& right) { return left.name < right.name; });
    return devices;
}

const DaqDeviceInfo* SettingsWorkspaceController::findDiscoveredDaqDevice(const QString& deviceName) const {
    for (const auto& device : discoveredDaqDevices_) {
        if (sameText(QString::fromStdString(device.name), deviceName.trimmed())) {
            return &device;
        }
    }
    return nullptr;
}

int SettingsWorkspaceController::discoveredCompatibleDeviceCount() const {
    int count = 0;
    for (const auto& device : discoveredDaqDevices_) {
        if (device.isCompatible()) {
            ++count;
        }
    }
    return count;
}

QString SettingsWorkspaceController::formatDaqDeviceLabel(const DaqDeviceInfo& device) const {
    QString label = QString::fromStdString(device.name);
    const QString product = QString::fromStdString(device.productType).trimmed();
    if (!product.isEmpty()) {
        label += QStringLiteral(" - %1").arg(product);
    }
    if (!device.isCompatible()) {
        label += QStringLiteral(" (no AO output)");
    }
    return label;
}

QString SettingsWorkspaceController::describeDiscoveredDaqDevices() const {
    QStringList parts;
    for (const auto& device : discoveredDaqDevices_) {
        QString part = formatDaqDeviceLabel(device);
        if (device.isCompatible()) {
            QStringList channels;
            for (const std::string& channel : device.aoChannels) {
                channels << QString::fromStdString(channel);
            }
            if (!channels.isEmpty()) {
                part += QStringLiteral(" [%1]").arg(channels.join(", "));
            }
        }
        parts << part;
    }
    return parts.join("; ");
}

QString SettingsWorkspaceController::chooseChannelForDevice(const DaqDeviceInfo& device, const QString& currentChannel,
                                                            const QString& previousDeviceName) const {
    if (!device.isCompatible()) {
        return {};
    }

    const QString trimmedChannel = currentChannel.trimmed();
    const QString selectedDeviceName = QString::fromStdString(device.name);
    auto resolveExactChannel = [&](const QString& candidate) {
        for (const std::string& channel : device.aoChannels) {
            const QString discoveredChannel = QString::fromStdString(channel);
            if (sameText(discoveredChannel, candidate)) {
                return discoveredChannel;
            }
        }
        return QString();
    };

    if (sameText(channelDeviceName(trimmedChannel), selectedDeviceName)) {
        const QString exactCurrent = resolveExactChannel(trimmedChannel);
        if (!exactCurrent.isEmpty()) {
            return exactCurrent;
        }
    }

    const QString suffix = channelSuffix(trimmedChannel);
    if (!suffix.isEmpty() &&
        (trimmedChannel.isEmpty() || sameText(channelDeviceName(trimmedChannel), previousDeviceName) ||
         sameText(channelDeviceName(trimmedChannel), selectedDeviceName))) {
        const QString directCandidate = resolveExactChannel(selectedDeviceName + "/" + suffix);
        if (!directCandidate.isEmpty()) {
            return directCandidate;
        }
        for (const std::string& channel : device.aoChannels) {
            const QString discoveredChannel = QString::fromStdString(channel);
            if (sameText(channelSuffix(discoveredChannel), suffix)) {
                return discoveredChannel;
            }
        }
    }

    return QString::fromStdString(device.preferredChannel());
}

void SettingsWorkspaceController::syncDaqDeviceComboToChannel() {
    if (!deps_.daqDeviceCombo || !deps_.daqChannelEdit) {
        return;
    }
    const QString deviceName = channelDeviceName(deps_.daqChannelEdit->text());
    if (deviceName.isEmpty()) {
        return;
    }
    for (int i = 0; i < deps_.daqDeviceCombo->count(); ++i) {
        if (sameText(deps_.daqDeviceCombo->itemData(i).toString(), deviceName)) {
            QSignalBlocker blocker(deps_.daqDeviceCombo);
            deps_.daqDeviceCombo->setCurrentIndex(i);
            break;
        }
    }
}

void SettingsWorkspaceController::refreshDaqDeviceOptions(bool allowChannelRewrite) {
    if (!deps_.daqDeviceCombo || !deps_.daqChannelEdit) {
        return;
    }

    const std::vector<DaqDeviceInfo> simulatedDevices = parseSimulatedDaqDevices();
    std::string discoveryErr;
    discoveredDaqDevices_ = simulatedDevices.empty() ? discoverDaqDevices(discoveryErr) : simulatedDevices;
    daqDiscoveryError_ = QString::fromStdString(discoveryErr).trimmed();
    if (!simulatedDevices.empty()) {
        daqDiscoveryError_ = QStringLiteral("Simulated DAQ discovery override active");
    }

    QSettings settings;
    const QString savedDevice = settings.value("settings/daqSelectedDevice").toString().trimmed();
    const QString currentChannelText = deps_.daqChannelEdit->text().trimmed();
    const QString currentDevice = channelDeviceName(currentChannelText);

    QSignalBlocker blocker(deps_.daqDeviceCombo);
    deps_.daqDeviceCombo->clear();
    for (const auto& device : discoveredDaqDevices_) {
        deps_.daqDeviceCombo->addItem(formatDaqDeviceLabel(device), QString::fromStdString(device.name));
    }

    QString chosenDevice;
    if (discoveredCompatibleDeviceCount() == 1) {
        for (const auto& device : discoveredDaqDevices_) {
            if (device.isCompatible()) {
                chosenDevice = QString::fromStdString(device.name);
                break;
            }
        }
    } else if (discoveredCompatibleDeviceCount() > 1) {
        if (const DaqDeviceInfo* saved = findDiscoveredDaqDevice(savedDevice); saved && saved->isCompatible()) {
            chosenDevice = savedDevice;
        } else if (const DaqDeviceInfo* current = findDiscoveredDaqDevice(currentDevice);
                   current && current->isCompatible()) {
            chosenDevice = currentDevice;
        } else {
            for (const auto& device : discoveredDaqDevices_) {
                if (device.isCompatible()) {
                    chosenDevice = QString::fromStdString(device.name);
                    break;
                }
            }
        }
    } else if (!discoveredDaqDevices_.empty()) {
        if (findDiscoveredDaqDevice(savedDevice)) {
            chosenDevice = savedDevice;
        } else if (findDiscoveredDaqDevice(currentDevice)) {
            chosenDevice = currentDevice;
        } else {
            chosenDevice = QString::fromStdString(discoveredDaqDevices_.front().name);
        }
    }

    if (discoveredDaqDevices_.empty()) {
        const QString emptyText = daqDiscoveryError_.isEmpty() ? QStringLiteral("No NI-DAQmx devices detected")
                                                               : QStringLiteral("DAQ discovery unavailable");
        deps_.daqDeviceCombo->addItem(emptyText, QString());
        deps_.daqDeviceCombo->setCurrentIndex(0);
        deps_.daqDeviceCombo->setEnabled(false);
        if (allowChannelRewrite) {
            deps_.daqChannelEdit->clear();
        }
        settings.setValue("settings/daqSelectedDevice", QString());
        settings.sync();
        return;
    }

    int chosenIndex = -1;
    for (int i = 0; i < deps_.daqDeviceCombo->count(); ++i) {
        if (sameText(deps_.daqDeviceCombo->itemData(i).toString(), chosenDevice)) {
            chosenIndex = i;
            break;
        }
    }
    if (chosenIndex < 0) {
        chosenIndex = 0;
    }
    deps_.daqDeviceCombo->setCurrentIndex(chosenIndex);
    deps_.daqDeviceCombo->setEnabled(deps_.daqDeviceCombo->count() > 0);

    const QString selectedDevice = deps_.daqDeviceCombo->currentData().toString().trimmed();
    settings.setValue("settings/daqSelectedDevice", selectedDevice);
    settings.sync();

    if (!allowChannelRewrite) {
        return;
    }

    if (const DaqDeviceInfo* selectedInfo = findDiscoveredDaqDevice(selectedDevice)) {
        const QString nextChannel = chooseChannelForDevice(*selectedInfo, currentChannelText, currentDevice);
        if (nextChannel != currentChannelText) {
            deps_.daqChannelEdit->setText(nextChannel);
        }
    } else if (!selectedDevice.isEmpty()) {
        deps_.daqChannelEdit->clear();
    }
}

DaqConfig SettingsWorkspaceController::currentDaqConfig() const {
    DaqConfig cfg;
    cfg.channel = deps_.daqChannelEdit ? deps_.daqChannelEdit->text().trimmed().toStdString() : std::string();
    cfg.rangeMin = -10.0;
    cfg.rangeMax = 10.0;
    cfg.amplitude = deps_.amplitudeSpin ? deps_.amplitudeSpin->value() : 0.0;
    cfg.frequencyHz = deps_.frequencySpin ? deps_.frequencySpin->value() * 1000.0 : 0.0;
    cfg.durationMs = deps_.durationSpin ? deps_.durationSpin->value() : 0.0;
    cfg.delayMs = deps_.delaySpin ? deps_.delaySpin->value() : 0.0;
    return cfg;
}

SettingsWorkspaceController::DaqAvailabilityState SettingsWorkspaceController::probeDaqAvailability() const {
    DaqAvailabilityState state;
    const DaqConfig cfg = currentDaqConfig();
    if (cfg.channel.empty()) {
        state.fault = true;
        state.statusText = QStringLiteral("DAQ: unavailable");
        if (discoveredDaqDevices_.empty()) {
            state.faultText =
                daqDiscoveryError_.isEmpty() ? QStringLiteral("No NI-DAQmx devices detected.") : daqDiscoveryError_;
        } else if (const DaqDeviceInfo* selectedInfo = findDiscoveredDaqDevice(
                       deps_.daqDeviceCombo ? deps_.daqDeviceCombo->currentData().toString() : QString());
                   selectedInfo && !selectedInfo->isCompatible()) {
            state.faultText = QStringLiteral("Selected device %1 does not report analog output channels in NI-DAQmx.")
                                  .arg(QString::fromStdString(selectedInfo->name));
        } else {
            state.faultText = QStringLiteral("No DAQ output channel is configured.");
        }
        state.indicatorText = QStringLiteral("Disabled");
        state.indicatorColor = QStringLiteral("#DC2626");
        return state;
    }

    DaqTrigger probeTrigger;
    std::string probeErr;
    if (probeTrigger.init(cfg, probeErr)) {
        state.available = true;
        state.statusText = QStringLiteral("DAQ: available");
        state.indicatorText = QStringLiteral("Connected");
        state.indicatorColor = QStringLiteral("#22C55E");
        return state;
    }

    state.fault = true;
    state.statusText = QStringLiteral("DAQ: unavailable");
    state.faultText = QString::fromStdString(probeErr);
    state.indicatorText = QStringLiteral("Disconnected");
    state.indicatorColor = QStringLiteral("#DC2626");
    return state;
}

void SettingsWorkspaceController::applyDaqAvailability(const DaqAvailabilityState& state) {
    setLabviewStatus(state.indicatorText, state.indicatorColor);
    if (deps_.daqStatusItem) {
        deps_.daqStatusItem->setText(state.statusText);
    }
    if (deps_.appState) {
        deps_.appState->daqAvailable = state.available;
        deps_.appState->daqDisabled = state.disabled;
        deps_.appState->daqFault = state.fault;
        deps_.appState->daqStatusText = state.statusText;
        deps_.appState->daqFaultText = state.faultText;
    }
}

void SettingsWorkspaceController::setLabviewStatus(const QString& text, const QString& color) {
    if (deps_.labviewStatusText) {
        deps_.labviewStatusText->setText(text);
    }
    if (deps_.labviewStatusDot) {
        deps_.labviewStatusDot->setStyleSheet(
            QString("background:%1;border-radius:7px;border:1px solid #94A3B8;").arg(color));
    }
}

void SettingsWorkspaceController::updateLabviewOutput() {
    if (!deps_.labviewOutputLabel || !deps_.daqChannelEdit || !deps_.amplitudeSpin || !deps_.frequencySpin ||
        !deps_.durationSpin || !deps_.delaySpin) {
        return;
    }
    const QString channel = deps_.daqChannelEdit->text().trimmed();
    if (channel.isEmpty()) {
        deps_.labviewOutputLabel->setText("Output: (disabled)");
        return;
    }
    deps_.labviewOutputLabel->setText(QString("Output: %1 | amp=%2 V freq=%3 kHz dur=%4 ms delay=%5 ms")
                                          .arg(channel)
                                          .arg(deps_.amplitudeSpin->value(), 0, 'f', 2)
                                          .arg(deps_.frequencySpin->value(), 0, 'f', 3)
                                          .arg(deps_.durationSpin->value(), 0, 'f', 2)
                                          .arg(deps_.delaySpin->value(), 0, 'f', 2));
}

void SettingsWorkspaceController::scheduleLabviewApply() {
    if (!autoApplyLabview_ || !labviewApplyTimer_) {
        return;
    }
    labviewApplyTimer_->start();
}

QString SettingsWorkspaceController::daqDiscoveryError() const {
    return daqDiscoveryError_;
}

const std::vector<DaqDeviceInfo>& SettingsWorkspaceController::discoveredDaqDevices() const {
    return discoveredDaqDevices_;
}

void SettingsWorkspaceController::wireControls() {
    if (deps_.daqDeviceCombo) {
        connect(deps_.daqDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
            const QString selectedDevice = deps_.daqDeviceCombo->currentData().toString().trimmed();
            QSettings settings;
            settings.setValue("settings/daqSelectedDevice", selectedDevice);
            settings.sync();
            if (const DaqDeviceInfo* device = findDiscoveredDaqDevice(selectedDevice)) {
                const QString nextChannel = chooseChannelForDevice(
                    *device, deps_.daqChannelEdit ? deps_.daqChannelEdit->text().trimmed() : QString(),
                    deps_.daqChannelEdit ? channelDeviceName(deps_.daqChannelEdit->text()) : QString());
                if (deps_.daqChannelEdit && nextChannel != deps_.daqChannelEdit->text().trimmed()) {
                    deps_.daqChannelEdit->setText(nextChannel);
                } else {
                    updateLabviewOutput();
                    scheduleLabviewApply();
                }
            } else if (deps_.daqChannelEdit) {
                deps_.daqChannelEdit->clear();
            }
        });
    }

    if (deps_.daqChannelEdit) {
        connect(deps_.daqChannelEdit, &QLineEdit::textChanged, this, [this]() {
            QSettings settings;
            settings.setValue("settings/daqChannel", deps_.daqChannelEdit->text().trimmed());
            settings.sync();
            syncDaqDeviceComboToChannel();
            updateLabviewOutput();
            scheduleLabviewApply();
        });
    }

    auto wireWaveformSpin = [this](QDoubleSpinBox* spin) {
        if (!spin) {
            return;
        }
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this]() {
            updateLabviewOutput();
            scheduleLabviewApply();
        });
    };
    wireWaveformSpin(deps_.amplitudeSpin);
    wireWaveformSpin(deps_.frequencySpin);
    wireWaveformSpin(deps_.durationSpin);
    wireWaveformSpin(deps_.delaySpin);
}

void SettingsWorkspaceController::handleLabviewApplyTimeout() {
    if (deps_.viewerOnly && *deps_.viewerOnly) {
        return;
    }
    const bool enableAfter = deps_.pipelineEnableCheck && deps_.pipelineEnableCheck->isChecked();
    if (reloadPipeline_) {
        reloadPipeline_(enableAfter);
    }
    if (updateForceTriggerState_) {
        updateForceTriggerState_();
    }
}
