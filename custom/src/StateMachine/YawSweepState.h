#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include "CustomState.h"

class Vehicle;
class CustomPlugin;

/// Yaws the hovering vehicle one full turn at a constant rate; completes once the
/// measured heading has turned the full 360 degrees. PX4: one DO_ORBIT facing the
/// circle centre, so any pilot mode change ends it. ArduPilot: streamed CONDITION_YAW.
class YawSweepState : public CustomState
{
    Q_OBJECT

public:
    // progressOffsetDeg: degrees already reported by earlier turns of the same sweep
    YawSweepState(QState* parentState, bool clockwise, double rateDegPerSec, uint32_t progressOffsetDeg);

signals:
    void sweepComplete();
    void orbitSent();

private:
    void _start();
    void _stop();
    void _tick();
    void _sendHeading(double headingDeg);
    void _sendOrbit();
    bool _checkOrbitMode();
    void _mavCommandResult(int vehicleId, int targetComponent, int command, int ackResult, int failureCode);
    static CustomPlugin* _customPlugin();
    QString _progressMessage() const;

    static constexpr int _tickMSecs = 500;
    static constexpr double _settledToleranceDeg = 2.0;
    // No turn of at least _stallProgressDeg within this window fails the sweep; covers spin-up and the slowest rate
    static constexpr int _stallTimeoutMSecs = 15 * 1000;
    static constexpr double _stallProgressDeg = 2.0;
    static constexpr double _orbitRadiusM = 3.0;
    static constexpr int _orbitEntryTimeoutMSecs = 5 * 1000;

    Vehicle* _vehicle = nullptr;
    bool _clockwise = true;
    double _rateDegPerSec = 0;
    uint32_t _progressOffsetDeg = 0;
    double _startHeadingDeg = 0;
    double _lastHeadingDeg = 0;
    double _turnedDeg = 0;  // unwrapped, positive clockwise
    bool _orbitSeen = false;
    double _lastProgressDeg = 0;
    qint64 _lastProgressMSecs = 0;
    QElapsedTimer _elapsed;
    QTimer _tickTimer;
};
