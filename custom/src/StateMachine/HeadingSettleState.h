#pragma once

#include <QElapsedTimer>
#include <QTimer>

#include "CustomState.h"

class Fact;

// Waits for the heading to stay within toleranceDeg of targetDeg for holdMsecs, so a slice
// does not start during the turn's overshoot. Gives up after maxWaitMsecs and still emits
// settled(): wind must cost a less steady slice, never the rotation.
class HeadingSettleState : public CustomState
{
    Q_OBJECT

public:
    HeadingSettleState(QState* parentState, Fact* heading, double targetDeg, double toleranceDeg = 3.0,
                       int holdMsecs = 1000, int maxWaitMsecs = 4000);

signals:
    void settled();

private slots:
    void _headingChanged(QVariant rawValue);
    void _holdTimeout();
    void _maxWaitTimeout();
    void _disconnectAll();

private:
    double _errorDeg(double headingDeg) const;

    Fact* _heading;
    double _targetDeg;
    double _toleranceDeg;
    double _maxErrorDeg = 0;
    QTimer _holdTimer;
    QTimer _maxWaitTimer;
    QElapsedTimer _elapsed;
};
