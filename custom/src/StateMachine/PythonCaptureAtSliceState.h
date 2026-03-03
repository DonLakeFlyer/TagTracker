#pragma once

#include "CustomState.h"

class Vehicle;
class CustomPlugin;
class CustomSettings;
class SendMavlinkCommandState;

// Per-slice state for Python detection mode.
// Flow: announce → rotate command → wait for heading → start detection →
//       wait for result (pulse or no-pulse from all detectors) → stop detection.
class PythonCaptureAtSliceState : public CustomState
{
    Q_OBJECT

public:
    PythonCaptureAtSliceState(QState* parentState, int sliceIndex);

private:
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState);

    double          _sliceHeadingDegrees    = 0;
    Vehicle*        _vehicle                = nullptr;
    CustomPlugin*   _customPlugin           = nullptr;
    CustomSettings* _customSettings         = nullptr;
    int             _rotationDivisions      = 0;
};
