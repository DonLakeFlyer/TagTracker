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
    Vehicle*            _vehicle;
    CustomPlugin*       _customPlugin;
    CustomSettings*     _customSettings;
    double              _firstHeading = 0;
    double              _degreesPerSlice = 0;
    FunctionState*      _rotationBeginState = nullptr;
    FunctionState*      _rotationEndState = nullptr;
    int                 _rotationDivisions = 0;
    DetectorList*       _detectorList = nullptr;

private:
    void _rotationBegin();
    void _rotationEnd();
    double _sliceIndexToHeading(int sliceIndex);
};
