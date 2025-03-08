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
    _timeoutTimer.setInterval(500);

    connect(this, &SetFlightModeState::entered, this, &SetFlightModeState::_setFlightMode);
}

void SetFlightModeState::_setFlightMode()
{
    if (_vehicle->flightMode() == _flightMode) {
        qCDebug(CustomPluginLog) << "Flight mode already set to" << _flightMode << " - " << Q_FUNC_INFO;
        emit flightModeChanged();
    } else {
        connect(_vehicle, &Vehicle::flightModeChanged, this, &SetFlightModeState::_validateFlightModeChange);
        connect(&_timeoutTimer, &QTimer::timeout, this, &SetFlightModeState::_timeout);
        _timeoutTimer.start();
        _vehicle->setFlightMode(_flightMode);
    }
}

void SetFlightModeState::_timeout()
{
    qCWarning(CustomPluginLog) << "Timeout waiting for flight mode change to" << _flightMode << " - " << Q_FUNC_INFO;
    _disconnectAll();
    emit error();
}
void SetFlightModeState::_disconnectAll()
{
    disconnect(_vehicle, &Vehicle::flightModeChanged, this, &SetFlightModeState::_validateFlightModeChange);
    disconnect(&_timeoutTimer, &QTimer::timeout, this, &SetFlightModeState::_timeout);
}

void SetFlightModeState::_validateFlightModeChange(const QString& flightMode)
{
    if (flightMode == _flightMode) {
        qCDebug(CustomPluginLog) << "Flight mode succesfully changed to" << flightMode << " - " << Q_FUNC_INFO;
        _disconnectAll();
        emit flightModeChanged();
    }
}