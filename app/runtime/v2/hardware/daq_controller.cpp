#include "daq_controller.h"

#include "daq_service.h"
#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <QVariantMap>
#include <QMutexLocker>
#include <QThread>

#include <utility>

namespace desktop_app::v2 {
namespace {

QString statusText(DaqStatus status)
{
    switch (status) {
    case DaqStatus::Disabled:
    case DaqStatus::Faulted:
        return QStringLiteral("Unavailable");
    case DaqStatus::Ready:
        return QStringLiteral("Ready");
    case DaqStatus::Busy:
        return QStringLiteral("Active");
    }
    return QStringLiteral("Unavailable");
}

bool activeSortingOwnsDaq(const OperationSnapshot &snapshot)
{
    return snapshot.kind &&
           (*snapshot.kind == OperationKind::LiveSorting ||
            *snapshot.kind == OperationKind::SequenceTest) &&
           snapshot.locks.testFlag(ResourceLock::Daq) &&
           (snapshot.lifecycle == OperationLifecycle::Starting ||
            snapshot.lifecycle == OperationLifecycle::Running ||
            snapshot.lifecycle == OperationLifecycle::Paused);
}

std::vector<DaqDeviceInfo> discover(std::string &error)
{
    return discoverDaqDevices(error);
}

bool sameSettings(const DaqAppliedSettings &left,
                  const DaqAppliedSettings &right)
{
    return left.outputChannel == right.outputChannel
        && left.amplitudeVpp == right.amplitudeVpp
        && left.frequencyHz == right.frequencyHz
        && left.durationMs == right.durationMs
        && left.delayMs == right.delayMs;
}

} // namespace

DaqController::DaqController(DaqService &service,
                             ApplicationStateStore &stateStore,
                             OperationCoordinator &operations,
                             DaqDiscoveryFunction discoveryFunction,
                             QObject *parent)
    : QObject(parent)
    , service_(service)
    , stateStore_(stateStore)
    , operations_(operations)
    , discovery_(discoveryFunction ? std::move(discoveryFunction)
                                  : DaqDiscoveryFunction(discover))
{
    restoreAppliedSettings();
    connect(&stateStore_, &ApplicationStateStore::changed, this, [this] {
        const DaqAppliedSettings applied =
            stateStore_.snapshot().daq.appliedSettings;
        if (!sameSettings(applied, observedAppliedSettings_)) {
            observedAppliedSettings_ = applied;
            if (!applyThread_) {
                restoreAppliedSettings();
                emit settingsChanged();
            }
        }
        emit stateChanged();
    });
    connect(&operations_, &OperationCoordinator::resourcesChanged,
            this, &DaqController::stateChanged);
    refreshDevices();
}

DaqController::~DaqController()
{
    if (!applyThread_)
        return;
    disconnect(applyThread_, nullptr, this, nullptr);
    applyThread_->wait();
    delete applyThread_;
}

QVariantList DaqController::devices() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(discoveredDevices_.size()));
    for (const DaqDeviceInfo &device : discoveredDevices_) {
        QStringList channels;
        channels.reserve(static_cast<qsizetype>(device.aoChannels.size()));
        for (const std::string &channel : device.aoChannels)
            channels.push_back(QString::fromStdString(channel));
        result.push_back(QVariantMap{
            {QStringLiteral("deviceId"), QString::fromStdString(device.name)},
            {QStringLiteral("productType"),
             QString::fromStdString(device.productType)},
            {QStringLiteral("outputChannels"), channels},
        });
    }
    return result;
}

QStringList DaqController::outputChannels() const
{
    QStringList result;
    for (const DaqDeviceInfo &device : discoveredDevices_) {
        for (const std::string &channel : device.aoChannels)
            result.push_back(QString::fromStdString(channel));
    }
    return result;
}

QString DaqController::selectedOutputChannel() const
{
    return selectedOutputChannel_;
}

void DaqController::setSelectedOutputChannel(const QString &channel)
{
    if (selectedOutputChannel_ == channel)
        return;
    selectedOutputChannel_ = channel;
    emit settingsChanged();
    emit stateChanged();
}

double DaqController::amplitudeVpp() const
{
    return amplitudeVpp_;
}

void DaqController::setAmplitudeVpp(double amplitudeVpp)
{
    if (amplitudeVpp_ == amplitudeVpp)
        return;
    amplitudeVpp_ = amplitudeVpp;
    emit settingsChanged();
    emit stateChanged();
}

double DaqController::frequencyHz() const
{
    return frequencyHz_;
}

