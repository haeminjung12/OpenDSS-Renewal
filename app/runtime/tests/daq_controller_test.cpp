#include "v2/hardware/daq_controller.h"
#include "v2/hardware/daq_output.h"
#include "v2/hardware/daq_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/state/application_state_store.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantMap>

#include <cstdio>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace desktop_app::v2;

namespace {

class FakeDaqOutput final : public IDaqOutput
{
public:
    bool configure(const DaqConfig &config, QString *error) override
    {
        if (configureDelayMs > 0)
            QThread::msleep(configureDelayMs);
        ++configureCalls;
        configurations.push_back(config);
        if (!configureError.isEmpty()) {
            if (dropReadyOnConfigureFailure)
                readyValue = false;
            if (error)
                *error = configureError;
            return false;
        }
        readyValue = true;
        if (error)
            error->clear();
        return true;
    }

    void shutdown() override
    {
        ++shutdownCalls;
        readyValue = false;
        continuous = false;
    }

    bool fire(QString *error) override
    {
        ++fireCalls;
        if (!fireError.isEmpty()) {
            if (error)
                *error = fireError;
            return false;
        }
        if (error)
            error->clear();
        return true;
    }

    bool fireImmediate(QString *error) override
    {
        ++immediateFireCalls;
        if (!immediateFireError.isEmpty()) {
            if (error)
                *error = immediateFireError;
            return false;
        }
        if (error)
            error->clear();
        return true;
    }

    bool startContinuous(QString *error) override
    {
        ++startContinuousCalls;
        if (!continuousError.isEmpty()) {
            if (error)
                *error = continuousError;
            return false;
        }
        continuous = true;
        if (error)
            error->clear();
        return true;
    }

    bool stopContinuous(QString *error) override
    {
        ++stopContinuousCalls;
        continuous = false;
        if (!continuousError.isEmpty()) {
            if (error)
                *error = continuousError;
            return false;
        }
        if (error)
            error->clear();
        return true;
    }

    bool ready() const override { return readyValue; }
    bool continuousActive() const override { return continuous; }

