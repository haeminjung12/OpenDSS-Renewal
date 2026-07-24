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
    int clearCalls = 0;
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
                    && defaults.frequencyHz == 1000.0
                    && defaults.durationMs == 5.0
                    && defaults.delayMs == 0.0,
                "DAQ settings must use the approved defaults.");
    ok &= check(daq.applySettings(defaults, &error), qPrintable(error));
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
                        == 1000.0
                    && snapshot.value(QStringLiteral("duration_ms")).toDouble()
                        == 5.0
                    && snapshot.value(QStringLiteral("delay_ms")).toDouble()
                        == 0.0
                    && snapshot.value(QStringLiteral("amplitude_vpp")).toDouble()
                        == 5.0,
                "DAQ snapshot must record the approved settings and fixed Vpp.");
    ok &= check(fake.channel == "Dev1/ao0" && fake.timingRate == 50000.0
                    && fake.timingSamples == 251,
                "Defaults must pass channel, frequency, and duration unchanged.");

    ok &= check(daq.issueLiveHit(false, &error)
                        == run::DaqPulseStatus::SuppressedNotIssued
                    && fake.writeCalls == 0,
                "DAQ Output OFF must suppress the Hit without an NI write.");
    ok &= check(daq.issueLiveHit(true, &error)
                        == run::DaqPulseStatus::Issued
                    && fake.writeCalls == 1 && fake.waveform.size() == 251
                    && fake.waveform.back() == 0.0,
                "DAQ Output ON must issue the qualified waveform with a final zero.");
    const auto extrema =
        std::minmax_element(fake.waveform.begin(), fake.waveform.end());
    ok &= check(extrema.first != fake.waveform.end()
                    && *extrema.first >= -2.5 && *extrema.second <= 2.5
                    && *extrema.first < -2.49
                    && *extrema.second > 2.49,
                "Fixed 5 Vpp must map to a zero-centered 2.5 V peak waveform.");

    DaqAppliedSettings invalid = defaults;
    invalid.frequencyHz = std::numeric_limits<double>::quiet_NaN();
    ok &= check(!daq.applySettings(invalid, &error) && daq.ready()
                    && sameSettings(store.snapshot().daq.appliedSettings, defaults),
                "Invalid settings must preserve the prior applied state.");

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
    ok &= check(held.acquired() && !daq.applySettings(candidate, &error)
                    && fake.createCalls == createsBeforeConflict
                    && daq.ready(),
                "A DAQ lock conflict must reject apply before touching NI.");
    held.lease.release();

    DaqAppliedSettings custom = defaults;
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

    QObject::disconnect(observerConnection);
    return ok ? 0 : 1;
}
