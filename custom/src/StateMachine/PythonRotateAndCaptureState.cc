#include "PythonRotateAndCaptureState.h"
#include "PythonCaptureAtSliceState.h"
#include "SendTunnelCommandState.h"
#include "FunctionState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"
#include "TagDatabase.h"
#include "TunnelProtocol.h"

#include <QFinalState>

using namespace TunnelProtocol;

PythonRotateAndCaptureState::PythonRotateAndCaptureState(QState* parentState)
    : CustomState       ("PythonRotateAndCaptureState", parentState)
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
{
    const int rotationDivisions     = _customSettings->divisions()->rawValue().toInt();

    // Build StartRotationDetection command
    StartRotationDetection_t startRotationDetection;
    memset(&startRotationDetection, 0, sizeof(startRotationDetection));
    startRotationDetection.header.command            = COMMAND_ID_START_ROTATION_DETECTION;
    startRotationDetection.radio_center_frequency_hz = TagDatabase::instance()->channelizerTuner();
    startRotationDetection.n_slices                  = rotationDivisions;
    startRotationDetection.detection_margin          = _customSettings->detectionMargin()->rawValue().toDouble();
    startRotationDetection.confidence_ratio          = _customSettings->confidenceRatio()->rawValue().toDouble();

    // Build StopRotationDetection command
    StopRotationDetection_t stopRotationDetection;
    memset(&stopRotationDetection, 0, sizeof(stopRotationDetection));
    stopRotationDetection.header.command = COMMAND_ID_STOP_ROTATION_DETECTION;

    auto rotationBeginState             = new FunctionState("Rotation Begin", this, std::bind(&PythonRotateAndCaptureState::_rotationBegin, this));
    auto startRotationDetectionState    = new SendTunnelCommandState("StartRotationDetection", this, reinterpret_cast<uint8_t*>(&startRotationDetection), sizeof(startRotationDetection));
    auto stopRotationDetectionState     = new SendTunnelCommandState("StopRotationDetection", this, reinterpret_cast<uint8_t*>(&stopRotationDetection), sizeof(stopRotationDetection));
    auto rotationEndState               = new FunctionState("Rotation End",   this, std::bind(&PythonRotateAndCaptureState::_rotationEnd, this));
    auto announceRotateCompleteState    = new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                     = new QFinalState(this);

    // Build the per-slice states
    QList<PythonCaptureAtSliceState*> sliceStates;
    for (int i = 0; i < rotationDivisions; i++) {
        sliceStates.append(new PythonCaptureAtSliceState(this, i));
    }

    // Transitions: rotationBegin → startRotationDetection → slice[0] → ... → slice[N-1] → stopRotationDetection → rotationEnd
    rotationBeginState->addTransition(rotationBeginState, &FunctionState::functionCompleted, startRotationDetectionState);
    startRotationDetectionState->addTransition(startRotationDetectionState, &SendTunnelCommandState::commandSucceeded, sliceStates.first());

    for (int i = 0; i < sliceStates.count() - 1; i++) {
        sliceStates[i]->addTransition(sliceStates[i], &QState::finished, sliceStates[i + 1]);
    }
    sliceStates.last()->addTransition(sliceStates.last(), &QState::finished, stopRotationDetectionState);

    stopRotationDetectionState->addTransition(stopRotationDetectionState, &SendTunnelCommandState::commandSucceeded, rotationEndState);
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
