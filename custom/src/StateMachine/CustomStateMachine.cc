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

#include <QFinalState>

CustomStateMachine::CustomStateMachine(const QString& machineName, QObject* parent)
    : QStateMachine (parent)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _errorState   (new FunctionState("DisplayError", this, std::bind(&CustomStateMachine::displayError, this)))
    , _finalState   (new QFinalState(this))
{
    setObjectName(machineName);

    connect(this, &CustomStateMachine::started, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine started:" << objectName();
    });
    connect(this, &CustomStateMachine::stopped, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine finished:" << objectName();
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    });

    // Error handling
    auto errorTransition = new ErrorTransition(this);
    errorTransition->setTargetState(_errorState);
    _errorState->addTransition(_errorState, &FunctionState::functionCompleted, _finalState);

    connect(this, &CustomStateMachine::stopped, this, [this] () { this->deleteLater(); });
}

void CustomStateMachine::addGuidedModeCancelledTransition()
{
    _guidedModeCancelledTransition  = new GuidedModeCancelledTransition(this);
    _announceCancelState            = new SayState("AnnounceCancel", this, "Auto Detection cancelled. User is in control of vehicle");

    _guidedModeCancelledTransition->setTargetState(_announceCancelState);
    _announceCancelState->addTransition(_announceCancelState, &FunctionState::functionCompleted, _finalState);

    connect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
}

void CustomStateMachine::removeGuidedModeCancelledTransition()
{
    this->removeTransition(_guidedModeCancelledTransition);
    this->removeState(_announceCancelState);

    _guidedModeCancelledTransition->deleteLater();
    _announceCancelState->deleteLater();
    _guidedModeCancelledTransition = nullptr;
    _announceCancelState = nullptr;

    disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
}

void CustomStateMachine::setError(const QString& errorString)
{
    _errorString = errorString;
    postEvent(new ErrorEvent(errorString));
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

    if (flightMode != holdFlightMode) {
        qCDebug(CustomPluginLog) << "Flight mode changed to" << flightMode << ". Posting GuidedModeCancelledEvent" << " - " << Q_FUNC_INFO;
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
        postEvent(new GuidedModeCancelledEvent());
    }
}

