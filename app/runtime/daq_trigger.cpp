#include "daq_trigger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <new>
#include <sstream>
#include <string>
#include <thread>

#ifdef HAVE_NIDAQMX
#include <NIDAQmx.h>

static std::string formatDaqError(const char* label, int32 err) {
    char buf[2048] = {};
    DAQmxGetExtendedErrorInfo(buf, static_cast<uInt32>(sizeof(buf)));
    std::string msg = label;
    msg += " (error ";
    msg += std::to_string(static_cast<int>(err));
    msg += ")";
    if (buf[0] != '\0') {
        msg += ": ";
        msg += buf;
    }
    return msg;
}
#endif

namespace {
constexpr double kMinSampleRate = 10000.0;
constexpr double kMaxSampleRate = 900000.0;
constexpr double kSamplesPerCycle = 50.0;
constexpr double kAbsoluteMinSampleRate = 2.0;
constexpr int kMaxRateClampAttempts = 24;

std::string trimCopy(const std::string& value) {
    const auto first =
        std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last =
        std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last)
        return {};
    return std::string(first, last);
}

std::vector<std::string> splitCsvList(const std::string& value) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = trimCopy(item);
        if (!item.empty())
            parts.push_back(item);
    }
    return parts;
}

#ifdef HAVE_NIDAQMX
std::string queryDaqString(int32 (*queryFn)(const char[], char[], uInt32), const std::string& deviceName,
                           std::string& err, const char* label) {
    char buffer[8192] = {};
    const int32 status = queryFn(deviceName.c_str(), buffer, static_cast<uInt32>(sizeof(buffer)));
    if (DAQmxFailed(status)) {
        err = formatDaqError(label, status);
        return {};
    }
    return std::string(buffer);
}

double queryDaqFloat64(int32 (*queryFn)(const char[], float64*), const std::string& deviceName, std::string& err,
                       const char* label) {
    float64 value = 0.0;
    const int32 status = queryFn(deviceName.c_str(), &value);
    if (DAQmxFailed(status)) {
        err = formatDaqError(label, status);
        return 0.0;
    }
    return static_cast<double>(value);
}
#endif

std::string deviceNameFromPhysicalChannel(const std::string& channel) {
    const std::string trimmed = trimCopy(channel);
    const std::size_t slash = trimmed.find('/');
    return slash == std::string::npos ? trimmed : trimmed.substr(0, slash);
}
} // namespace

std::string DaqDeviceInfo::preferredChannel() const {
    return aoChannels.empty() ? std::string() : aoChannels.front();
}

std::vector<DaqDeviceInfo> discoverDaqDevices(std::string& err) {
    err.clear();
#ifdef HAVE_NIDAQMX
    char deviceNamesBuffer[8192] = {};
    const int32 status = DAQmxGetSysDevNames(deviceNamesBuffer, static_cast<uInt32>(sizeof(deviceNamesBuffer)));
    if (DAQmxFailed(status)) {
        err = formatDaqError("DAQmxGetSysDevNames failed", status);
        return {};
    }

    std::vector<DaqDeviceInfo> devices;
    for (const std::string& deviceName : splitCsvList(deviceNamesBuffer)) {
        DaqDeviceInfo info;
        info.name = deviceName;

        std::string productErr;
        info.productType =
            trimCopy(queryDaqString(DAQmxGetDevProductType, deviceName, productErr, "DAQmxGetDevProductType failed"));
        if (!productErr.empty() && err.empty())
            err = productErr;

        std::string channelsErr;
        const std::string aoChannelText =
            queryDaqString(DAQmxGetDevAOPhysicalChans, deviceName, channelsErr, "DAQmxGetDevAOPhysicalChans failed");
        info.aoChannels = splitCsvList(aoChannelText);
        if (!channelsErr.empty() && err.empty())
            err = channelsErr;

        devices.push_back(std::move(info));
    }

    std::sort(devices.begin(), devices.end(),
              [](const DaqDeviceInfo& left, const DaqDeviceInfo& right) { return left.name < right.name; });
    return devices;
#else
    err = "NI-DAQmx not available at build time";
    return {};
#endif
}

DaqTrigger::DaqTrigger()
    : ready_(false)
    , continuous_(false)
#ifdef HAVE_NIDAQMX
      ,
      task_(nullptr)
#endif
{
}

DaqTrigger::~DaqTrigger() {
    shutdown();
}

