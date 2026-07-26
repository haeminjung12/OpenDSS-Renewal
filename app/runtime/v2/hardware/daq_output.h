#pragma once

#include <QString>

#include <memory>

struct DaqConfig;
class DaqTrigger;

namespace desktop_app::v2 {

class IDaqOutput
{
public:
    virtual ~IDaqOutput() = default;

    virtual bool configure(const DaqConfig &config, QString *error) = 0;
    virtual void shutdown() = 0;
    virtual bool fire(QString *error) = 0;
    virtual bool fireImmediate(QString *error) = 0;
    virtual bool startContinuous(QString *error) = 0;
    virtual bool stopContinuous(QString *error) = 0;
    virtual bool ready() const = 0;
    virtual bool continuousActive() const = 0;
};

class DaqTriggerOutput final : public IDaqOutput
{
public:
    DaqTriggerOutput();
    ~DaqTriggerOutput() override;

    bool configure(const DaqConfig &config, QString *error) override;
    void shutdown() override;
    bool fire(QString *error) override;
    bool fireImmediate(QString *error) override;
    bool startContinuous(QString *error) override;
    bool stopContinuous(QString *error) override;
    bool ready() const override;
    bool continuousActive() const override;

private:
    std::unique_ptr<DaqTrigger> trigger_;
};

} // namespace desktop_app::v2
