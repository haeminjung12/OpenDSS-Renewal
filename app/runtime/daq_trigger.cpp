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
            const int samples = std::max(2, static_cast<int>(std::lround(durationSec * workingSampleRate)));

            waveform_.assign(samples, 0.0);
            const double omega = 2.0 * 3.14159265358979323846 * cfg_.frequencyHz;
            for (int i = 0; i < samples; ++i) {
                const double t = static_cast<double>(i) / workingSampleRate;
                waveform_[i] = amplitude * std::sin(omega * t);
            }

            error =
                DAQmxCfgSampClkTiming(task, "", workingSampleRate, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, samples);
            if (!DAQmxFailed(error)) {
                error = DAQmxCfgOutputBuffer(task, static_cast<uInt32>(samples));
                if (!DAQmxFailed(error)) {
                    sampleRate_ = workingSampleRate;
                    samples_ = samples;
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
}

bool DaqTrigger::fire(std::string& err) {
    if (!ready_) {
        err = "DAQ trigger not initialized";
        return false;
    }

    err.clear();
    try {
#ifdef HAVE_NIDAQMX
        if (cfg_.delayMs > 0.0) {
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

        double timeout = 5.0 + (cfg_.durationMs + cfg_.delayMs) / 1000.0;
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

bool DaqTrigger::isReady() const {
    return ready_;
}
