#pragma once

#include "CustomState.h"

#include <QTimer>

class CustomPlugin;

// Rotates to a heading of 0 degrees and then waits for a heartbeat from the two rate detectors before signaling completion

class RotateAndRateHeartbeatWaitState : public CustomState
{
    Q_OBJECT

public:
    RotateAndRateHeartbeatWaitState(QState* parentState, double heading);

signals:
    void heartbeatsReceived();

private slots:
    void _detectorHeartbeatReceived(int oneBasedRateIndex);

private:
    void _heartbeatWaitStateEntered();
    void _disconnectAll();

    CustomPlugin*   _customPlugin           = nullptr;
    bool            _rateDetector1Heartbeat = false;
    bool            _rateDetector2Heartbeat = false;
    QTimer          _heartbeatTimeoutTimer;
};