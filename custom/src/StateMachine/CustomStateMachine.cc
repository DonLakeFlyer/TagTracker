#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "GuidedModeCancelledTransition.h"

#include "QGCApplication.h"
#include "AudioOutput.h"
#include "MultiVehicleManager.h"
#include "Vehicle.h"

#include <QFinalState>

CustomStateMachine::CustomStateMachine(const QString& machineName, QObject* parent)
    : QStateMachine (parent)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _errorState   (new FunctionState(std::bind(&CustomStateMachine::displayError, this), this))
    , _finalState   (new QFinalState(this))
{
    setObjectName(machineName);

    connect(this, &CustomStateMachine::started, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine started:" << objectName();
        connect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    });
    connect(this, &CustomStateMachine::stopped, this, [this] () {
        qCDebug(CustomPluginLog) << "State machine finished:" << objectName();
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &CustomStateMachine::_flightModeChanged);
    });
    connect(this, &CustomStateMachine::error, this, [this] () {
        qCWarning(CustomPluginLog) << "State machine error:" << _errorString << "on" << objectName() ;
    });

    auto cancelledTransition = new GuidedModeCancelledTransition(this);
    cancelledTransition->setTargetState(_finalState);

    _errorState->addTransition(_errorState, &QState::entered, _finalState);

    connect(this, &CustomStateMachine::stopped, this, [this] () { this->deleteLater(); });
}

void CustomStateMachine::setError(const QString& errorString)
{
    _errorString = errorString;
    emit error();
}

void CustomStateMachine::displayError()
{
    qDebug() << "CustomStateMachine::displayError: errorString" << _errorString;
    AudioOutput::instance()->say(_errorString);
    qgcApp()->showAppMessage(_errorString);
    qCWarning(CustomPluginLog) << _errorString;
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

