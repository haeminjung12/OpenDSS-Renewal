#pragma once

using int32 = int;
using uInt32 = unsigned int;
using bool32 = uInt32;
using float64 = double;
using TaskHandle = void*;

constexpr int32 DAQmx_Val_Volts = 10348;
constexpr int32 DAQmx_Val_Rising = 10280;
constexpr int32 DAQmx_Val_FiniteSamps = 10178;
constexpr int32 DAQmx_Val_GroupByChannel = 0;

inline bool DAQmxFailed(int32 status) {
    return status < 0;
}

extern "C" {
int32 DAQmxGetExtendedErrorInfo(char buffer[], uInt32 bufferSize);
int32 DAQmxGetSysDevNames(char buffer[], uInt32 bufferSize);
int32 DAQmxGetDevProductType(const char device[], char buffer[], uInt32 bufferSize);
int32 DAQmxGetDevAOPhysicalChans(const char device[], char buffer[], uInt32 bufferSize);
int32 DAQmxGetDevAOMaxRate(const char device[], float64* value);
int32 DAQmxCreateTask(const char name[], TaskHandle* task);
int32 DAQmxCreateAOVoltageChan(TaskHandle task, const char physicalChannel[], const char nameToAssignToChannel[],
                               float64 minVal, float64 maxVal, int32 units, const char customScaleName[]);
int32 DAQmxCfgSampClkTiming(TaskHandle task, const char source[], float64 rate, int32 activeEdge,
                            int32 sampleMode, uInt32 samplesPerChannel);
int32 DAQmxCfgOutputBuffer(TaskHandle task, uInt32 numSampsPerChan);
int32 DAQmxStopTask(TaskHandle task);
int32 DAQmxClearTask(TaskHandle task);
int32 DAQmxWriteAnalogF64(TaskHandle task, int32 numSampsPerChan, bool32 autoStart, float64 timeout,
                          bool32 dataLayout, const float64 writeArray[], int32* sampsPerChanWritten,
                          bool32* reserved);
int32 DAQmxWaitUntilTaskDone(TaskHandle task, float64 timeToWait);
}
