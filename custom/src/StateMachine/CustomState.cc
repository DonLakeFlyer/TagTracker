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
        qCWarning(CustomPluginLog) << "setError with no CustomStateMachine state:" << objectName()
                                   << "error:" << errorString;
        return;
    }

    stateMachine->setError(errorString);
}

CustomStateMachine* CustomState::customMachine() const
{
    return qobject_cast<CustomStateMachine*>(machine());
}
