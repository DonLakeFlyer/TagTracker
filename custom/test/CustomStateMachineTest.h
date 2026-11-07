#pragma once

#include "VehicleTest.h"

class CustomStateMachineTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void _flightModeChangeAfterFinishDoesNotCancel();
    void _flightModeChangeWhileRunningCancels();
    void _allowedFlightModeDoesNotCancel();
    void _errorAfterCancelIsIgnored();
    void _cancelWhileRunningStopsWithoutRTL();
    void _stallAfterTakeoffReturns();
    void _headingTargetWrapsAtNorth();
    void _headingSettleWaitsOutOvershoot();
    void _headingSettleGivesUpInGusts();
};
