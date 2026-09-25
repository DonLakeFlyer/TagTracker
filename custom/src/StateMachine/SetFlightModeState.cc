#include "SetFlightModeState.h"
#include "CustomLoggingCategory.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"

SetFlightModeState::SetFlightModeState(QState* parentState, const QString& flightMode)
    : CustomState   ("SetFlightModeState", parentState)
    , _flightMode   (flightMode)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
{
    _timeoutTimer.setSingleShot(true);
    _timeoutTimer.setInterval(kTimeoutMsecs);

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
        _timeoutTimer.start();
        _vehicle->setFlightMode(_flightMode);
    }
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
    disconnect(_vehicle, &Vehicle::flightModeChanged, this, &SetFlightModeState::_validateFlightModeChange);
    disconnect(&_timeoutTimer, &QTimer::timeout, this, &SetFlightModeState::_timeout);
}

void SetFlightModeState::_validateFlightModeChange(const QString& flightMode)
{
    if (flightMode == _flightMode) {
        qCDebug(CustomPluginLog) << "Flight mode changed flightMode:" << flightMode;
        _disconnectAll();
        emit flightModeChanged();
    }
}
