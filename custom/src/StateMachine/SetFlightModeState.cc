#include "SetFlightModeState.h"
#include "CustomLoggingCategory.h"

#include "CustomStateMachine.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <algorithm>

SetFlightModeState::SetFlightModeState(QState* parentState, const QString& flightMode)
    : CustomState   ("SetFlightModeState", parentState)
    , _flightMode   (flightMode)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
{
    _timeoutTimer.setSingleShot(true);
    _timeoutTimer.setInterval(kTimeoutMsecs);
    _retryTimer.setSingleShot(true);

    connect(this, &SetFlightModeState::entered, this, &SetFlightModeState::_setFlightMode);
    connect(this, &SetFlightModeState::exited, this, &SetFlightModeState::_disconnectAll);
}

void SetFlightModeState::_setFlightMode()
{
    if (_vehicle->flightMode() == _flightMode) {
        qCDebug(CustomPluginLog) << "Flight mode already set flightMode:" << _flightMode;
        emit flightModeChanged();
    } else {
        qCDebug(CustomPluginLog) << "Setting flightMode:" << _flightMode;
        connect(_vehicle, &Vehicle::flightModeChanged, this, &SetFlightModeState::_validateFlightModeChange);
        connect(&_timeoutTimer, &QTimer::timeout, this, &SetFlightModeState::_timeout);
        connect(&_retryTimer, &QTimer::timeout, this, &SetFlightModeState::_retry);
        _timeoutTimer.start();
        _attempts = 0;
        _requestFlightMode();
    }
}

void SetFlightModeState::setRetry(int retryMsecs, int attempts)
{
    _retryTimer.setInterval(retryMsecs);
    _maxAttempts = std::max(attempts, 1);
}

void SetFlightModeState::_requestFlightMode()
{
    _attempts++;
    if (_modeRequest) {
        _modeRequest(_flightMode);
    } else {
        _vehicle->setFlightMode(_flightMode);
    }
    if (_attempts < _maxAttempts) {
        _retryTimer.start();
    }
}

void SetFlightModeState::_retry()
{
    // stop() is asynchronous: after a cancel nothing more may be sent
    const CustomStateMachine* machine = customMachine();
    if (machine && machine->stopping()) {
        return;
    }
    qCDebug(CustomPluginLog) << "Flight mode not reached, requesting again flightMode:" << _flightMode
                             << "attempt:" << (_attempts + 1) << "of" << _maxAttempts;
    _requestFlightMode();
}

void SetFlightModeState::_timeout()
{
    qCWarning(CustomPluginLog) << "Timeout waiting for flight mode change flightMode:" << _flightMode;
    _disconnectAll();
    setError("Timeout waiting for flight mode change to " + _flightMode);
}

void SetFlightModeState::_disconnectAll()
{
    _timeoutTimer.stop();
    _retryTimer.stop();
    disconnect(_vehicle, &Vehicle::flightModeChanged, this, &SetFlightModeState::_validateFlightModeChange);
    disconnect(&_timeoutTimer, &QTimer::timeout, this, &SetFlightModeState::_timeout);
    disconnect(&_retryTimer, &QTimer::timeout, this, &SetFlightModeState::_retry);
}

void SetFlightModeState::_validateFlightModeChange(const QString& flightMode)
{
    if (flightMode == _flightMode) {
        qCDebug(CustomPluginLog) << "Flight mode changed flightMode:" << flightMode;
        _disconnectAll();
        emit flightModeChanged();
    }
}
