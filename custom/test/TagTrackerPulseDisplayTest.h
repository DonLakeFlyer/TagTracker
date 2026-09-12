#pragma once

#include "UnitTest.h"

class TagTrackerPulseDisplayTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _pythonUsesSignalPower();
    void _pythonDisplaysDbAboveNoise();
    void _pythonNonPositivePowerDisplaysZero();
    void _pythonInvalidNoiseIsNotAMeasurement();
    void _pythonBelowNoiseDisplaysNegativeDb();
    void _rateLabel_data();
    void _rateLabel();
    void _confirmedMeasurementReplacesPriorSliceValue();
    void _confirmedMeasurementsAggregateAcrossTags();
    void _startupWaitIsNotHeartbeatFailure();
    void _heartbeatWatchdogArmsOnlyWhenStarted();
    void _flightModeChangeTimeoutToleratesSlowLink();
    void _sliceVisitOrder_data();
    void _sliceVisitOrder();
    void _bearingResultValidity_data();
    void _bearingResultValidity();
    void _bearingState();
    void _bearingSector_data();
    void _bearingSector();
    void _outcomeAnnouncement();
};