void DaqController::setFrequencyHz(double frequencyHz)
{
    if (frequencyHz_ == frequencyHz)
        return;
    frequencyHz_ = frequencyHz;
    emit settingsChanged();
    emit stateChanged();
}

double DaqController::durationMs() const
{
    return durationMs_;
}

void DaqController::setDurationMs(double durationMs)
{
    if (durationMs_ == durationMs)
        return;
    durationMs_ = durationMs;
    emit settingsChanged();
    emit stateChanged();
}

double DaqController::delayMs() const
{
    return delayMs_;
}

void DaqController::setDelayMs(double delayMs)
{
    if (delayMs_ == delayMs)
        return;
    delayMs_ = delayMs;
    emit settingsChanged();
    emit stateChanged();
}

QString DaqController::daqStatus() const
{
    return statusText(stateStore_.snapshot().daq.status);
}

bool DaqController::ready() const
{
    return stateStore_.snapshot().daq.status == DaqStatus::Ready;
}

bool DaqController::canApply() const
{
    if (!selectedChannelExists()
        || !DaqService::settingsValidationError(draftSettings()).isEmpty()) {
        return false;
    }
    if (applyThread_)
        return true;
    return !actionInProgress_ && !service_.continuousActive() &&
           (operations_.momentaryAvailable(ResourceLock::Daq) ||
            activeSortingOwnsDaq(operations_.snapshot()));
}

bool DaqController::applyInProgress() const { return applyThread_ != nullptr; }

bool DaqController::continuousWaveformActive() const
{
    return service_.continuousActive();
}

QString DaqController::error() const
{
    const QString serviceFault = stateStore_.snapshot().daq.fault;
    return serviceFault.isEmpty() ? actionError_ : serviceFault;
}

bool DaqController::refreshDevices()
{
    if (actionInProgress_)
        return false;

    actionInProgress_ = true;
    emit stateChanged();
    std::string discoveryError;
    std::vector<DaqDeviceInfo> discovered = discovery_(discoveryError);
    discoveredDevices_ = std::move(discovered);

    const QString previousSelection = selectedOutputChannel_;
    if (!selectedChannelExists()) {
        const QStringList channels = outputChannels();
        const QString appliedChannel =
            stateStore_.snapshot().daq.appliedSettings.outputChannel;
        selectedOutputChannel_ =
            channels.contains(appliedChannel)
                ? appliedChannel
                : (channels.isEmpty() ? QString() : channels.constFirst());
    }

    const QString factualDiscoveryError =
        QString::fromStdString(discoveryError).trimmed();
    const QString appliedChannel =
        stateStore_.snapshot().daq.appliedSettings.outputChannel;
    const bool configuredChannelMissing =
        ready() && !outputChannels().contains(appliedChannel);
    QString unavailableReason;
    if (outputChannels().isEmpty()
        && !factualDiscoveryError.isEmpty()) {
        unavailableReason = factualDiscoveryError;
    } else if (configuredChannelMissing) {
        unavailableReason = outputChannels().isEmpty()
            ? QStringLiteral("No DAQ analog-output channels were found.")
            : QStringLiteral(
                  "The configured DAQ output channel is no longer available.");
    }

    if (ready() && !unavailableReason.isEmpty()) {
        service_.markUnavailable(unavailableReason);
        setActionError({});
    } else if (!factualDiscoveryError.isEmpty()) {
        setActionError(factualDiscoveryError);
    } else if (outputChannels().isEmpty()) {
        setActionError(
            QStringLiteral("No DAQ analog-output channels were found."));
    } else {
        setActionError({});
    }

    actionInProgress_ = false;
    emit discoveryChanged();
    if (selectedOutputChannel_ != previousSelection)
        emit settingsChanged();
    emit stateChanged();
    return !outputChannels().isEmpty();
}

bool DaqController::apply()
{
    const DaqAppliedSettings settings = draftSettings();
    const QString invalid = DaqService::settingsValidationError(settings);
    if (!invalid.isEmpty()) {
        restoreAppliedSettings();
        setActionError(invalid);
        emit settingsChanged();
        emit stateChanged();
        return false;
    }
    if (!selectedChannelExists()) {
        restoreAppliedSettings();
        setActionError(
            QStringLiteral("Select an available DAQ analog-output channel."));
        emit settingsChanged();
        emit stateChanged();
        return false;
    }
    if (applyThread_) {
        pendingSettings_ = settings;
        hasPendingSettings_ = true;
        setActionError({});
        emit stateChanged();
        return true;
    }
    if (actionInProgress_)
        return false;
    if (!operations_.momentaryAvailable(ResourceLock::Daq) &&
        !activeSortingOwnsDaq(operations_.snapshot())) {
        restoreAppliedSettings();
        setActionError(
            QStringLiteral("DAQ settings are locked by another operation."));
        emit settingsChanged();
        emit stateChanged();
        return false;
    }

    startApply(settings);
    return true;
}

