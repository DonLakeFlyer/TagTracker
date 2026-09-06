#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"

#include "QGCApplication.h"
#include "AudioOutput.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

CustomStateMachine::CustomStateMachine(const QString& machineName, QObject* parent)
    : QGCStateMachine(machineName, MultiVehicleManager::instance()->activeVehicle(), parent)
{
    connect(this, &CustomStateMachine::stopped, this, [this] () {
        qobject_cast<CustomPlugin*>(CustomPlugin::instance())->rotationIsEnding();
        if (!_vehicleLost) {
            disconnect(vehicle(), &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
        }
    });

    // Callers guard against a null active vehicle before constructing a machine
    connect(vehicle(), &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    connect(MultiVehicleManager::instance(), &MultiVehicleManager::vehicleRemoved, this, &CustomStateMachine::_vehicleRemoved);
}

void CustomStateMachine::setError(const QString& errorString)
{
    const bool rtlOnError = _eventMode & RTLOnError;

    qCWarning(CustomStateMachineLog) << "errorString" << errorString << " - " << Q_FUNC_INFO;
    _errorString = errorString;
    if (!_vehicleLost && vehicle()->flying()) {
        AudioOutput::instance()->say(QStringLiteral("%1 failed. %2").arg(objectName()).arg(rtlOnError ? "Returning" : "User is in control of vehicle"));
        if (rtlOnError) {
            vehicle()->setFlightMode(vehicle()->rtlFlightMode());
        }
    } else {
        AudioOutput::instance()->say(QStringLiteral("%1 failed").arg(objectName()));
    }
    displayError();
    _runStopHandler();
    stopMachine();
}

void CustomStateMachine::displayError()
{
    qCWarning(CustomStateMachineLog) << _errorString << " - " << Q_FUNC_INFO;
    qgcApp()->showAppMessage(_errorString);
    _errorString.clear();
}

void CustomStateMachine::_flightModeChanged(const QString& flightMode)
{
    const QString holdFlightMode = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->holdFlightMode();

    if (_eventMode & CancelOnFlightModeChange && flightMode != holdFlightMode) {
        disconnect(vehicle(), &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
        AudioOutput::instance()->say(QStringLiteral("%1 cancelled. User is in control of vehicle.").arg(objectName()));
        _runStopHandler();
        stopMachine();
    }
}

void CustomStateMachine::_vehicleRemoved(Vehicle* vehicle)
{
    if (vehicle != this->vehicle() || _vehicleLost) {
        return;
    }
    _vehicleLost = true;
    disconnect(vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);

    if (!isRunning()) {
        return;
    }
    qCWarning(CustomStateMachineLog) << "Vehicle removed while running, cancelling" << objectName() << " - " << Q_FUNC_INFO;
    AudioOutput::instance()->say(QStringLiteral("%1 cancelled. Vehicle disconnected.").arg(objectName()));
    _runStopHandler();
    stopMachine();
}

void CustomStateMachine::setEventMode(uint eventMode)
{
    if (eventMode == 0) {
        qCDebug(CustomStateMachineLog) << "Clearing all event modes"<< " - " << Q_FUNC_INFO;
    } else {
        if (eventMode & CancelOnFlightModeChange) {
            qCDebug(CustomStateMachineLog) << "Setting event mode: CancelOnFlightModeChange" << " - " << Q_FUNC_INFO;
        }
        if (eventMode & RTLOnError) {
            qCDebug(CustomStateMachineLog) << "Setting event mode: RTLOnError" << " - " << Q_FUNC_INFO;
        }
    }
    _eventMode = eventMode;
}

void CustomStateMachine::_runStopHandler()
{
    if (_stopHandler) {
        auto handler = std::exchange(_stopHandler, nullptr);
        handler();
    }
}
