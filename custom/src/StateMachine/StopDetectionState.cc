#include "StopDetectionState.h"
#include "SendTunnelCommandState.h"
#include "TunnelProtocol.h"
#include "DetectorList.h"
#include "FunctionState.h"

#include <QFinalState>

using namespace TunnelProtocol;

StopDetectionState::StopDetectionState(QState* parentState)
    : CustomState("StopDetectionState", parentState)
{
    StopDetectionInfo_t stopDetectionInfo;

    stopDetectionInfo.header.command = COMMAND_ID_STOP_DETECTION;

    auto sendStopDetectionState = new SendTunnelCommandState("StopDetectionCommand", this, (uint8_t*)&stopDetectionInfo, sizeof(stopDetectionInfo));
    auto clearDetectorListState = new FunctionState("ClearDetectorList", this, std::bind(&StopDetectionState::_clearDetectorList, this));
    auto finalState             = new QFinalState(this);

    // Transitions
    sendStopDetectionState->addTransition(sendStopDetectionState, &SendTunnelCommandState::commandSucceeded, clearDetectorListState);
    clearDetectorListState->addTransition(clearDetectorListState, &FunctionState::functionCompleted, finalState);

    setInitialState(sendStopDetectionState);
}

void StopDetectionState::_clearDetectorList()
{
    DetectorList::instance()->clear();
}