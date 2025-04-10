#include "FullRotateAndCaptureState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "SliceSequenceCaptureState.h"

FullRotateAndCaptureState::FullRotateAndCaptureState(QState* parentState)
    : RotateAndCaptureStateBase("FullRotateAndCaptureState", parentState)
{
    // States
    auto sliceSequenceState = new SliceSequenceCaptureState(
                                        "Full Rotation Slice Sequence",
                                        this,                       // parentState
                                        0,                          // firstSlice
                                        0,                          // skipCount
                                        true,                       // clockwiseDirection
                                        _rotationDivisions);        // sliceCount

    // Transitions
    _rotationBeginState->addTransition(_rotationBeginState, &FunctionState::functionCompleted, sliceSequenceState);
    sliceSequenceState->addTransition(sliceSequenceState, &QState::finished, _rotationEndState);
}
