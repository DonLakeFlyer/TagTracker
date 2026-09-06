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
    void _pythonInvalidNoiseIsNotAMeasurement();
    void _pythonBelowNoiseDisplaysNegativeDb();
    void _confirmedMeasurementReplacesPriorSliceValue();
    void _confirmedMeasurementsAggregateAcrossTags();
    void _startupWaitIsNotHeartbeatFailure();
    void _flightModeChangeTimeoutToleratesSlowLink();
    void _sliceVisitOrder_data();
    void _sliceVisitOrder();
};
