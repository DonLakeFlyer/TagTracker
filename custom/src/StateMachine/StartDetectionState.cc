#include "StartDetectionState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"
#include "FunctionState.h"
#include "SendTagsState.h"
#include "SendTunnelCommandState.h"
#include "TagDatabase.h"
#include "CustomPlugin.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"

#include <QFinalState>

using namespace TunnelProtocol;

StartDetectionState::StartDetectionState(QState* parentState)
    : CustomState("StartDetectionState", parentState)
{
    StartDetectionInfo_t startDetectionInfo;

    memset(&startDetectionInfo, 0, sizeof(startDetectionInfo));

    startDetectionInfo.header.command               = COMMAND_ID_START_DETECTION;
    startDetectionInfo.radio_center_frequency_hz    = TagDatabase::instance()->channelizerTuner();
    startDetectionInfo.gain                         = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings()->gain()->rawValue().toUInt();

    if (startDetectionInfo.radio_center_frequency_hz == 0) {
        _channelizerTunerFailed = true;
    }

    auto finalState                 = new QFinalState(this);
    auto checkTunerState            = new FunctionState("CheckTuner", this, std::bind(&StartDetectionState::_checkTuner, this));
    auto sendTagsState              = new SendTagsState(this);
    auto sendStartDetectionState    = new SendTunnelCommandState("StartDetectionCommand", this, (uint8_t*)&startDetectionInfo, sizeof(startDetectionInfo));

    // Transitions
    checkTunerState->addTransition(this, &StartDetectionState::tunerSucceeded, sendTagsState);
    sendTagsState->addTransition(sendTagsState, &QState::finished, sendStartDetectionState);
    sendStartDetectionState->addTransition(sendStartDetectionState, &SendTunnelCommandState::commandSucceeded, finalState);

    setInitialState(checkTunerState);
}

void StartDetectionState::_checkTuner()
{
    if (_channelizerTunerFailed) {
        setError("Channelizer tuner failed");
    } else {
        emit tunerSucceeded();
    }
}