bool DaqTrigger::init(const DaqConfig& cfg, std::string& err) {
    err.clear();
    try {
        cfg_ = cfg;
        ready_ = false;
        shutdown();

        if (cfg_.channel.empty()) {
            err = "DAQ channel is empty";
            return false;
        }
        if (cfg_.rangeMin >= cfg_.rangeMax) {
            err = "DAQ output range is invalid";
            return false;
        }
        double durationSec = cfg_.durationMs / 1000.0;
        if (durationSec <= 0.0) {
            err = "DAQ duration must be > 0";
            return false;
        }
        if (cfg_.frequencyHz <= 0.0) {
            err = "DAQ frequency must be > 0";
            return false;
        }
        double maxAbs = std::min(std::abs(cfg_.rangeMin), std::abs(cfg_.rangeMax));
        if (maxAbs <= 0.0) {
            err = "DAQ output range invalid";
            return false;
        }

#ifdef HAVE_NIDAQMX
        TaskHandle task = nullptr;
        int32 error = DAQmxCreateTask("", &task);
        if (DAQmxFailed(error)) {
            err = formatDaqError("DAQmxCreateTask failed", error);
            return false;
        }
        error = DAQmxCreateAOVoltageChan(task, cfg_.channel.c_str(), "", cfg_.rangeMin, cfg_.rangeMax, DAQmx_Val_Volts,
                                         nullptr);
        if (DAQmxFailed(error)) {
            err = formatDaqError("DAQmxCreateAOVoltageChan failed", error);
            DAQmxClearTask(task);
            return false;
        }

        const double amplitude = std::clamp(std::abs(cfg_.amplitude), 0.0, maxAbs);
        double sampleRate = std::max(kMinSampleRate, cfg_.frequencyHz * kSamplesPerCycle);
        sampleRate = std::min(sampleRate, kMaxSampleRate);

        std::string rateErr;
        const std::string deviceName = deviceNameFromPhysicalChannel(cfg_.channel);
        if (!deviceName.empty()) {
            const double deviceMaxRate =
                queryDaqFloat64(DAQmxGetDevAOMaxRate, deviceName, rateErr, "DAQmxGetDevAOMaxRate failed");
            if (rateErr.empty() && deviceMaxRate > 0.0) {
                sampleRate = std::min(sampleRate, deviceMaxRate);
            }
        }

        sampleRate = std::max(sampleRate, kAbsoluteMinSampleRate);
        double workingSampleRate = sampleRate;
        std::string lastTimingErr;
        bool timingConfigured = false;

        for (int attempt = 0; attempt < kMaxRateClampAttempts && workingSampleRate >= kAbsoluteMinSampleRate;
             ++attempt) {
            const int generatedSamples = std::max(2, static_cast<int>(std::lround(durationSec * workingSampleRate)));
            const int finiteSamples = generatedSamples + 1;

            waveform_.assign(finiteSamples, 0.0);
            const double omega = 2.0 * 3.14159265358979323846 * cfg_.frequencyHz;
            for (int i = 0; i < generatedSamples; ++i) {
                const double t = static_cast<double>(i) / workingSampleRate;
                waveform_[i] = amplitude * std::sin(omega * t);
            }

            error = DAQmxCfgSampClkTiming(task, "", workingSampleRate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps,
                                          finiteSamples);
            if (!DAQmxFailed(error)) {
                error = DAQmxCfgOutputBuffer(task, static_cast<uInt32>(finiteSamples));
                if (!DAQmxFailed(error)) {
                    sampleRate_ = workingSampleRate;
                    samples_ = finiteSamples;
                    timingConfigured = true;
                    break;
                }
                lastTimingErr = formatDaqError("DAQmxCfgOutputBuffer failed", error);
            } else {
                lastTimingErr = formatDaqError("DAQmxCfgSampClkTiming failed", error);
            }

            waveform_.clear();
            sampleRate_ = 0.0;
            samples_ = 0;
            const double nextRate = std::floor(workingSampleRate * 0.5);
            if (nextRate >= workingSampleRate)
                break;
            workingSampleRate = std::max(kAbsoluteMinSampleRate, nextRate);
        }

        if (!timingConfigured) {
            err = !lastTimingErr.empty() ? lastTimingErr : "DAQ timing configuration failed";
            DAQmxClearTask(task);
            return false;
        }

        task_ = task;
        ready_ = true;
        return true;
#else
        (void)cfg_;
        err = "NI-DAQmx not available at build time";
        return false;
#endif
    } catch (const std::bad_alloc&) {
        shutdown();
        err = "DAQ waveform allocation failed";
        return false;
    } catch (const std::exception& e) {
        shutdown();
        err = std::string("DAQ init exception: ") + e.what();
        return false;
    } catch (...) {
        shutdown();
        err = "DAQ init exception: unknown";
        return false;
    }
}

