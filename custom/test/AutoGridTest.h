#pragma once

#include "VehicleTest.h"

class AutoGridStateMachine;

class AutoGridTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void _settingsDefaults();
    void _normalizeHeading_data();
    void _normalizeHeading();
    void _classifyFlightModeChange_data();
    void _classifyFlightModeChange();
    void _isAtMissionEnd();
    void _latchParams();
    void _uploadsSetsCurrentAndStartsMission();
    void _endOfMission_data();
    void _endOfMission();
    void _cancelSendsNothingMore();
    void _gridAndRotationExclusive();

private:
    void _takeoff();
    /// Starts a grid machine at a fixed pose and waits until it monitors the flying mission.
    AutoGridStateMachine* _startMonitoringGrid();
    void _sendMissionCurrent(int wireSeq);
};
