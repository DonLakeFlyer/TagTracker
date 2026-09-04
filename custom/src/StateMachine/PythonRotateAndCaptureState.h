#pragma once

#include "CustomState.h"

#include <cstdint>

class CustomPlugin;
class CustomSettings;
class FunctionState;
class DetectorList;

// Full-rotation state machine for Python detection mode. Owns one persistent
// collection: START_COLLECTION → one PythonCaptureAtSliceState per heading →
// FINISH_COLLECTION (controller computes and sends the bearing).  Any
// COLLECTION_STATUS_FAILED for this collection aborts the machine.
class PythonRotateAndCaptureState : public CustomState
{
    Q_OBJECT

public:
    PythonRotateAndCaptureState(QState* parentState);

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);

private:
    void _rotationBegin();
    void _rotationEnd();

    CustomPlugin*   _customPlugin       = nullptr;
    CustomSettings* _customSettings     = nullptr;
    uint32_t        _collectionId       = 0;
};
