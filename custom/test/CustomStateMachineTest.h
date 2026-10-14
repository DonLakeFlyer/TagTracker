#pragma once

#include "VehicleTest.h"

class CustomStateMachineTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void _flightModeChangeAfterFinishDoesNotCancel();
    void _flightModeChangeWhileRunningCancels();
};
