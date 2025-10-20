#include "StopDetectionState.h"
#include "SendTunnelCommandState.h"
#include "TunnelProtocol.h"
#include "DetectorList.h"
#include "FunctionState.h"
#include "CustomPlugin.h"

#include <QFinalState>

using namespace TunnelProtocol;

StopDetectionState::StopDetectionState(QState* parentState)
    : CustomState("StopDetectionState", parentState)
{
    StopDetectionInfo_t stopDetectionInfo;

    stopDetectionInfo.header.command = COMMAND_ID_STOP_DETECTION;

    auto stopPulseLoggingState  = new FunctionState("StopPulseLogging", this, std::bind(&StopDetectionState::_stopPulseLogging, this));
    auto sendStopDetectionState = new SendTunnelCommandState("StopDetectionCommand", this, (uint8_t*)&stopDetectionInfo, sizeof(stopDetectionInfo));
    auto clearDetectorListState = new FunctionState("ClearDetectorList", this, std::bind(&StopDetectionState::_clearDetectorList, this));
    auto finalState             = new QFinalState(this);

    // Transitions
    stopPulseLoggingState->addTransition(stopPulseLoggingState, &FunctionState::functionCompleted, sendStopDetectionState);
    sendStopDetectionState->addTransition(sendStopDetectionState, &SendTunnelCommandState::commandSucceeded, clearDetectorListState);
    clearDetectorListState->addTransition(clearDetectorListState, &FunctionState::functionCompleted, finalState);

    setInitialState(stopPulseLoggingState);
}

void StopDetectionState::_clearDetectorList()
{
    DetectorList::instance()->clear();
}

void StopDetectionState::_stopPulseLogging()
{
    qobject_cast<CustomPlugin*>(CustomPlugin::instance())->csvLogManager().csvStopFullPulseLog();
}