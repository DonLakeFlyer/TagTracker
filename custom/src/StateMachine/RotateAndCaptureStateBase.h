#pragma once

#include "CustomState.h"

class Vehicle;
class CustomSettings;
class SendMavlinkCommandState;
class CustomPlugin;
class DetectorList;
class FunctionState;
class SayState;
class QFinalState;

class RotateAndCaptureStateBase : public CustomState
{
    Q_OBJECT

public:
    RotateAndCaptureStateBase(const QString& stateName, QState* parentState);

signals:
    void _rotateAndCaptureAtHeadingFinished();
    void _cancelAndReturn();

protected:
    CustomState* _rotateAndCaptureAtHeadingState(QState* parentState, int sliceIndex);

    Vehicle*            _vehicle;
    CustomPlugin*       _customPlugin;
    CustomSettings*     _customSettings;
    double              _firstHeading = 0;
    double              _degreesPerSlice = 0;
    FunctionState*      _initRotationState = nullptr;
    SayState*           _announceRotateCompleteState = nullptr;
    QFinalState*        _finalState = nullptr;
    int                 _rotationDivisions = 0;
    QList<CustomState*> _rotationStates;
    DetectorList*       _detectorList = nullptr;

private:
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState, double headingDegrees);
    void _initNewRotationDuringFlight();
    void _sliceBegin();
    void _sliceEnd(int sliceIndex);
    double _sliceIndexToHeading(int sliceIndex);
};
