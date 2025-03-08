#pragma once

#include "CustomState.h"

#include <QTimer>

class Vehicle;
class CustomSettings;
class SendMavlinkCommandState;
class CustomPlugin;

class RotateAndCaptureState : public CustomState
{
    Q_OBJECT

public:
    RotateAndCaptureState(QState* parentState, bool rtlOnFlightModeChange);

signals:
    void _rotateAndCaptureAtHeadingFinished();
    void _cancelAndReturn();

private:
    CustomState* _rotateAndCaptureAtHeadingState(QState* parentState, double headingDegrees, bool rtlOnFlightModeChange);
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState, double headingDegrees);

private:
    Vehicle*        _vehicle = nullptr;
    CustomPlugin*   _customPlugin = nullptr;
    CustomSettings* _customSettings = nullptr;
};
