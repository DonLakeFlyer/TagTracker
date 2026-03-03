#pragma once

#include "CustomState.h"

class StopDetectionState : public CustomState
{
    Q_OBJECT

public:
    // sendCommand=false skips sending STOP_DETECTION (Python mode already stopped it after the last slice)
    StopDetectionState(QState* parentState, bool sendCommand = true);

private:
    void _clearDetectorList();
    void _stopPulseLogging();
};
