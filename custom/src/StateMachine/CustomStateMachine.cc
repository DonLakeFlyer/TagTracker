#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "GuidedModeCancelledTransition.h"
#include "ErrorTransition.h"
#include "SayState.h"

#include "QGCApplication.h"
#include "AudioOutput.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "AudioOutput.h"

#include <QFinalState>

CustomStateMachine::CustomStateMachine(const QString& machineName, QObject* parent)
    : QStateMachine (parent)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
{
    setObjectName(machineName);

    connect(this, &CustomStateMachine::started, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine started:" << objectName();
    });
    connect(this, &CustomStateMachine::stopped, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine finished:" << objectName();
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    });

    connect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    connect(this, &CustomStateMachine::stopped, this, [this] () { this->deleteLater(); });
}

void CustomStateMachine::setError(const QString& errorString)
{
    bool rtlOnError = _eventMode & RTLOnError;
    qWarning(CustomPluginLog) << "errorString" << errorString << " - " << Q_FUNC_INFO;
    _errorString = errorString;
    AudioOutput::instance()->say(QStringLiteral("%1 cancelled. %2").arg(objectName()).arg(rtlOnError ? "Returning" : "User is in control of vehicle"));
    if (rtlOnError) {
        _vehicle->setFlightMode(_vehicle->rtlFlightMode());
    }
    displayError();
    stop();
}

void CustomStateMachine::displayError()
{
    qCWarning(CustomPluginLog) << _errorString << " - " << Q_FUNC_INFO;
    qgcApp()->showAppMessage(_errorString);
    _errorString.clear();
}

void CustomStateMachine::_flightModeChanged(const QString& flightMode)
{
    QString holdFlightMode = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->holdFlightMode();

    if (_eventMode & CancelOnFlightModeChange && flightMode != holdFlightMode) {
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
        AudioOutput::instance()->say(QStringLiteral("%1 cancelled. User is in control of vehicle.").arg(objectName()));
        stop();
    }
}

void CustomStateMachine::setEventMode(uint eventMode) 
{ 
    if (eventMode == 0) {
        qCDebug(CustomPluginLog) << "Clearing all event modes"<< " - " << Q_FUNC_INFO;
    } else {
        if (eventMode & CancelOnFlightModeChange) {
            qCDebug(CustomPluginLog) << "Setting event mode: CancelOnFlightModeChange" << " - " << Q_FUNC_INFO;
        }
        if (eventMode & RTLOnError) {
            qCDebug(CustomPluginLog) << "Setting event mode: RTLOnError" << " - " << Q_FUNC_INFO;
        }
    }
    _eventMode = eventMode; 
}