void DaqController::startApply(const DaqAppliedSettings &settings)
{
    actionInProgress_ = true;
    setActionError({});
    applyThread_ = QThread::create([this, settings] {
        QString error;
        const bool succeeded = service_.applySettings(settings, &error, true);
        const QMutexLocker lock(&applyResultMutex_);
        applySucceeded_ = succeeded;
        applyError_ = std::move(error);
    });
    connect(applyThread_, &QThread::finished, this, &DaqController::finishApply);
    applyThread_->start();
    emit stateChanged();
}

void DaqController::finishApply()
{
    QThread *completedThread = applyThread_;
    applyThread_ = nullptr;
    if (completedThread)
        completedThread->deleteLater();

    bool succeeded = false;
    QString error;
    {
        const QMutexLocker lock(&applyResultMutex_);
        succeeded = applySucceeded_;
        error = applyError_;
    }

    if (!succeeded) {
        hasPendingSettings_ = false;
        actionInProgress_ = false;
        restoreAppliedSettings();
        setActionError(error.isEmpty()
                           ? QStringLiteral("The DAQ settings could not be applied.")
                           : error);
        emit settingsChanged();
        emit stateChanged();
        return;
    }

    if (hasPendingSettings_
        && !sameSettings(pendingSettings_,
                         stateStore_.snapshot().daq.appliedSettings)) {
        const DaqAppliedSettings nextSettings = pendingSettings_;
        hasPendingSettings_ = false;
        startApply(nextSettings);
        return;
    }

    hasPendingSettings_ = false;
    actionInProgress_ = false;
    restoreAppliedSettings();
    setActionError({});
    emit settingsChanged();
    emit stateChanged();
}

bool DaqController::sendTestSineWave()
{
    if (actionInProgress_) {
        setActionError(QStringLiteral("A DAQ action is already in progress."));
        return false;
    }
    actionInProgress_ = true;
    QString error;
    const bool sent = service_.sendTestSine(&error);
    actionInProgress_ = false;
    if (!sent) {
        setActionError(error.isEmpty()
                           ? QStringLiteral("The DAQ test sine wave could not be sent.")
                           : error);
        emit stateChanged();
        return false;
    }
    setActionError({});
    emit stateChanged();
    return true;
}

bool DaqController::toggleContinuousWaveform()
{
    if (actionInProgress_) {
        setActionError(QStringLiteral("A DAQ action is already in progress."));
        emit stateChanged();
        return false;
    }
    actionInProgress_ = true;
    emit stateChanged();
    QString error;
    const bool completed = service_.continuousActive()
        ? service_.stopContinuous(&error)
        : service_.startContinuous(&error);
    actionInProgress_ = false;
    setActionError(completed ? QString() : error);
    emit stateChanged();
    return completed;
}

DaqAppliedSettings DaqController::draftSettings() const
{
    DaqAppliedSettings settings;
    settings.outputChannel = selectedOutputChannel_;
    settings.amplitudeVpp = amplitudeVpp_;
    settings.frequencyHz = frequencyHz_;
    settings.durationMs = durationMs_;
    settings.delayMs = delayMs_;
    return settings;
}

bool DaqController::selectedChannelExists() const
{
    for (const DaqDeviceInfo &device : discoveredDevices_) {
        for (const std::string &channel : device.aoChannels) {
            if (selectedOutputChannel_ == QString::fromStdString(channel))
                return true;
        }
    }
    return false;
}

void DaqController::setActionError(const QString &error)
{
    actionError_ = error;
}

void DaqController::restoreAppliedSettings()
{
    const DaqAppliedSettings settings =
        stateStore_.snapshot().daq.appliedSettings;
    observedAppliedSettings_ = settings;
    if (outputChannels().contains(settings.outputChannel))
        selectedOutputChannel_ = settings.outputChannel;
    amplitudeVpp_ = settings.amplitudeVpp;
    frequencyHz_ = settings.frequencyHz;
    durationMs_ = settings.durationMs;
    delayMs_ = settings.delayMs;
}

} // namespace desktop_app::v2
