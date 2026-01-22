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

    auto checkForSelectedTagsState  = new FunctionState("CheckForSelectedTags", this, std::bind(&StartDetectionState::_checkForSelectedTags, this));
    auto checkTunerState            = new FunctionState("CheckTuner", this, std::bind(&StartDetectionState::_checkTuner, this));
    auto sendTagsState              = new SendTagsState(this);
    auto sendStartDetectionState    = new SendTunnelCommandState("StartDetectionCommand", this, (uint8_t*)&startDetectionInfo, sizeof(startDetectionInfo));
    auto startPulseLoggingState     = new FunctionState("StartPulseLogging", this, std::bind(&StartDetectionState::_startPulseLogging, this));
    auto finalState                 = new QFinalState(this);

    // Transitions
    checkForSelectedTagsState->addTransition(this, &StartDetectionState::checkForSelectedTagsSucceeded, checkTunerState);
    checkTunerState->addTransition(this, &StartDetectionState::tunerSucceeded, sendTagsState);
    sendTagsState->addTransition(sendTagsState, &QState::finished, sendStartDetectionState);
    sendStartDetectionState->addTransition(sendStartDetectionState, &SendTunnelCommandState::commandSucceeded, startPulseLoggingState);
    startPulseLoggingState->addTransition(startPulseLoggingState, &FunctionState::functionCompleted, finalState);

    setInitialState(checkForSelectedTagsState);
}

void StartDetectionState::_checkTuner()
{
    if (_channelizerTunerFailed) {
        setError("Channelizer tuner failed");
    } else {
        emit tunerSucceeded();
    }
}

void StartDetectionState::_checkForSelectedTags()
{
    TagDatabase* tagDatabase = TagDatabase::instance();

    for (int i=0; i<tagDatabase->tagInfoListModel()->count(); i++) {
        TagInfo* tagInfo = tagDatabase->tagInfoListModel()->value<TagInfo*>(i);

        if (tagInfo->selected()->rawValue().toUInt()) {
            emit checkForSelectedTagsSucceeded();
            return;
        }
    }

    setError("No tags selected for detection");
}

void StartDetectionState::_startPulseLogging()
{
    qobject_cast<CustomPlugin*>(CustomPlugin::instance())->csvLogManager().csvStartFullPulseLog();
}
