#pragma once

#include "CustomState.h"

class CustomPlugin;
class CustomSettings;
class FunctionState;
class DetectorList;

// Full-rotation state machine for Python detection mode.
// For each slice: rotate to heading, start detection, wait for pulse/no-pulse
// from all detectors, stop detection.  No rate-detector heartbeat gate needed.
class PythonRotateAndCaptureState : public CustomState
{
    Q_OBJECT

public:
    PythonRotateAndCaptureState(QState* parentState);

private:
    void _rotationBegin();
    void _rotationEnd();

    CustomPlugin*   _customPlugin       = nullptr;
    CustomSettings* _customSettings     = nullptr;
};
