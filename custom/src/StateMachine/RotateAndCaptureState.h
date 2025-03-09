#pragma once

#include "CustomState.h"

#include <QTimer>

class Vehicle;
class CustomSettings;
class SendMavlinkCommandState;
class CustomPlugin;
class DetectorList;

class RotateAndCaptureState : public CustomState
{
    Q_OBJECT

public:
    RotateAndCaptureState(QState* parentState);

signals:
    void _rotateAndCaptureAtHeadingFinished();
    void _cancelAndReturn();

private:
    CustomState* _rotateAndCaptureAtHeadingState(QState* parentState, double headingDegrees);
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState, double headingDegrees);
    void _initNewRotationDuringFlight();
    void _sliceBegin();
    void _sliceEnd();

private:
    Vehicle*        _vehicle = nullptr;
    CustomPlugin*   _customPlugin = nullptr;
    CustomSettings* _customSettings = nullptr;
    DetectorList*   _detectorList = nullptr;
    int             _currentSlice = 0;
    int             _rotationDivisions = 0;
};
