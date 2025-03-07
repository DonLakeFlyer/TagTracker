#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

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
