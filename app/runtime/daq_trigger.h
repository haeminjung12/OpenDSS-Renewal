#pragma once

#include <string>
#include <vector>

struct DaqConfig {
    std::string channel = "Dev1/ao0";
    double rangeMin = -10.0;
    double rangeMax = 10.0;
    double amplitude = 5.0;
    double frequencyHz = 1000.0;
    double durationMs = 5.0;
    double delayMs = 0.0;
};

struct DaqDeviceInfo {
    std::string name;
    std::string productType;
    std::vector<std::string> aoChannels;

    bool isCompatible() const { return !aoChannels.empty(); }
    std::string preferredChannel() const;
};

std::vector<DaqDeviceInfo> discoverDaqDevices(std::string& err);

class DaqTrigger {
public:
    DaqTrigger();
    ~DaqTrigger();
    DaqTrigger(const DaqTrigger&) = delete;
    DaqTrigger& operator=(const DaqTrigger&) = delete;
    DaqTrigger(DaqTrigger&&) = delete;
    DaqTrigger& operator=(DaqTrigger&&) = delete;

    bool init(const DaqConfig& cfg, std::string& err);
    void shutdown();
    bool fire(std::string& err);
    bool isReady() const;

private:
    DaqConfig cfg_;
    bool ready_;

#ifdef HAVE_NIDAQMX
    void* task_;
    double sampleRate_ = 0.0;
    int samples_ = 0;
    std::vector<double> waveform_;
#endif
};
