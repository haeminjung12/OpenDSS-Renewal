#pragma once

#include <QString>

namespace desktop_app {

struct AppState {
    QString targetClassId;
    QString activeModelId;
    bool sortNonTarget = false;

    bool triggerArmed = false;
    bool cameraStreaming = false;

    bool daqAvailable = false;
    bool daqDisabled = false;
    bool daqFault = false;
    bool daqWaveformValid = false;
    QString daqStatusText;
    QString daqFaultText;
};

} // namespace desktop_app