void DaqTrigger::shutdown() {
#ifdef HAVE_NIDAQMX
    if (task_) {
        TaskHandle task = static_cast<TaskHandle>(task_);
        DAQmxStopTask(task);
        DAQmxClearTask(task);
        task_ = nullptr;
    }
    waveform_.clear();
    sampleRate_ = 0.0;
    samples_ = 0;
#endif
    ready_ = false;
    continuous_ = false;
}

bool DaqTrigger::fire(std::string& err) {
    return fireImpl(true, err);
}

bool DaqTrigger::fireImmediate(std::string& err) {
    return fireImpl(false, err);
}

bool DaqTrigger::fireImpl(bool honorDelay, std::string& err) {
    if (!ready_) {
        err = "DAQ trigger not initialized";
        return false;
    }
    if (continuous_) {
        err = "Continuous DAQ output is active";
        return false;
    }

    err.clear();
    try {
#ifdef HAVE_NIDAQMX
        if (honorDelay && cfg_.delayMs > 0.0) {
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(cfg_.delayMs));
        }

        if (samples_ <= 0 || waveform_.empty()) {
            err = "DAQ waveform not configured";
            return false;
        }

        TaskHandle task = static_cast<TaskHandle>(task_);
        DAQmxStopTask(task);

        int32 written = 0;
        int32 error =
            DAQmxWriteAnalogF64(task, samples_, 1, 10.0, DAQmx_Val_GroupByChannel, waveform_.data(), &written, nullptr);
        if (DAQmxFailed(error)) {
            err = formatDaqError("DAQmxWriteAnalogF64 failed", error);
            return false;
        }

        const double timeout =
            5.0 + (cfg_.durationMs + (honorDelay ? cfg_.delayMs : 0.0)) / 1000.0;
        error = DAQmxWaitUntilTaskDone(task, timeout);
        if (DAQmxFailed(error)) {
            err = formatDaqError("DAQmxWaitUntilTaskDone failed", error);
            DAQmxStopTask(task);
            return false;
        }
        DAQmxStopTask(task);
        return true;
#else
        err = "NI-DAQmx not available at build time";
        return false;
#endif
    } catch (const std::exception& e) {
        err = std::string("DAQ fire exception: ") + e.what();
        return false;
    } catch (...) {
        err = "DAQ fire exception: unknown";
        return false;
    }
}

