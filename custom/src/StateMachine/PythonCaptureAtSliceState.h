#pragma once

#include "CustomState.h"

class Vehicle;
class CustomPlugin;
class CustomSettings;
class SendMavlinkCommandState;

// Per-slice state for Python detection mode within a persistent collection.
// Flow: announce → rotate command → wait for heading → START_COLLECTION_SLICE →
//       wait for COLLECTION_STATUS_SLICE_COMPLETE.
class PythonCaptureAtSliceState : public CustomState
{
    Q_OBJECT

public:
    PythonCaptureAtSliceState(QState* parentState, int headingIndex,
                              int sequenceIndex, uint32_t collectionId);

private:
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState);

    double          _sliceHeadingDegrees    = 0;
    Vehicle*        _vehicle                = nullptr;
    CustomPlugin*   _customPlugin           = nullptr;
    CustomSettings* _customSettings         = nullptr;
    int             _rotationDivisions      = 0;
};
