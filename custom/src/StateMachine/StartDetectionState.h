#pragma once

#include "CustomState.h"

class StartDetectionState : public CustomState
{
    Q_OBJECT

public:
    // sendCommand=false skips sending START_DETECTION (Python mode manages it per-slice)
    StartDetectionState(QState* parentState, bool sendCommand = true);

signals:
    void tunerSucceeded();
    void checkForSelectedTagsSucceeded();

private:
    void _checkTuner();
    void _checkForSelectedTags();
    void _startPulseLogging();

    bool _channelizerTunerFailed = false;
};
