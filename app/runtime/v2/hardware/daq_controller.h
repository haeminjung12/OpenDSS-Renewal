#pragma once

#include "../../daq_trigger.h"
#include "../state/domain_state.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <functional>
#include <string>
#include <vector>

namespace desktop_app::v2 {

class ApplicationStateStore;
class DaqService;
class OperationCoordinator;

using DaqDiscoveryFunction =
    std::function<std::vector<DaqDeviceInfo>(std::string &error)>;

class DaqController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY discoveryChanged)
    Q_PROPERTY(QStringList outputChannels READ outputChannels NOTIFY discoveryChanged)
    Q_PROPERTY(QString selectedOutputChannel READ selectedOutputChannel
                   WRITE setSelectedOutputChannel NOTIFY settingsChanged)
    Q_PROPERTY(double amplitudeVpp READ amplitudeVpp WRITE setAmplitudeVpp
                   NOTIFY settingsChanged)
    Q_PROPERTY(double frequencyHz READ frequencyHz WRITE setFrequencyHz
                   NOTIFY settingsChanged)
    Q_PROPERTY(double durationMs READ durationMs WRITE setDurationMs
                   NOTIFY settingsChanged)
    Q_PROPERTY(double delayMs READ delayMs WRITE setDelayMs NOTIFY settingsChanged)
    Q_PROPERTY(QString daqStatus READ daqStatus NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool canApply READ canApply NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    DaqController(DaqService &service, ApplicationStateStore &stateStore,
                  OperationCoordinator &operations,
                  DaqDiscoveryFunction discovery = {},
                  QObject *parent = nullptr);

    QVariantList devices() const;
    QStringList outputChannels() const;
    QString selectedOutputChannel() const;
    void setSelectedOutputChannel(const QString &channel);
    double amplitudeVpp() const;
    void setAmplitudeVpp(double amplitudeVpp);
    double frequencyHz() const;
    void setFrequencyHz(double frequencyHz);
    double durationMs() const;
    void setDurationMs(double durationMs);
    double delayMs() const;
    void setDelayMs(double delayMs);
    QString daqStatus() const;
    bool ready() const;
    bool canApply() const;
    QString error() const;

    Q_INVOKABLE bool refreshDevices();
    Q_INVOKABLE bool apply();

signals:
    void discoveryChanged();
    void settingsChanged();
    void stateChanged();

private:
    DaqAppliedSettings draftSettings() const;
    bool selectedChannelExists() const;
    void setActionError(const QString &error);
    void restoreAppliedSettings();

    DaqService &service_;
    ApplicationStateStore &stateStore_;
    OperationCoordinator &operations_;
    DaqDiscoveryFunction discovery_;
    std::vector<DaqDeviceInfo> discoveredDevices_;
    QString selectedOutputChannel_;
    double amplitudeVpp_ = 5.0;
    double frequencyHz_ = 10000.0;
    double durationMs_ = 5.0;
    double delayMs_ = 0.0;
    QString actionError_;
    DaqAppliedSettings observedAppliedSettings_;
    bool actionInProgress_ = false;
};

} // namespace desktop_app::v2
