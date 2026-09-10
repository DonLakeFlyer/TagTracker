#pragma once

#include "CustomState.h"

#include <QList>

#include <cstdint>

class CustomPlugin;
class CustomSettings;
class FunctionState;
class DetectorList;
class QFinalState;

// Full-rotation state machine for Python detection mode. Owns one persistent
// collection: START_COLLECTION → one PythonCaptureAtSliceState per heading →
// FINISH_COLLECTION → either BEARING_RESULT, or COLLECTION_STATUS_REVISIT_REQUESTED
// (winning lock seen on one heading only) → one more slice at the requested
// heading → FINISH_COLLECTION again → BEARING_RESULT. Any
// COLLECTION_STATUS_FAILED for this collection aborts the machine.
class PythonRotateAndCaptureState : public CustomState
{
    Q_OBJECT

public:
    PythonRotateAndCaptureState(QState* parentState);

    // Heading-index visit order. With a finite prior bearing: nearest slice first, then
    // alternating outward clockwise/counter-clockwise. Without one: spread order for 8
    // slices, sequential otherwise.
    static QList<int> sliceVisitOrder(int rotationDivisions, double priorBearingDeg, double antennaOffsetDeg);

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);
    void _buildRevisitSlice(float headingDeg);

private:
    void _rotationBegin();
    void _rotationEnd();

    CustomPlugin*   _customPlugin       = nullptr;
    CustomSettings* _customSettings     = nullptr;
    uint32_t        _collectionId       = 0;
    int             _rotationDivisions  = 0;
    // Populated on demand once the controller names the revisit heading
    QState*         _revisitState       = nullptr;
    QFinalState*    _revisitDone        = nullptr;
};
