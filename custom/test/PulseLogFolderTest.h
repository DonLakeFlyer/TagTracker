#pragma once

#include "VehicleTestManualConnect.h"

class PulseLogFolderTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _twoRunsShareOneFolder();
    void _openRunSidecarHasNoStop();
    void _disconnectStartsNewFolder();
    void _rotationLogsNumberedPerFolder();
    void _noVehicleRunGetsOwnFolder();
};
