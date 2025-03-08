#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

CustomState::CustomState(const QString& stateName, QState* parentState, QState* errorState) 
    : QState(QState::ExclusiveStates, parentState)
{
    setObjectName(stateName);

    connect(this, &QState::entered, this, [this] () {
        qCDebug(CustomPluginLog) << "Entered state" << objectName() << " - " << Q_FUNC_INFO;
    });
    connect(this, &QState::exited, this, [this] () {
        qCDebug(CustomPluginLog) << "Exited state" << objectName() << " - " << Q_FUNC_INFO;
    });

    if (errorState) {
        addTransition(this, &CustomState::error, errorState);
    }
}

void CustomState::setError(const QString& errorString) 
{
    qWarning(CustomPluginLog) << "errorString" << errorString << " - " << Q_FUNC_INFO;

    _errorString = errorString; 

    // Bubble the error up to the top level state machine
    auto customStateMachine = qobject_cast<CustomStateMachine*>(machine());
    if (customStateMachine) {
        customStateMachine->_errorString = errorString;
    }

    emit error(); 
}

CustomStateMachine* CustomState::machine() 
{ 
    return qobject_cast<CustomStateMachine*>(QState::machine()); 
}
