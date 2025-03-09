#include "CustomStateMachine.h"
#include "CustomLoggingCategory.h"

CustomState::CustomState(const QString& stateName, QState* parentState) 
    : QState(QState::ExclusiveStates, parentState)
{
    setObjectName(stateName);

    connect(this, &QState::entered, this, [this] () {
        qCDebug(CustomPluginLog) << "Entered state" << objectName() << " - " << Q_FUNC_INFO;
    });
    connect(this, &QState::exited, this, [this] () {
        qCDebug(CustomPluginLog) << "Exited state" << objectName() << " - " << Q_FUNC_INFO;
    });
}

void CustomState::setError(const QString& errorString) 
{
    qWarning(CustomPluginLog) << "errorString" << errorString << " - " << Q_FUNC_INFO;
    machine()->setError(errorString);
}

CustomStateMachine* CustomState::machine() 
{ 
    return qobject_cast<CustomStateMachine*>(QState::machine()); 
}
