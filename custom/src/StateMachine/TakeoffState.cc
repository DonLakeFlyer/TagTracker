#include "TakeoffState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"

#include "FactMetaData.h"
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
    connect(this, &QState::exited, this, &TakeoffState::_disconnectAll);
}

void TakeoffState::_onEntry()
{
    if (_vehicle->airborne()) {
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
        qCDebug(CustomPluginLog) << "Takeoff flight mode detected";
        _takeoffDetected = true;
    } else if (_takeoffDetected && flightMode == _guidedFlightMode) {
        qCDebug(CustomPluginLog) << "Guided flight mode detected. Waiting for altitude to settle.";
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

    // Baro drift and firmware takeoff acceptance radius grow with altitude, so scale the tolerance
    const double altitudeRel = _vehicle->altitudeRelative()->rawValue().toDouble();
    const double tolerance = qMax(_minAltToleranceMeters, _takeoffAltRel * _altToleranceFraction);
    qCDebug(CustomPluginLog) << "altitudeRelative:" << altitudeRel
                             << "takeoffAltRel:" << _takeoffAltRel
                             << "tolerance:" << tolerance;
    if (qAbs(altitudeRel - _takeoffAltRel) <= tolerance) {
        qCDebug(CustomPluginLog) << "Takeoff completed";
        emit takeoffComplete();
    } else {
        const QString units = FactMetaData::appSettingsVerticalDistanceUnitsString();
        setError(QStringLiteral("Vehicle failed to reach takeoff altitude. Altitude %1 %4, expected %2 %4 within %3 %4.")
                     .arg(FactMetaData::metersToAppSettingsVerticalDistanceUnits(altitudeRel).toDouble(), 0, 'f', 0)
                     .arg(FactMetaData::metersToAppSettingsVerticalDistanceUnits(_takeoffAltRel).toDouble(), 0, 'f', 0)
                     .arg(FactMetaData::metersToAppSettingsVerticalDistanceUnits(tolerance).toDouble(), 0, 'f', 0)
                     .arg(units));
    }
}
