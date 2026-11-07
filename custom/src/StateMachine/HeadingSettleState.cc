#include "HeadingSettleState.h"

#include <cmath>

#include "CustomLoggingCategory.h"
#include "Fact.h"

HeadingSettleState::HeadingSettleState(QState* parentState, Fact* heading, double targetDeg, double toleranceDeg,
                                       int holdMsecs, int maxWaitMsecs)
    : CustomState("HeadingSettleState", parentState)
    , _heading(heading)
    , _targetDeg(targetDeg)
    , _toleranceDeg(toleranceDeg)
{
    _holdTimer.setSingleShot(true);
    _holdTimer.setInterval(holdMsecs);
    _maxWaitTimer.setSingleShot(true);
    _maxWaitTimer.setInterval(maxWaitMsecs);
    connect(&_holdTimer, &QTimer::timeout, this, &HeadingSettleState::_holdTimeout);
    connect(&_maxWaitTimer, &QTimer::timeout, this, &HeadingSettleState::_maxWaitTimeout);

    connect(this, &QState::entered, this, [this]() {
        _maxErrorDeg = 0;
        _elapsed.start();
        connect(_heading, &Fact::rawValueChanged, this, &HeadingSettleState::_headingChanged);
        _maxWaitTimer.start();
        _headingChanged(_heading->rawValue());
    });
    connect(this, &QState::exited, this, &HeadingSettleState::_disconnectAll);
}

double HeadingSettleState::_errorDeg(double headingDeg) const
{
    return qAbs(std::remainder(headingDeg - _targetDeg, 360.0));
}

void HeadingSettleState::_headingChanged(QVariant rawValue)
{
    const double errorDeg = _errorDeg(rawValue.toDouble());
    _maxErrorDeg = qMax(_maxErrorDeg, errorDeg);
    if (!std::isfinite(errorDeg) || errorDeg > _toleranceDeg) {
        _holdTimer.stop();
    } else if (!_holdTimer.isActive()) {
        _holdTimer.start();
    }
}

void HeadingSettleState::_holdTimeout()
{
    qCDebug(CustomPluginLog) << "Heading settled at" << _targetDeg << "deg after" << _elapsed.elapsed()
                             << "ms, max error" << _maxErrorDeg << "deg";
    _disconnectAll();
    emit settled();
}

void HeadingSettleState::_maxWaitTimeout()
{
    qCWarning(CustomPluginLog) << "Heading did not settle at" << _targetDeg << "deg within" << _elapsed.elapsed()
                               << "ms (error now" << _errorDeg(_heading->rawValue().toDouble()) << "deg, max"
                               << _maxErrorDeg << "deg); starting slice anyway";
    _disconnectAll();
    emit settled();
}

void HeadingSettleState::_disconnectAll()
{
    _holdTimer.stop();
    _maxWaitTimer.stop();
    disconnect(_heading, &Fact::rawValueChanged, this, &HeadingSettleState::_headingChanged);
}
