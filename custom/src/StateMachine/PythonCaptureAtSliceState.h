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
    struct ExplicitYaw {};

    PythonCaptureAtSliceState(QState* parentState, int headingIndex,
                              int sequenceIndex, uint32_t collectionId);
    // Confirmation revisit: fly this aircraft yaw as-is. The controller's fitted
    // bearing is in the same frame as the slice headings it was fitted from, so
    // the antenna offset is already folded in.
    PythonCaptureAtSliceState(QState* parentState, double yawDeg,
                              uint32_t sliceId, uint32_t collectionId, ExplicitYaw);

private:
    void _buildStates(uint32_t collectionId, uint32_t sliceId);
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState);

    double          _sliceHeadingDegrees    = 0;
    Vehicle*        _vehicle                = nullptr;
    CustomPlugin*   _customPlugin           = nullptr;
    CustomSettings* _customSettings         = nullptr;
    int             _rotationDivisions      = 0;
};
