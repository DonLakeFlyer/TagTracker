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
    RotateAndCaptureState(QState* parentState);

private:
    CustomState* _rotateAndCaptureState(QState* parentState, double headingDegrees);
    SendMavlinkCommandState* _rotateMavlinkCommandState(QState* parentState, double headingDegrees);

    Vehicle*        _vehicle = nullptr;
    CustomPlugin*   _customPlugin = nullptr;
    CustomSettings* _customSettings = nullptr;
};
