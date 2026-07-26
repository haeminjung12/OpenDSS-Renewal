#include "v2/hardware/daq_service.h"
#include "v2/operation/operation_coordinator.h"
#include "v2/state/application_state_store.h"

#include <NIDAQmx.h>

#include <QCoreApplication>
#include <QDebug>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace desktop_app::v2;

namespace {

struct FakeDaq {
    int createStatus = 0;
    int writeStatus = 0;
    int waitStatus = 0;
    int createCalls = 0;
    int writeCalls = 0;
    int startCalls = 0;
    int stopCalls = 0;
    int scalarWriteCalls = 0;
    int clearCalls = 0;
    double lastScalarValue = std::numeric_limits<double>::quiet_NaN();
    std::string channel;
    double timingRate = 0.0;
    unsigned int timingSamples = 0;
    double waitTimeout = 0.0;
    std::vector<double> waveform;

    void reset()
    {
        *this = FakeDaq{};
    }
};

FakeDaq fake;

bool check(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
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

extern "C" int32 DAQmxGetExtendedErrorInfo(char buffer[], uInt32 bufferSize)
{
    const char *message = "injected NI failure";
    if (bufferSize > 0) {
        std::strncpy(buffer, message, bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    }
    return 0;
}

extern "C" int32 DAQmxGetSysDevNames(char buffer[], uInt32 bufferSize)
{
    return bufferSize ? (buffer[0] = '\0', 0) : 0;
}

extern "C" int32 DAQmxGetDevProductType(const char[], char buffer[],
                                         uInt32 bufferSize)
{
    return bufferSize ? (buffer[0] = '\0', 0) : 0;
}

extern "C" int32 DAQmxGetDevAOPhysicalChans(const char[], char buffer[],
                                             uInt32 bufferSize)
{
    return bufferSize ? (buffer[0] = '\0', 0) : 0;
}

extern "C" int32 DAQmxGetDevAOMaxRate(const char[], float64 *value)
{
    *value = 0.0;
    return 0;
}

extern "C" int32 DAQmxCreateTask(const char[], TaskHandle *task)
{
    ++fake.createCalls;
    if (fake.createStatus < 0)
        return fake.createStatus;
    *task = reinterpret_cast<TaskHandle>(
        static_cast<std::uintptr_t>(fake.createCalls));
    return 0;
}

extern "C" int32 DAQmxCreateAOVoltageChan(
    TaskHandle, const char physicalChannel[], const char[], float64, float64,
    int32, const char[])
{
    fake.channel = physicalChannel;
    return 0;
}

extern "C" int32 DAQmxCfgSampClkTiming(TaskHandle, const char[], float64 rate,
                                        int32, int32, uInt32 samples)
{
    fake.timingRate = rate;
    fake.timingSamples = samples;
    return 0;
}

extern "C" int32 DAQmxCfgOutputBuffer(TaskHandle, uInt32)
{
    return 0;
}

extern "C" int32 DAQmxStopTask(TaskHandle)
{
    ++fake.stopCalls;
    return 0;
}

extern "C" int32 DAQmxStartTask(TaskHandle)
{
    ++fake.startCalls;
    return 0;
}

extern "C" int32 DAQmxClearTask(TaskHandle)
{
    ++fake.clearCalls;
    return 0;
}

extern "C" int32 DAQmxWriteAnalogF64(
    TaskHandle, int32 samples, bool32, float64, bool32,
    const float64 values[], int32 *written, bool32*)
{
    ++fake.writeCalls;
    if (fake.writeStatus < 0)
        return fake.writeStatus;
    fake.waveform.assign(values, values + samples);
    *written = samples;
    return 0;
}

extern "C" int32 DAQmxWriteAnalogScalarF64(
    TaskHandle, bool32, float64, float64 value, bool32*)
{
    ++fake.scalarWriteCalls;
    fake.lastScalarValue = value;
    return 0;
}

extern "C" int32 DAQmxWaitUntilTaskDone(TaskHandle, float64 timeout)
{
    fake.waitTimeout = timeout;
    return fake.waitStatus;
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    bool ok = true;
    QString error;

    OperationCoordinator operations;
    ApplicationStateStore store;
    DaqService daq(operations, store);
    int observerCalls = 0;
    bool observerReady = false;
    QJsonObject observerSettings;
    const QMetaObject::Connection observerConnection =
        QObject::connect(&store, &ApplicationStateStore::changed, [&] {
            ++observerCalls;
            observerReady = daq.ready();
            observerSettings = daq.settingsSnapshot();
        });
    ok &= check(store.snapshot().daq.status == DaqStatus::Disabled,
                "DAQ must initially publish Disabled.");

    const DaqAppliedSettings defaults;
    ok &= check(defaults.outputChannel == QStringLiteral("Dev1/ao0")
                    && defaults.amplitudeVpp == 5.0
                    && defaults.frequencyHz == 10000.0
                    && defaults.durationMs == 5.0
                    && defaults.delayMs == 0.0,
                "DAQ settings must use the approved defaults.");
    bool applyInProgress = false;
    bool resourceObserverRanDuringApply = false;
    bool resourceSnapshotSucceeded = false;
    bool resourceSnapshotThrew = false;
    const QMetaObject::Connection resourceConnection =
        QObject::connect(&operations, &OperationCoordinator::resourcesChanged,
                         [&] {
            resourceObserverRanDuringApply |= applyInProgress;
            try {
                daq.settingsSnapshot();
                resourceSnapshotSucceeded = true;
            } catch (...) {
                resourceSnapshotThrew = true;
            }
        });
    bool applyThrew = false;
    bool applied = false;
    applyInProgress = true;
    try {
        applied = daq.applySettings(defaults, &error);
    } catch (...) {
        applyThrew = true;
    }
    applyInProgress = false;
    QObject::disconnect(resourceConnection);
    ok &= check(applied && !applyThrew && resourceObserverRanDuringApply
                    && resourceSnapshotSucceeded && !resourceSnapshotThrew,
                "Resource observers must query DAQ settings during apply without deadlock or exception.");
    const QJsonObject snapshot = daq.settingsSnapshot();
    ok &= check(daq.ready()
                    && sameSettings(store.snapshot().daq.appliedSettings, defaults)
                    && store.snapshot().daq.status == DaqStatus::Ready
                    && store.snapshot().daq.deviceId == QStringLiteral("Dev1"),
                "Successful apply must publish Ready with the applied settings.");
    ok &= check(observerCalls == 1 && observerReady
                    && observerSettings.value(QStringLiteral("channel")).toString()
                        == QStringLiteral("Dev1/ao0"),
                "Changed observers must query DAQ readiness and settings without deadlock.");
    ok &= check(snapshot.value(QStringLiteral("channel")).toString()
                        == QStringLiteral("Dev1/ao0")
                    && snapshot.value(QStringLiteral("frequency_hz")).toDouble()
                        == 10000.0
                    && snapshot.value(QStringLiteral("duration_ms")).toDouble()
                        == 5.0
                    && snapshot.value(QStringLiteral("delay_ms")).toDouble()
                        == 0.0
                    && snapshot.value(QStringLiteral("amplitude_vpp")).toDouble()
                        == 5.0,
                "DAQ snapshot must record the approved unit-explicit settings.");
    ok &= check(fake.channel == "Dev1/ao0" && fake.timingRate == 500000.0
                    && fake.timingSamples == 2501,
                "Defaults must pass channel, frequency, and duration unchanged.");

    ok &= check(daq.issueLiveHit(false, &error)
                        == run::DaqPulseStatus::SuppressedNotIssued
                    && fake.writeCalls == 0,
                "DAQ Output OFF must suppress the Hit without an NI write.");
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::Issued
                    && fake.writeCalls == 1 && fake.waveform.size() == 2501
                    && fake.waveform.back() == 0.0,
                "DAQ Output ON must issue the qualified waveform with a final zero.");
    const auto extrema =
        std::minmax_element(fake.waveform.begin(), fake.waveform.end());
    ok &= check(extrema.first != fake.waveform.end()
                    && *extrema.first >= -2.5 && *extrema.second <= 2.5
                    && *extrema.first < -2.49
                    && *extrema.second > 2.49,
                "Fixed 5 Vpp must map to a zero-centered 2.5 V peak waveform.");

    std::vector<DaqAppliedSettings> invalidSettings;
    for (double amplitude : {-0.1, 10.1,
                             std::numeric_limits<double>::quiet_NaN()}) {
        DaqAppliedSettings invalid = defaults;
        invalid.amplitudeVpp = amplitude;
        invalidSettings.push_back(invalid);
    }
    for (double frequency : {999.0, 1000001.0}) {
        DaqAppliedSettings invalid = defaults;
        invalid.frequencyHz = frequency;
        invalidSettings.push_back(invalid);
    }
    for (double duration : {0.9, 500.1}) {
        DaqAppliedSettings invalid = defaults;
        invalid.durationMs = duration;
        invalidSettings.push_back(invalid);
    }
    for (double delay : {-0.1, 500.1}) {
        DaqAppliedSettings invalid = defaults;
        invalid.delayMs = delay;
        invalidSettings.push_back(invalid);
    }
    const int createsBeforeInvalid = fake.createCalls;
    for (const DaqAppliedSettings &invalid : invalidSettings) {
        ok &= check(!daq.applySettings(invalid, &error) && daq.ready()
                        && sameSettings(store.snapshot().daq.appliedSettings,
                                        defaults),
                    "Out-of-range settings must preserve prior applied state.");
    }
    ok &= check(fake.createCalls == createsBeforeInvalid,
                "Range validation must reject before touching output.");

    fake.createStatus = -1;
    DaqAppliedSettings candidate = defaults;
    candidate.outputChannel = QStringLiteral("Dev2/ao3");
    ok &= check(!daq.applySettings(candidate, &error) && daq.ready()
                    && sameSettings(store.snapshot().daq.appliedSettings, defaults)
                    && store.snapshot().daq.status == DaqStatus::Ready
                    && store.snapshot().daq.fault.contains(
                        QStringLiteral("injected NI failure")),
                "Candidate failure must preserve the prior ready trigger and settings.");
    fake.createStatus = 0;

    auto held = operations.acquire(OperationKind::LiveSorting,
                                   ResourceLock::Daq);
    const int createsBeforeConflict = fake.createCalls;
    const int clearsBeforeConflict = fake.clearCalls;
    ok &= check(held.acquired() && !daq.applySettings(candidate, &error)
                    && fake.createCalls == createsBeforeConflict
                    && daq.ready(),
                "A DAQ lock conflict must reject apply before touching NI.");
    const int writesBeforeUnavailable = fake.writeCalls;
    daq.markUnavailable(QStringLiteral("Injected discovery fault."));
    ok &= check(!daq.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && store.snapshot().daq.fault
                           == QStringLiteral("Injected discovery fault.")
                    && fake.clearCalls == clearsBeforeConflict
                    && daq.issueLiveHit(true, &error)
                           == run::DaqPulseStatus::Failed
                    && fake.writeCalls == writesBeforeUnavailable
                    && fake.clearCalls == clearsBeforeConflict,
                "Factual loss must suppress output immediately while teardown is lease-blocked.");
    held.lease.release();

    DaqAppliedSettings custom = defaults;
    custom.amplitudeVpp = 8.0;
    custom.frequencyHz = 2000.0;
    custom.durationMs = 4.0;
    custom.delayMs = 3.0;
    ok &= check(daq.applySettings(custom, &error)
                    && fake.timingRate == 100000.0
                    && fake.timingSamples == 401,
                "Custom frequency and duration must pass unchanged.");
    fake.waveform.clear();
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::Issued
                    && std::abs(fake.waitTimeout - 5.007) < 1e-12,
                "Custom duration and delay must pass unchanged to firing.");
    const auto customExtrema =
        std::minmax_element(fake.waveform.begin(), fake.waveform.end());
    ok &= check(customExtrema.first != fake.waveform.end()
                    && *customExtrema.first >= -4.0
                    && *customExtrema.second <= 4.0
                    && *customExtrema.first < -3.98
                    && *customExtrema.second > 3.98,
                "8 Vpp must map to a zero-centered 4 V peak waveform.");

    const int writesBeforeTest = fake.writeCalls;
    auto liveDaq = operations.acquire(OperationKind::LiveSorting,
                                      ResourceLock::Daq);
    ok &= check(liveDaq.acquired() && !daq.sendTestSine(&error)
                    && !daq.startContinuous(&error)
                    && fake.writeCalls == writesBeforeTest,
                "Finite test and continuous Start must not bypass an active DAQ owner.");
    liveDaq.lease.release();
    ok &= check(daq.sendTestSine(&error)
                    && fake.writeCalls == writesBeforeTest + 1
                    && std::abs(fake.waitTimeout - 5.004) < 1e-12,
                "Live test sine must issue immediately without decision delay.");
    const int writesBeforeContinuous = fake.writeCalls;
    ok &= check(daq.startContinuous(&error) && daq.continuousActive()
                    && daq.startContinuous(&error)
                    && fake.startCalls == 1
                    && fake.writeCalls == writesBeforeContinuous + 1,
                "Hardware Start must begin one idempotent continuous waveform.");
    auto blockedLive = operations.acquire(OperationKind::LiveSorting,
                                          ResourceLock::Daq);
    ok &= check(!blockedLive.acquired(),
                "Continuous output must retain DAQ ownership until Hardware Stop.");
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::SuppressedNotIssued
                    && !daq.sendTestSine(&error)
                    && fake.writeCalls == writesBeforeContinuous + 1,
                "Continuous output must suppress finite event and test output.");
    bool releaseCallbackObservedStoppedOutput = false;
    const QMetaObject::Connection continuousReleaseConnection =
        QObject::connect(&operations, &OperationCoordinator::resourcesChanged, [&] {
            releaseCallbackObservedStoppedOutput = !daq.continuousActive();
        });
    const int stopsBeforeContinuousStop = fake.stopCalls;
    ok &= check(daq.stopContinuous(&error) && !daq.continuousActive()
                    && daq.ready() && fake.scalarWriteCalls == 1
                    && fake.stopCalls > stopsBeforeContinuousStop
                    && fake.lastScalarValue == 0.0
                    && releaseCallbackObservedStoppedOutput,
                "Hardware Stop must return output to zero, restore finite readiness, "
                "and release ownership without re-entering the DAQ state lock.");
    QObject::disconnect(continuousReleaseConnection);
    auto liveAfterStop = operations.acquire(OperationKind::LiveSorting,
                                            ResourceLock::Daq);
    ok &= check(liveAfterStop.acquired(),
                "Hardware Stop must release DAQ ownership.");
    liveAfterStop.lease.release();

