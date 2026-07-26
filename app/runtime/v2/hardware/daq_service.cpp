#include "daq_service.h"

#include "daq_output.h"
#include "../../daq_trigger.h"
#include "../operation/operation_coordinator.h"
#include "../state/application_state_store.h"

#include <cmath>
#include <string>

namespace desktop_app::v2 {
namespace {

constexpr double kMinAmplitudeVpp = 0.0;
constexpr double kMaxAmplitudeVpp = 10.0;
constexpr double kMinFrequencyHz = 1000.0;
constexpr double kMaxFrequencyHz = 1000000.0;
constexpr double kMinDurationMs = 1.0;
constexpr double kMaxDurationMs = 500.0;
constexpr double kMinDelayMs = 0.0;
constexpr double kMaxDelayMs = 500.0;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

QString factualError(const QString &fallback, const QString &deviceError)
{
    const QString detail = deviceError.trimmed();
    return detail.isEmpty() ? fallback : detail;
}

} // namespace

DaqService::DaqService(OperationCoordinator &operations,
                       ApplicationStateStore &stateStore)
    : DaqService(operations, stateStore, std::make_unique<DaqTriggerOutput>())
{
}

DaqService::DaqService(OperationCoordinator &operations,
                       ApplicationStateStore &stateStore,
                       std::unique_ptr<IDaqOutput> output)
    : operations_(operations)
    , stateStore_(stateStore)
    , output_(std::move(output))
{
    state_.status = DaqStatus::Disabled;
    stateStore_.publishDaq(state_);
}

DaqService::~DaqService()
{
    shutdown();
}

QString DaqService::settingsValidationError(const DaqAppliedSettings &settings)
{
    const QString channel = settings.outputChannel.trimmed();
    const qsizetype slash = channel.indexOf(QLatin1Char('/'));
    if (channel.isEmpty() || channel != settings.outputChannel
        || slash <= 0 || slash == channel.size() - 1
        || channel.indexOf(QLatin1Char('/'), slash + 1) >= 0
        || channel.contains(QLatin1Char(','))
        || !channel.sliced(slash + 1).startsWith(QStringLiteral("ao"))
        || channel.sliced(slash + 3).isEmpty()) {
        return QStringLiteral(
            "DAQ Output Channel must be a direct device analog-output channel such as Dev1/ao0.");
    }
    for (const QChar character : channel.sliced(slash + 3)) {
        if (!character.isDigit()) {
            return QStringLiteral(
                "DAQ Output Channel must be a direct device analog-output channel such as Dev1/ao0.");
        }
    }
    if (!std::isfinite(settings.amplitudeVpp)
        || settings.amplitudeVpp < kMinAmplitudeVpp
        || settings.amplitudeVpp > kMaxAmplitudeVpp) {
        return QStringLiteral("DAQ amplitude must be between 0 and 10 Vpp.");
    }
    if (!std::isfinite(settings.frequencyHz)
        || settings.frequencyHz < kMinFrequencyHz
        || settings.frequencyHz > kMaxFrequencyHz) {
        return QStringLiteral(
            "DAQ frequency must be between 1,000 and 1,000,000 Hz.");
    }
    if (!std::isfinite(settings.durationMs)
        || settings.durationMs < kMinDurationMs
        || settings.durationMs > kMaxDurationMs) {
        return QStringLiteral("DAQ event duration must be between 1 and 500 ms.");
    }
    if (!std::isfinite(settings.delayMs)
        || settings.delayMs < kMinDelayMs
        || settings.delayMs > kMaxDelayMs) {
        return QStringLiteral(
            "DAQ decision-to-trigger delay must be between 0 and 500 ms.");
    }
    return {};
}

bool DaqService::applySettings(const DaqAppliedSettings &settings, QString *error)
{
    std::lock_guard operationOrderLock(operationOrderMutex_);
    const QString invalid = settingsValidationError(settings);
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

    std::unique_lock stateLock(stateMutex_);
    DaqConfig config;
    config.channel = settings.outputChannel.toStdString();
    config.amplitude = settings.amplitudeVpp / 2.0;
    config.frequencyHz = settings.frequencyHz;
    config.durationMs = settings.durationMs;
    config.delayMs = settings.delayMs;

    QString deviceError;
    if (!output_ || !output_->configure(config, &deviceError)) {
        const QString message =
            factualError(QStringLiteral("The DAQ settings could not be applied."),
                         deviceError);
        const bool retainedReady = output_ && output_->ready();
        if (!retainedReady && output_)
            output_->shutdown();
        const DaqState published =
            updateStateLocked(retainedReady ? DaqStatus::Ready
                                            : DaqStatus::Faulted,
                              message);
        setError(error, message);
        stateLock.unlock();
        stateStore_.publishDaq(published);
        return false;
    }

    state_.appliedSettings = settings;
    const DaqState published = updateStateLocked(DaqStatus::Ready);
    setError(error, {});
    stateLock.unlock();
    stateStore_.publishDaq(published);
    return true;
}

void DaqService::markUnavailable(const QString &reason)
{
    std::lock_guard operationOrderLock(operationOrderMutex_);
    auto acquired = operations_.acquireMomentary(ResourceLock::Daq);

    std::unique_lock stateLock(stateMutex_);
    const QString message =
        reason.trimmed().isEmpty() ? QStringLiteral("DAQ unavailable.")
                                   : reason.trimmed();
    if (acquired.acquired() && output_)
        output_->shutdown();
    const DaqState published = updateStateLocked(DaqStatus::Faulted, message);
    stateLock.unlock();
    stateStore_.publishDaq(published);
}

bool DaqService::ready() const
{
    std::lock_guard stateLock(stateMutex_);
    return state_.status == DaqStatus::Ready && output_ && output_->ready();
}

QJsonObject DaqService::settingsSnapshot() const
{
    std::lock_guard stateLock(stateMutex_);
    const DaqAppliedSettings &settings = state_.appliedSettings;
    return {
        {QStringLiteral("channel"), settings.outputChannel},
        {QStringLiteral("frequency_hz"), settings.frequencyHz},
        {QStringLiteral("duration_ms"), settings.durationMs},
        {QStringLiteral("delay_ms"), settings.delayMs},
        {QStringLiteral("amplitude_vpp"), settings.amplitudeVpp},
    };
}

run::DaqPulseStatus DaqService::issueLiveHit(bool outputEnabled, QString *error)
{
    std::lock_guard operationOrderLock(operationOrderMutex_);
    std::unique_lock stateLock(stateMutex_);
    if (!outputEnabled) {
        setError(error, {});
        return run::DaqPulseStatus::SuppressedNotIssued;
    }
    if (state_.status != DaqStatus::Ready) {
        const QString message =
            state_.fault.isEmpty() ? QStringLiteral("DAQ is not ready.")
                                   : state_.fault;
        setError(error, message);
        return run::DaqPulseStatus::Failed;
    }
    if (!output_ || !output_->ready()) {
        const QString message = QStringLiteral("DAQ is not ready.");
        if (output_)
            output_->shutdown();
        const DaqState published = updateStateLocked(DaqStatus::Faulted, message);
        setError(error, message);
        stateLock.unlock();
        stateStore_.publishDaq(published);
        return run::DaqPulseStatus::Failed;
    }

    QString deviceError;
    if (!output_->fire(&deviceError)) {
        const QString message =
            factualError(QStringLiteral("The DAQ Hit pulse failed."), deviceError);
        output_->shutdown();
        const DaqState published = updateStateLocked(DaqStatus::Faulted, message);
        setError(error, message);
        stateLock.unlock();
        stateStore_.publishDaq(published);
        return run::DaqPulseStatus::Failed;
    }

    setError(error, {});
    return run::DaqPulseStatus::Issued;
}

void DaqService::shutdown()
{
    std::lock_guard operationOrderLock(operationOrderMutex_);
    std::unique_lock stateLock(stateMutex_);
    if (state_.status == DaqStatus::Disabled
        && (!output_ || !output_->ready())) {
        return;
    }
    if (output_)
        output_->shutdown();
    const DaqState published = updateStateLocked(DaqStatus::Disabled);
    stateLock.unlock();
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