bool DaqTrigger::startContinuous(std::string& err) {
    if (!ready_) {
        err = "DAQ trigger not initialized";
        return false;
    }
    if (continuous_) {
        err.clear();
        return true;
    }

#ifdef HAVE_NIDAQMX
    if (task_) {
        TaskHandle task = static_cast<TaskHandle>(task_);
        DAQmxStopTask(task);
        DAQmxClearTask(task);
        task_ = nullptr;
    }
    waveform_.clear();
    samples_ = 0;
    sampleRate_ = 0.0;
    ready_ = false;

    TaskHandle task = nullptr;
    int32 error = DAQmxCreateTask("", &task);
    if (DAQmxFailed(error)) {
        err = formatDaqError("DAQmxCreateTask failed", error);
        return false;
    }
    error = DAQmxCreateAOVoltageChan(task, cfg_.channel.c_str(), "",
                                     cfg_.rangeMin, cfg_.rangeMax,
                                     DAQmx_Val_Volts, nullptr);
    if (DAQmxFailed(error)) {
        err = formatDaqError("DAQmxCreateAOVoltageChan failed", error);
        DAQmxClearTask(task);
        return false;
    }

    const double maxAbs = std::min(std::abs(cfg_.rangeMin), std::abs(cfg_.rangeMax));
    const double amplitude = std::clamp(std::abs(cfg_.amplitude), 0.0, maxAbs);
    double sampleRate = std::clamp(cfg_.frequencyHz * kSamplesPerCycle,
                                   kMinSampleRate, kMaxSampleRate);
    std::string rateErr;
    const std::string deviceName = deviceNameFromPhysicalChannel(cfg_.channel);
    if (!deviceName.empty()) {
        const double deviceMaxRate =
            queryDaqFloat64(DAQmxGetDevAOMaxRate, deviceName, rateErr,
                            "DAQmxGetDevAOMaxRate failed");
        if (rateErr.empty() && deviceMaxRate > 0.0)
            sampleRate = std::min(sampleRate, deviceMaxRate);
    }
    sampleRate = std::max(sampleRate, kAbsoluteMinSampleRate);

    const int cycleSamples =
        std::max(2, static_cast<int>(std::lround(sampleRate / cfg_.frequencyHz)));
    waveform_.resize(cycleSamples);
    for (int i = 0; i < cycleSamples; ++i) {
        waveform_[i] =
            amplitude * std::sin(2.0 * 3.14159265358979323846
                                 * static_cast<double>(i)
                                 / static_cast<double>(cycleSamples));
    }

    error = DAQmxCfgSampClkTiming(task, "", sampleRate, DAQmx_Val_Rising,
                                  DAQmx_Val_ContSamps, cycleSamples);
    if (!DAQmxFailed(error))
        error = DAQmxCfgOutputBuffer(task, cycleSamples);
    int32 written = 0;
    if (!DAQmxFailed(error)) {
        error = DAQmxWriteAnalogF64(task, cycleSamples, 0, 10.0,
                                    DAQmx_Val_GroupByChannel, waveform_.data(),
                                    &written, nullptr);
    }
    if (!DAQmxFailed(error))
        error = DAQmxStartTask(task);
    if (DAQmxFailed(error)) {
        err = formatDaqError("Starting continuous DAQ output failed", error);
        DAQmxStopTask(task);
        DAQmxClearTask(task);
        waveform_.clear();
        return false;
    }

    task_ = task;
    sampleRate_ = sampleRate;
    samples_ = cycleSamples;
    continuous_ = true;
    ready_ = true;
    err.clear();
    return true;
#else
    err = "NI-DAQmx not available at build time";
    return false;
#endif
}

bool DaqTrigger::stopContinuous(std::string& err) {
    if (!continuous_) {
        err.clear();
        return true;
    }

#ifdef HAVE_NIDAQMX
    TaskHandle task = static_cast<TaskHandle>(task_);
    const int32 stopError = DAQmxStopTask(task);
    DAQmxClearTask(task);

    task_ = nullptr;
    continuous_ = false;
    ready_ = false;
    waveform_.clear();
    sampleRate_ = 0.0;
    samples_ = 0;

    int32 zeroError = 0;
    TaskHandle zeroTask = nullptr;
    zeroError = DAQmxCreateTask("", &zeroTask);
    if (!DAQmxFailed(zeroError)) {
        zeroError = DAQmxCreateAOVoltageChan(
            zeroTask, cfg_.channel.c_str(), "", cfg_.rangeMin, cfg_.rangeMax,
            DAQmx_Val_Volts, nullptr);
    }
    if (!DAQmxFailed(zeroError))
        zeroError = DAQmxWriteAnalogScalarF64(zeroTask, 1, 1.0, 0.0, nullptr);
    if (zeroTask)
        DAQmxClearTask(zeroTask);

    const DaqConfig savedConfig = cfg_;
    std::string restoreError;
    const bool restored = init(savedConfig, restoreError);

    if (DAQmxFailed(stopError)) {
        err = formatDaqError("Stopping continuous DAQ output failed", stopError);
        return false;
    }
    if (DAQmxFailed(zeroError)) {
        err = formatDaqError("Returning DAQ output to zero failed", zeroError);
        return false;
    }
    if (!restored) {
        err = "Continuous output stopped, but finite DAQ output could not be restored: "
            + restoreError;
        return false;
    }
    err.clear();
    return true;
#else
    err = "NI-DAQmx not available at build time";
    return false;
#endif
}

bool DaqTrigger::isReady() const {
    return ready_;
}

bool DaqTrigger::isContinuous() const {
    return continuous_;
}

double DaqTrigger::sampleRateHz() const {
#ifdef HAVE_NIDAQMX
    return sampleRate_;
#else
    return 0.0;
#endif
}

int DaqTrigger::finiteSampleCount() const {
#ifdef HAVE_NIDAQMX
    return samples_;
#else
    return 0;
#endif
}

double DaqTrigger::finalSampleValue() const {
#ifdef HAVE_NIDAQMX
    return waveform_.empty() ? 0.0 : waveform_.back();
#else
    return 0.0;
#endif
}
