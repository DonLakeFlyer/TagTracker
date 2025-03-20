#include "FullRotateAndCaptureState.h"
#include "FunctionState.h"
#include "SayState.h"

FullRotateAndCaptureState::FullRotateAndCaptureState(QState* parentState)
    : RotateAndCaptureStateBase("FullRotateAndCaptureState", parentState)
{
    // Transitions
    _initRotationState->addTransition(_initRotationState, &FunctionState::functionCompleted, _rotationStates.first());
    for (int i=0; i<_rotationStates.count(); i++) {
        if (i == _rotationStates.count() - 1) {
            _rotationStates[i]->addTransition(_rotationStates[i], &QState::finished, _announceRotateCompleteState);
        } else {
            _rotationStates[i]->addTransition(_rotationStates[i], &QState::finished, _rotationStates[i+1]);
        }
    }
}