    QString configureError;
    QString fireError;
    QString immediateFireError;
    QString continuousError;
    bool dropReadyOnConfigureFailure = false;
    bool readyValue = false;
    int configureCalls = 0;
    int fireCalls = 0;
    int immediateFireCalls = 0;
    int startContinuousCalls = 0;
    int stopContinuousCalls = 0;
    bool continuous = false;
    int shutdownCalls = 0;
    unsigned long configureDelayMs = 0;
    std::vector<DaqConfig> configurations;
};

bool check(bool condition, const char *message)
{
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

bool waitForApply(DaqController &controller, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (controller.applyInProgress() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    QCoreApplication::processEvents();
    return !controller.applyInProgress();
}

DaqDeviceInfo device(const std::string &name, const std::string &product,
                     std::vector<std::string> channels)
{
    return {name, product, std::move(channels)};
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    bool ok = true;

    {
        ApplicationStateStore store;
        OperationCoordinator operations;
        auto output = std::make_unique<FakeDaqOutput>();
        FakeDaqOutput *fake = output.get();
        DaqService service(operations, store, std::move(output));
        int discoveryCalls = 0;
        DaqController controller(
            service, store, operations,
            [&](std::string &) {
                ++discoveryCalls;
                return std::vector<DaqDeviceInfo>{};
            });

        ok &= check(discoveryCalls == 1 && controller.devices().isEmpty()
                        && controller.outputChannels().isEmpty()
                        && controller.selectedOutputChannel().isEmpty()
                        && !controller.canApply()
                        && !controller.ready()
                        && controller.daqStatus() == QStringLiteral("Unavailable")
                        && controller.error()
                               == QStringLiteral(
                                   "No DAQ analog-output channels were found."),
                    "Empty discovery must project factual unavailability.");
        ok &= check(!controller.apply()
                        && service.issueLiveHit(true)
                               == run::DaqPulseStatus::Failed
                        && fake->configureCalls == 0 && fake->fireCalls == 0,
                    "Unavailable DAQ actions must not touch output.");
    }

    {
        ApplicationStateStore store;
        OperationCoordinator operations;
        auto output = std::make_unique<FakeDaqOutput>();
        FakeDaqOutput *fake = output.get();
        DaqService service(operations, store, std::move(output));
        DaqController controller(
            service, store, operations,
            [](std::string &error) {
                error = "DAQ discovery warning with no usable channel.";
                return std::vector<DaqDeviceInfo>{
                    device("Dev1", "USB-DAQ", {}),
                };
            });

        ok &= check(!controller.refreshDevices()
                        && controller.devices().size() == 1
                        && controller.outputChannels().isEmpty()
                        && controller.selectedOutputChannel().isEmpty()
                        && controller.error()
                               == QStringLiteral(
                                   "DAQ discovery warning with no usable channel.")
                        && !controller.ready() && !controller.canApply()
                        && fake->configureCalls == 0,
                    "A discovery warning without usable channels must remain a failure.");
    }

    {
        ApplicationStateStore store;
        OperationCoordinator operations;
        auto output = std::make_unique<FakeDaqOutput>();
        FakeDaqOutput *fake = output.get();
        DaqService service(operations, store, std::move(output));
        bool reportWarning = false;
        DaqController controller(
            service, store, operations,
            [&](std::string &error) {
                if (reportWarning)
                    error = "Product type unavailable for one DAQ device.";
                return std::vector<DaqDeviceInfo>{
                    device("Dev1", "", {"Dev1/ao0"}),
                };
            });

        ok &= check(controller.apply() && waitForApply(controller)
                        && controller.ready()
                        && controller.selectedOutputChannel()
                               == QStringLiteral("Dev1/ao0"),
                    "A usable discovered channel must be selectable and applicable.");
        reportWarning = true;
        ok &= check(controller.refreshDevices() && controller.ready()
                        && controller.canApply()
                        && controller.selectedOutputChannel()
                               == QStringLiteral("Dev1/ao0")
                        && controller.error()
                               == QStringLiteral(
                                   "Product type unavailable for one DAQ device.")
                        && fake->configureCalls == 1,
                    "A partial discovery warning must retain usable ready DAQ state.");
    }

    ApplicationStateStore store;
    OperationCoordinator operations;
    auto output = std::make_unique<FakeDaqOutput>();
    FakeDaqOutput *fake = output.get();
    DaqService service(operations, store, std::move(output));
    enum class DiscoveryMode {
        Available,
        MissingConfiguredChannel,
        Error,
    };
    DiscoveryMode discoveryMode = DiscoveryMode::Available;
    int discoveryCalls = 0;
    DaqController controller(
        service, store, operations,
        [&](std::string &error) {
            ++discoveryCalls;
            if (discoveryMode == DiscoveryMode::Error) {
                error = "Injected discovery fault.";
                return std::vector<DaqDeviceInfo>{};
            }
            if (discoveryMode == DiscoveryMode::MissingConfiguredChannel) {
                return std::vector<DaqDeviceInfo>{
                    device("Dev3", "PCIe-DAQ", {"Dev3/ao2"}),
                };
            }
            return std::vector<DaqDeviceInfo>{
                device("Dev2", "USB-DAQ", {"Dev2/ao0", "Dev2/ao1"}),
                device("Dev3", "PCIe-DAQ", {"Dev3/ao2"}),
            };
        });

    const QVariantMap firstDevice = controller.devices().constFirst().toMap();
    ok &= check(discoveryCalls == 1 && controller.devices().size() == 2
                    && firstDevice.value(QStringLiteral("deviceId")).toString()
                           == QStringLiteral("Dev2")
                    && firstDevice.value(QStringLiteral("productType")).toString()
                           == QStringLiteral("USB-DAQ")
                    && firstDevice.value(QStringLiteral("outputChannels"))
                               .toStringList()
                           == QStringList{QStringLiteral("Dev2/ao0"),
                                          QStringLiteral("Dev2/ao1")}
                    && controller.selectedOutputChannel()
                           == QStringLiteral("Dev2/ao0")
                    && controller.canApply() && controller.error().isEmpty(),
                "Discovery must project exact devices and existing AO channels.");

    controller.setSelectedOutputChannel(QStringLiteral("Dev2/ao9"));
    ok &= check(!controller.canApply() && !controller.apply()
                    && fake->configureCalls == 0,
                "A guessed channel must not be selectable or applied.");

    controller.setSelectedOutputChannel(QStringLiteral("Dev2/ao1"));
    int eligibilityNotifications = 0;
    QObject::connect(&controller, &DaqController::stateChanged,
                     [&] { ++eligibilityNotifications; });
    controller.setAmplitudeVpp(std::numeric_limits<double>::quiet_NaN());
    ok &= check(!controller.canApply() && eligibilityNotifications == 1,
                "Invalid amplitude must notify canApply eligibility.");
    controller.setAmplitudeVpp(5.0);
    ok &= check(controller.canApply() && eligibilityNotifications == 2,
                "Valid amplitude must notify canApply eligibility.");
    controller.setFrequencyHz(std::numeric_limits<double>::infinity());
    ok &= check(!controller.canApply() && eligibilityNotifications == 3,
                "Invalid frequency must notify canApply eligibility.");
    controller.setFrequencyHz(10000.0);
    ok &= check(controller.canApply() && eligibilityNotifications == 4,
                "Valid frequency must notify canApply eligibility.");
    controller.setDurationMs(std::numeric_limits<double>::quiet_NaN());
    ok &= check(!controller.canApply() && eligibilityNotifications == 5,
                "Invalid duration must notify canApply eligibility.");
    controller.setDurationMs(5.0);
    ok &= check(controller.canApply() && eligibilityNotifications == 6,
                "Valid duration must notify canApply eligibility.");
    controller.setDelayMs(std::numeric_limits<double>::quiet_NaN());
    ok &= check(!controller.canApply() && eligibilityNotifications == 7,
                "Invalid delay must notify canApply eligibility.");
    controller.setDelayMs(0.0);
    ok &= check(controller.canApply() && eligibilityNotifications == 8,
                "Valid delay must notify canApply eligibility.");

    const int configureCallsBeforeInvalid = fake->configureCalls;
    const auto rejectsAmplitude = [&](double value) {
        controller.setAmplitudeVpp(value);
        return !controller.canApply() && !controller.apply()
            && fake->configureCalls == configureCallsBeforeInvalid;
    };
    const auto rejectsFrequency = [&](double value) {
        controller.setFrequencyHz(value);
        return !controller.canApply() && !controller.apply()
            && fake->configureCalls == configureCallsBeforeInvalid;
    };
    const auto rejectsDuration = [&](double value) {
        controller.setDurationMs(value);
        return !controller.canApply() && !controller.apply()
            && fake->configureCalls == configureCallsBeforeInvalid;
    };
    const auto rejectsDelay = [&](double value) {
        controller.setDelayMs(value);
        return !controller.canApply() && !controller.apply()
            && fake->configureCalls == configureCallsBeforeInvalid;
    };
    ok &= check(rejectsAmplitude(std::numeric_limits<double>::quiet_NaN())
                    && rejectsAmplitude(-0.1)
                    && rejectsAmplitude(10.1)
                    && rejectsFrequency(std::numeric_limits<double>::infinity())
                    && rejectsFrequency(999.0)
                    && rejectsFrequency(1000001.0)
                    && rejectsDuration(std::numeric_limits<double>::quiet_NaN())
                    && rejectsDuration(0.9)
                    && rejectsDuration(500.1)
                    && rejectsDelay(std::numeric_limits<double>::quiet_NaN())
                    && rejectsDelay(-0.1)
                    && rejectsDelay(500.1),
                "Controller readiness must reuse service validation for every numeric draft.");

    controller.setAmplitudeVpp(8.0);
    controller.setFrequencyHz(2000.0);
    controller.setDurationMs(4.0);
    controller.setDelayMs(3.0);
    ok &= check(controller.apply() && waitForApply(controller)
                    && fake->configureCalls == 1
                    && fake->configurations.back().channel == "Dev2/ao1"
                    && fake->configurations.back().amplitude == 4.0
                    && fake->configurations.back().frequencyHz == 2000.0
                    && fake->configurations.back().durationMs == 4.0
                    && fake->configurations.back().delayMs == 3.0
                    && controller.ready()
                    && controller.daqStatus() == QStringLiteral("Ready")
                    && store.snapshot().daq.appliedSettings.outputChannel
                           == QStringLiteral("Dev2/ao1")
                    && store.snapshot().daq.appliedSettings.amplitudeVpp == 8.0,
                 "Apply must delegate current supported configuration to the service.");

    fake->configureDelayMs = 120;
    const int configureCallsBeforeCoalescing = fake->configureCalls;
    controller.setAmplitudeVpp(6.0);
    QElapsedTimer initialApplyTimer;
    initialApplyTimer.start();
    const bool initialApplyAccepted = controller.apply();
    const qint64 initialApplyCallMs = initialApplyTimer.elapsed();

    QElapsedTimer repeatedEditTimer;
    repeatedEditTimer.start();
    for (int step = 1; step <= 20; ++step) {
        controller.setAmplitudeVpp(6.0 + step * 0.1);
        ok &= controller.apply();
    }
    const qint64 repeatedEditMs = repeatedEditTimer.elapsed();
    ok &= check(initialApplyAccepted && controller.applyInProgress()
                    && initialApplyCallMs < 30 && repeatedEditMs < 50
                    && waitForApply(controller)
                    && fake->configureCalls == configureCallsBeforeCoalescing + 2
                    && fake->configurations.at(
                           static_cast<std::size_t>(configureCallsBeforeCoalescing))
                               .amplitude == 3.0
                    && fake->configurations.back().amplitude == 4.0
                    && controller.amplitudeVpp() == 8.0
                    && store.snapshot().daq.appliedSettings.amplitudeVpp == 8.0,
                "Repeated numeric edits must stay responsive, coalesce, and apply the exact final value.");
    qInfo().noquote()
        << "daq_apply_sync_equivalent_ms=120"
        << "initial_async_call_ms=" << initialApplyCallMs
        << "repeated_20_edits_ms=" << repeatedEditMs;
    fake->configureDelayMs = 0;

    ok &= check(service.issueLiveHit(false)
                            == run::DaqPulseStatus::SuppressedNotIssued
                        && fake->fireCalls == 0,
                    "Disabled operation output must remain suppressed.");

    ok &= check(controller.sendTestSineWave()
                    && fake->immediateFireCalls == 1
                    && fake->fireCalls == 0,
                "The finite test action must use the immediate path, not the delayed event path.");
    ok &= check(controller.toggleContinuousWaveform()
                    && controller.continuousWaveformActive()
                    && fake->startContinuousCalls == 1
                    && !controller.canApply(),
                "Hardware Start Sine Wave must start and expose continuous output state.");
    QString suppressedError;
    ok &= check(service.issueLiveHit(true, &suppressedError)
                        == run::DaqPulseStatus::SuppressedNotIssued
                    && fake->fireCalls == 0
                    && suppressedError.contains(QStringLiteral("continuous")),
                "Continuous output must suppress event-driven finite output.");
    ok &= check(!controller.sendTestSineWave()
                    && fake->immediateFireCalls == 1,
                "A finite test waveform must not overlap continuous output.");
    ok &= check(controller.toggleContinuousWaveform()
                    && !controller.continuousWaveformActive()
                    && fake->stopContinuousCalls == 1
                    && controller.canApply(),
                "Hardware Stop Sine Wave must stop continuous output and restore finite readiness.");

    auto held =
        operations.acquire(OperationKind::LiveSorting, ResourceLock::Daq);
    const int configureCallsBeforeLock = fake->configureCalls;
    ok &= check(held.acquired() && controller.canApply()
                    && controller.apply() && waitForApply(controller)
                    && fake->configureCalls == configureCallsBeforeLock + 1,
                "Active sorting must permit serialized DAQ settings apply.");
    held.lease.release();
    ok &= check(controller.canApply(),
                "DAQ settings must become available after lock release.");

    ok &= check(service.issueLiveHit(true) == run::DaqPulseStatus::Issued
                    && fake->fireCalls == 1,
                "Ordinary output must use the configured output path.");
    fake->fireError = QStringLiteral("Injected output fault.");
    ok &= check(service.issueLiveHit(true) == run::DaqPulseStatus::Failed
                    && fake->fireCalls == 2
                    && !controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && controller.error()
                           == QStringLiteral("Injected output fault."),
                "Output failure must publish the factual DAQ fault.");
    ok &= check(service.issueLiveHit(true) == run::DaqPulseStatus::Failed
                    && fake->fireCalls == 2,
                "An unready DAQ must not issue another output.");

    fake->fireError.clear();
    ok &= check(controller.apply() && waitForApply(controller)
                    && controller.ready()
                    && fake->configureCalls == configureCallsBeforeCoalescing + 4,
                "A successful apply must recover a faulted DAQ.");

    fake->configureError = QStringLiteral("Injected apply fault.");
    controller.setAmplitudeVpp(7.0);
    ok &= check(controller.apply() && waitForApply(controller)
                    && controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Ready
                    && controller.amplitudeVpp() == 8.0
                    && controller.error()
                           == QStringLiteral("Injected apply fault."),
                "Apply failure must retain prior settings only while output remains ready.");

    fake->dropReadyOnConfigureFailure = true;
    controller.setAmplitudeVpp(7.0);
    ok &= check(controller.apply() && waitForApply(controller)
                    && !controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && fake->shutdownCalls >= 2,
                "Apply failure after readiness loss must shut down and fault DAQ.");

    fake->configureError.clear();
    fake->dropReadyOnConfigureFailure = false;
    ok &= check(controller.apply() && waitForApply(controller)
                    && controller.ready(),
                "Only a successful explicit apply must recover DAQ.");

    const int fireCallsBeforeMissingChannel = fake->fireCalls;
    const int shutdownsBeforeMissingChannel = fake->shutdownCalls;
    auto heldDuringMissingChannel =
        operations.acquire(OperationKind::LiveSorting, ResourceLock::Daq);
    discoveryMode = DiscoveryMode::MissingConfiguredChannel;
    ok &= check(heldDuringMissingChannel.acquired()
                    && controller.refreshDevices() && !controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && controller.selectedOutputChannel()
                           == QStringLiteral("Dev3/ao2")
                    && service.issueLiveHit(true)
                           == run::DaqPulseStatus::Failed
                    && fake->fireCalls == fireCallsBeforeMissingChannel
                    && fake->shutdownCalls == shutdownsBeforeMissingChannel,
                "Lost AO membership must suppress old output while Live holds teardown.");
    heldDuringMissingChannel.lease.release();

    discoveryMode = DiscoveryMode::Available;
    ok &= check(controller.refreshDevices() && controller.apply()
                    && waitForApply(controller)
                    && controller.ready(),
                "Explicit apply must recover after the configured channel returns.");

    const int fireCallsBeforeDiscoveryError = fake->fireCalls;
    const int shutdownsBeforeDiscoveryError = fake->shutdownCalls;
    auto heldDuringDiscoveryError =
        operations.acquire(OperationKind::SequenceTest, ResourceLock::Daq);
    discoveryMode = DiscoveryMode::Error;
    ok &= check(heldDuringDiscoveryError.acquired()
                    && !controller.refreshDevices() && !controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && controller.error()
                           == QStringLiteral("Injected discovery fault.")
                    && service.issueLiveHit(true)
                           == run::DaqPulseStatus::Failed
                    && fake->fireCalls == fireCallsBeforeDiscoveryError
                    && fake->shutdownCalls == shutdownsBeforeDiscoveryError,
                "Discovery failure must suppress old output while Sequence Test holds teardown.");
    heldDuringDiscoveryError.lease.release();

    discoveryMode = DiscoveryMode::Available;
    ok &= check(controller.refreshDevices() && controller.apply()
                    && waitForApply(controller)
                    && controller.ready(),
                "Explicit apply must recover after discovery recovers.");
    service.shutdown();
    const int shutdownCalls = fake->shutdownCalls;
    service.shutdown();
    ok &= check(!controller.ready()
                    && store.snapshot().daq.status == DaqStatus::Disabled
                    && fake->shutdownCalls == shutdownCalls
                    && service.issueLiveHit(false)
                           == run::DaqPulseStatus::SuppressedNotIssued
                    && fake->fireCalls == 2,
                "Shutdown must be idempotent and disabled output must stay suppressed.");

    return ok ? 0 : 1;
}
