#include "daq_output.h"

#include "../../daq_trigger.h"

#include <string>

namespace desktop_app::v2 {
namespace {

void setError(QString *error, const std::string &message)
{
    if (error)
        *error = QString::fromStdString(message).trimmed();
}

} // namespace

DaqTriggerOutput::DaqTriggerOutput()
    : trigger_(std::make_unique<DaqTrigger>())
{
}

DaqTriggerOutput::~DaqTriggerOutput() = default;

bool DaqTriggerOutput::configure(const DaqConfig &config, QString *error)
{
    auto candidate = std::make_unique<DaqTrigger>();
    std::string deviceError;
    if (!candidate->init(config, deviceError)) {
        setError(error, deviceError);
        return false;
    }

    trigger_.swap(candidate);
    if (error)
        error->clear();
    return true;
}

void DaqTriggerOutput::shutdown()
{
    if (trigger_)
        trigger_->shutdown();
}

bool DaqTriggerOutput::fire(QString *error)
{
    if (!ready()) {
        if (error)
            *error = QStringLiteral("DAQ trigger not initialized");
        return false;
    }

    std::string deviceError;
    const bool fired = trigger_->fire(deviceError);
    setError(error, deviceError);
    return fired;
}

bool DaqTriggerOutput::fireImmediate(QString *error)
{
    if (!ready()) {
        if (error)
            *error = QStringLiteral("DAQ trigger not initialized");
        return false;
    }
    std::string deviceError;
    const bool fired = trigger_->fireImmediate(deviceError);
    setError(error, deviceError);
    return fired;
}

bool DaqTriggerOutput::startContinuous(QString *error)
{
    if (!ready()) {
        if (error)
            *error = QStringLiteral("DAQ trigger not initialized");
        return false;
    }
    std::string deviceError;
    const bool started = trigger_->startContinuous(deviceError);
    setError(error, deviceError);
    return started;
}

bool DaqTriggerOutput::stopContinuous(QString *error)
{
    if (!trigger_) {
        if (error)
            *error = QStringLiteral("DAQ trigger not initialized");
        return false;
    }
    std::string deviceError;
    const bool stopped = trigger_->stopContinuous(deviceError);
    setError(error, deviceError);
    return stopped;
}

bool DaqTriggerOutput::ready() const
{
    return trigger_ && trigger_->isReady();
}

bool DaqTriggerOutput::continuousActive() const
{
    return trigger_ && trigger_->isContinuous();
}

} // namespace desktop_app::v2
