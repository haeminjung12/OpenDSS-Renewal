#include "daq_service.h"

#include "../../daq_trigger.h"
#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <cmath>
#include <string>

namespace desktop_app::v2 {
namespace {

constexpr double kAmplitudeVpp = 5.0;
constexpr double kPeakAmplitudeVolts = kAmplitudeVpp / 2.0;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString validationError(const DaqAppliedSettings &settings)
{
    const QString channel = settings.outputChannel.trimmed();
    const qsizetype slash = channel.indexOf(QLatin1Char('/'));
    if (channel.isEmpty() || channel != settings.outputChannel ||
        slash <= 0 || slash == channel.size() - 1 ||
        channel.indexOf(QLatin1Char('/'), slash + 1) >= 0 ||
        channel.contains(QLatin1Char(',')) ||
        !channel.sliced(slash + 1).startsWith(QStringLiteral("ao")) ||
        channel.sliced(slash + 3).isEmpty()) {
        return QStringLiteral("DAQ Output Channel must be a direct device analog-output channel such as Dev1/ao0.");
    }
    for (const QChar character : channel.sliced(slash + 3)) {
        if (!character.isDigit())
            return QStringLiteral("DAQ Output Channel must be a direct device analog-output channel such as Dev1/ao0.");
    }
    if (!std::isfinite(settings.frequencyHz) || settings.frequencyHz <= 0.0)
        return QStringLiteral("DAQ frequency must be finite and greater than zero.");
    if (!std::isfinite(settings.durationMs) || settings.durationMs <= 0.0)
        return QStringLiteral("DAQ duration must be finite and greater than zero.");
    if (!std::isfinite(settings.delayMs) || settings.delayMs < 0.0)
        return QStringLiteral("DAQ delay must be finite and zero or greater.");
    return {};
}

QString factualError(const QString &fallback, const std::string &deviceError)
{
    const QString detail = QString::fromStdString(deviceError).trimmed();
    return detail.isEmpty() ? fallback : detail;
}

} // namespace

DaqService::DaqService(OperationCoordinator &operations,
                       ApplicationStateStore &stateStore)
    : operations_(operations)
    , stateStore_(stateStore)
{
    state_.status = DaqStatus::Disabled;
    stateStore_.publishDaq(state_);
}

DaqService::~DaqService()
{
    shutdown();
}

bool DaqService::applySettings(const DaqAppliedSettings &settings, QString *error)
{
    std::unique_lock lock(mutex_);
    const QString invalid = validationError(settings);
    if (!invalid.isEmpty()) {
        setError(error, invalid);
        return false;
    }

    auto acquired = operations_.acquireMomentary(ResourceLock::Daq);
    if (!acquired.acquired()) {
        const QString message =
            acquired.fault ? acquired.fault->reason
                           : QStringLiteral("DAQ settings are locked by another operation.");
        setError(error, message);
        return false;
    }

    DaqConfig config;
    config.channel = settings.outputChannel.toStdString();
    config.amplitude = kPeakAmplitudeVolts;
    config.frequencyHz = settings.frequencyHz;
    config.durationMs = settings.durationMs;
    config.delayMs = settings.delayMs;

    auto candidate = std::make_unique<DaqTrigger>();
    std::string deviceError;
    if (!candidate->init(config, deviceError)) {
        const QString message =
            factualError(QStringLiteral("The DAQ settings could not be applied."),
                         deviceError);
        const DaqState published =
            updateStateLocked(trigger_ && trigger_->isReady()
                                  ? DaqStatus::Ready
                                  : DaqStatus::Faulted,
                              message);
        setError(error, message);
        lock.unlock();
        stateStore_.publishDaq(published);
        return false;
    }

    trigger_.swap(candidate);
    state_.appliedSettings = settings;
    const DaqState published = updateStateLocked(DaqStatus::Ready);
    setError(error, {});
    lock.unlock();
    stateStore_.publishDaq(published);
    return true;
}

bool DaqService::ready() const
{
    std::lock_guard lock(mutex_);
    return state_.status == DaqStatus::Ready && trigger_ && trigger_->isReady();
}

QJsonObject DaqService::settingsSnapshot() const
{
    std::lock_guard lock(mutex_);
    const DaqAppliedSettings &settings = state_.appliedSettings;
    return {
        {QStringLiteral("channel"), settings.outputChannel},
        {QStringLiteral("frequency_hz"), settings.frequencyHz},
        {QStringLiteral("duration_ms"), settings.durationMs},
        {QStringLiteral("delay_ms"), settings.delayMs},
        {QStringLiteral("amplitude_vpp"), kAmplitudeVpp},
    };
}

run::DaqPulseStatus DaqService::issueLiveHit(bool outputEnabled, QString *error)
{
    std::unique_lock lock(mutex_);
    if (!outputEnabled) {
        setError(error, {});
        return run::DaqPulseStatus::SuppressedNotIssued;
    }
    if (!trigger_ || !trigger_->isReady() || state_.status != DaqStatus::Ready) {
        const QString message = QStringLiteral("DAQ is not ready.");
        if (trigger_)
            trigger_->shutdown();
        const DaqState published = updateStateLocked(DaqStatus::Faulted, message);
        setError(error, message);
        lock.unlock();
        stateStore_.publishDaq(published);
        return run::DaqPulseStatus::Failed;
    }

    std::string deviceError;
    if (!trigger_->fire(deviceError)) {
        const QString message =
            factualError(QStringLiteral("The DAQ Hit pulse failed."), deviceError);
        trigger_->shutdown();
        const DaqState published = updateStateLocked(DaqStatus::Faulted, message);
        setError(error, message);
        lock.unlock();
        stateStore_.publishDaq(published);
        return run::DaqPulseStatus::Failed;
    }

    setError(error, {});
    return run::DaqPulseStatus::Issued;
}

void DaqService::shutdown()
{
    std::unique_lock lock(mutex_);
    if (trigger_)
        trigger_->shutdown();
    trigger_.reset();
    const DaqState published = updateStateLocked(DaqStatus::Disabled);
    lock.unlock();
    stateStore_.publishDaq(published);
}

DaqState DaqService::updateStateLocked(DaqStatus status, const QString &fault)
{
    state_.status = status;
    state_.deviceId =
        state_.appliedSettings.outputChannel.section(QLatin1Char('/'), 0, 0);
    state_.fault = fault;
    return state_;
}

} // namespace desktop_app::v2
