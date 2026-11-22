#pragma once

#include "GridGenerator.h"
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
    void _previewLatchesAtTap();
    void _confirmFliesLatchedPreview();
    void _movedGuardRefusesAndClears();
    void _cancelClearsPreview();
    void _missionSwitchRetried();
    void _pilotCancelStopsMissionRetries();

private:
    void _takeoff();
    /// Starts a grid machine at a fixed pose and waits until it monitors the flying mission.
    AutoGridStateMachine* _startMonitoringGrid();
    void _sendMissionCurrent(int wireSeq);
    /// Waits for the grid's mission upload and checks it is exactly the previewed grid.
    void _verifyUploadedGrid(const GridGenerator::Result& previewed);
    /// Ends a running grid the way a pilot would.
    void _endGridByPilot();
};