    ok &= check(daq.startContinuous(&error) && daq.continuousActive(),
                "Continuous output must restart after a safe Stop.");
    daq.markUnavailable(QStringLiteral("Injected continuous discovery fault."));
    auto liveAfterFault = operations.acquire(OperationKind::LiveSorting,
                                             ResourceLock::Daq);
    ok &= check(!daq.continuousActive() && !daq.ready()
                    && liveAfterFault.acquired(),
                "A continuous-output fault must stop output and release DAQ ownership.");
    liveAfterFault.lease.release();
    ok &= check(daq.applySettings(custom, &error) && daq.ready(),
                "DAQ must recover after a continuous-output discovery fault.");

    const int zerosBeforeContinuousShutdown = fake.scalarWriteCalls;
    const int stopsBeforeContinuousShutdown = fake.stopCalls;
    const int clearsBeforeContinuousShutdown = fake.clearCalls;
    bool releaseCallbackObservedShutdown = false;
    const QMetaObject::Connection continuousShutdownConnection =
        QObject::connect(&operations, &OperationCoordinator::resourcesChanged, [&] {
            releaseCallbackObservedShutdown = !daq.continuousActive();
        });
    ok &= check(daq.startContinuous(&error) && daq.continuousActive(),
                "Continuous output must start before the exit teardown check.");
    daq.shutdown();
    ok &= check(!daq.continuousActive() && !daq.ready()
                    && fake.scalarWriteCalls == zerosBeforeContinuousShutdown + 1
                    && fake.stopCalls > stopsBeforeContinuousShutdown
                    && fake.lastScalarValue == 0.0
                    && fake.clearCalls > clearsBeforeContinuousShutdown
                    && releaseCallbackObservedShutdown,
                "Exit teardown must stop and clear the continuous task, return output "
                "to zero, and release ownership without re-entering the DAQ state lock.");
    QObject::disconnect(continuousShutdownConnection);
    ok &= check(daq.applySettings(custom, &error) && daq.ready(),
                "DAQ must recover after the exit teardown check.");

