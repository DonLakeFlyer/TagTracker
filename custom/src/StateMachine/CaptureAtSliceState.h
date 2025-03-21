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

class CaptureAtSliceState : public CustomState
{
    Q_OBJECT

public:
    CaptureAtSliceState(QState* parentState, int sliceIndex);

signals:
    void _rotateAndCaptureAtHeadingFinished();
    void _cancelAndReturn();

private:
    CustomState* _rotateAndCaptureAtHeadingState();
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState);
    void _sliceBegin();
    void _sliceEnd();

    int                 _sliceIndex = 0;
    Vehicle*            _vehicle = nullptr;
    CustomPlugin*       _customPlugin = nullptr;
    CustomSettings*     _customSettings = nullptr;
    double              _sliceHeadingDegrees = 0;
    int                 _rotationDivisions = 0;
    QList<CustomState*> _rotationStates;
    DetectorList*       _detectorList = nullptr;
};
