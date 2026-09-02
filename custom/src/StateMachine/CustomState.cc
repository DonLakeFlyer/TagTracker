#include "CustomState.h"
#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

CustomState::CustomState(const QString& stateName, QState* parentState)
    : QGCState(stateName, parentState)
{
}

void CustomState::setError(const QString& errorString)
{
    CustomStateMachine* const stateMachine = customMachine();
    if (!stateMachine) {
        qCWarning(CustomStateMachineLog) << "setError with no CustomStateMachine" << objectName() << errorString;
        return;
    }

    stateMachine->setError(errorString);
}

CustomStateMachine* CustomState::customMachine() const
{
    return qobject_cast<CustomStateMachine*>(machine());
}