    fake.writeStatus = -1;
    const int writesBeforeFailure = fake.writeCalls;
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::Failed
                    && !daq.ready()
                    && store.snapshot().daq.status == DaqStatus::Faulted
                    && fake.clearCalls > 0,
                "A fire failure must fault and shut down the task.");
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::Failed
                    && fake.writeCalls == writesBeforeFailure + 1,
                "A faulted DAQ must not retry until settings are reapplied.");
    fake.writeStatus = 0;
    ok &= check(daq.applySettings(defaults, &error) && daq.ready(),
                "A successful reapply must recover readiness.");

    std::mutex publicationMutex;
    std::condition_variable publicationChanged;
    bool readyPublicationEntered = false;
    bool releaseReadyPublication = false;
    const QMetaObject::Connection orderingConnection =
        QObject::connect(&store, &ApplicationStateStore::changed, [&] {
            if (store.snapshot().daq.status != DaqStatus::Ready)
                return;
            std::unique_lock publicationLock(publicationMutex);
            if (readyPublicationEntered)
                return;
            readyPublicationEntered = true;
            publicationChanged.notify_all();
            publicationChanged.wait(publicationLock,
                                    [&] { return releaseReadyPublication; });
        });

    std::atomic_bool applyReturned{false};
    std::atomic_bool shutdownStarted{false};
    std::atomic_bool shutdownReturned{false};
    std::thread concurrentApply([&] {
        QString applyError;
        daq.applySettings(defaults, &applyError);
        applyReturned.store(true);
    });
    {
        std::unique_lock publicationLock(publicationMutex);
        ok &= check(publicationChanged.wait_for(
                        publicationLock, std::chrono::seconds(2),
                        [&] { return readyPublicationEntered; }),
                    "Ready publication must enter the deterministic ordering gate.");
    }
    std::thread concurrentShutdown([&] {
        shutdownStarted.store(true);
        daq.shutdown();
        shutdownReturned.store(true);
    });
    const auto shutdownDeadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!shutdownStarted.load()
           && std::chrono::steady_clock::now() < shutdownDeadline) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ok &= check(shutdownStarted.load() && !shutdownReturned.load()
                    && !applyReturned.load()
                    && store.snapshot().daq.status == DaqStatus::Ready,
                "Shutdown must wait until the earlier Ready publication completes.");
    {
        std::lock_guard publicationLock(publicationMutex);
        releaseReadyPublication = true;
    }
    publicationChanged.notify_all();
    concurrentApply.join();
    concurrentShutdown.join();
    QObject::disconnect(orderingConnection);
    ok &= check(!daq.ready()
                    && store.snapshot().daq.status == DaqStatus::Disabled
                    && applyReturned.load() && shutdownReturned.load(),
                "Shutdown must publish Disabled after the earlier apply publication.");
    const int clearsAfterShutdown = fake.clearCalls;
    const int publicationsAfterShutdown = observerCalls;
    daq.shutdown();
    ok &= check(fake.clearCalls == clearsAfterShutdown
                    && observerCalls == publicationsAfterShutdown
                    && store.snapshot().daq.status == DaqStatus::Disabled,
                "Repeated shutdown must be idempotent.");

    QObject::disconnect(observerConnection);
    return ok ? 0 : 1;
}
