#pragma once

#include "UnitTest.h"

class TagTrackerPulseDisplayTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _pythonUsesSignalPower();
    void _legacyUsesSNR();
    void _pythonDisplaysDbAboveNoise();
    void _pythonNonPositivePowerDisplaysZero();
    void _confirmedMeasurementReplacesPriorSliceValue();
    void _startupWaitIsNotHeartbeatFailure();
    void _flightModeChangeTimeoutToleratesSlowLink();
};
