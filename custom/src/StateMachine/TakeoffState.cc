#include "TakeoffState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"

TakeoffState::TakeoffState(QState* parentState, double takeoffAltRel)
    : CustomState       ("TakeoffState", parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _takeoffAltRel    (takeoffAltRel)
    , _guidedFlightMode (qobject_cast<CustomPlugin*>(CustomPlugin::instance())->holdFlightMode())
    , _takeoffFlightMode(_vehicle->firmwarePlugin()->takeOffFlightMode())
{
    _timeoutTimer.setSingleShot(true);
    _timeoutTimer.setInterval(90 * 1000);
    connect(&_timeoutTimer, &QTimer::timeout, this, &TakeoffState::_takeoffTimeout);

    _settleTimer.setSingleShot(true);
    _settleTimer.setInterval(5 * 1000);
    connect(&_settleTimer, &QTimer::timeout, this, &TakeoffState::_settleTimeout);

    connect(this, &QState::entered, this, &TakeoffState::_onEntry);
}

void TakeoffState::_onEntry()
{
    if (_vehicle->flying()) {
        setError("Vehicle is already flying");
        return;
    }

    connect(_vehicle, &Vehicle::flightModeChanged, this, &TakeoffState::_flightModeChanged);

    _vehicle->guidedModeTakeoff(_takeoffAltRel);
    _timeoutTimer.start();
    _flightModeChanged(_vehicle->flightMode());
}

void TakeoffState::_disconnectAll()
{
    _timeoutTimer.stop();
    _settleTimer.stop();
    disconnect(_vehicle, &Vehicle::flightModeChanged, this, &TakeoffState::_flightModeChanged);
    disconnect(&_timeoutTimer, &QTimer::timeout, this, &TakeoffState::_takeoffTimeout);
    disconnect(&_settleTimer, &QTimer::timeout, this, &TakeoffState::_settleTimeout);
}

void TakeoffState::_flightModeChanged(const QString& flightMode)
{
    if (flightMode == _takeoffFlightMode) {
        qCDebug(CustomPluginLog) << "Takeoff flight mode detected" << " - " << Q_FUNC_INFO;
        _takeoffDetected = true;
    } else if (_takeoffDetected && flightMode == _guidedFlightMode) {
        qCDebug(CustomPluginLog) << "Guided flight mode detected. Waiting for altitude to settle." << " - " << Q_FUNC_INFO;
        _timeoutTimer.stop();
        _settleTimer.start();
    }
}

void TakeoffState::_takeoffTimeout()
{
    QString flightMode = _takeoffDetected ? _guidedFlightMode : _takeoffFlightMode;
    setError(QStringLiteral("Vehicle failed to enter %1 flight mode").arg(flightMode));
    _disconnectAll();
}

void TakeoffState::_settleTimeout()
{
    _disconnectAll();
    qDebug() << _vehicle->altitudeRelative()->rawValue().toDouble() << _takeoffAltRel;
    if (qAbs(_vehicle->altitudeRelative()->rawValue().toDouble() - _takeoffAltRel) <= 1.0) {
        qCDebug(CustomPluginLog) << "Takeoff completed" << " - " << Q_FUNC_INFO;
        emit takeoffComplete();
    } else {
        setError("Vehicle failed to reach takeoff altitude");
    }
}