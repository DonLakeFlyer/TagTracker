#include "PythonRotateAndCaptureState.h"
#include "PythonCaptureAtSliceState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"

#include <QFinalState>

PythonRotateAndCaptureState::PythonRotateAndCaptureState(QState* parentState)
    : CustomState       ("PythonRotateAndCaptureState", parentState)
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
{
    const int rotationDivisions     = _customSettings->divisions()->rawValue().toInt();

    auto rotationBeginState         = new FunctionState("Rotation Begin", this, std::bind(&PythonRotateAndCaptureState::_rotationBegin, this));
    auto rotationEndState           = new FunctionState("Rotation End",   this, std::bind(&PythonRotateAndCaptureState::_rotationEnd, this));
    auto announceRotateCompleteState= new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                 = new QFinalState(this);

    // Build the per-slice states
    QList<PythonCaptureAtSliceState*> sliceStates;
    for (int i = 0; i < rotationDivisions; i++) {
        sliceStates.append(new PythonCaptureAtSliceState(this, i));
    }

    // Transitions: rotationBegin → slice[0] → slice[1] → ... → slice[N-1] → rotationEnd
    rotationBeginState->addTransition(rotationBeginState, &FunctionState::functionCompleted, sliceStates.first());

    for (int i = 0; i < sliceStates.count() - 1; i++) {
        sliceStates[i]->addTransition(sliceStates[i], &QState::finished, sliceStates[i + 1]);
    }
    sliceStates.last()->addTransition(sliceStates.last(), &QState::finished, rotationEndState);

    rotationEndState->addTransition(rotationEndState, &FunctionState::functionCompleted, announceRotateCompleteState);
    announceRotateCompleteState->addTransition(announceRotateCompleteState, &SayState::functionCompleted, finalState);

    setInitialState(rotationBeginState);
}

void PythonRotateAndCaptureState::_rotationBegin()
{
    _customPlugin->rotationIsStarting();
    qCDebug(CustomStateMachineLog) << "Python rotation begin" << " - " << Q_FUNC_INFO;
}

void PythonRotateAndCaptureState::_rotationEnd()
{
    _customPlugin->rotationIsEnding();
}
