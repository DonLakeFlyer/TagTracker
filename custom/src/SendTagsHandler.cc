#include "SendTagsHandler.h"
#include "CustomLoggingCategory.h"
#include "SendTunnelCommandState.h"
#include "TagDatabase.h"
#include "TunnelProtocol.h"
#include "CustomPlugin.h"
#include "DetectorList.h"
#include "CustomStateMachine.h"

#include "QGCApplication.h"

#include <QFinalState>

using namespace TunnelProtocol;

Q_APPLICATION_STATIC(SendTagsHandler, _sendTagsHandlerInstance);

SendTagsHandler::SendTagsHandler(QObject* parent)
    : QObject(parent)
{

}

SendTagsHandler* SendTagsHandler::instance()
{
    return _sendTagsHandlerInstance();
}

CustomState* SendTagsHandler::createSendTagsState(QState* parent)
{
    auto tagDatabase = TagDatabase::instance();

    // Do we have any selected tags?
    bool foundSelectedTag = false;
    for (int i=0; i<tagDatabase->tagInfoListModel()->count(); i++) {
        TagInfo* tagInfo = tagDatabase->tagInfoListModel()->value<TagInfo*>(i);
        if (tagInfo->selected()->rawValue().toUInt()) {
            foundSelectedTag = true;
            break;
        }
    }
    if (!foundSelectedTag) {
        qgcApp()->showAppMessage(("No tags are available/selected to send."));
        return nullptr;
    }

    if (!tagDatabase->channelizerTuner()) {
        qgcApp()->showAppMessage("Channelizer tuner failed. Detectors not started");
        return nullptr;
    }

    auto stateGroup         = new CustomState(parent);
    auto sendStartTags      = _sendStartTagsState(stateGroup);
    auto sendEndTags        = _sendEndTagsState(stateGroup);
    auto setupDetectorList  = new QState(stateGroup);
    auto errorState         = new QState(stateGroup);
    auto finalState         = new QFinalState(stateGroup);

    // States for each Send tag
    QState* firstSendTagState = nullptr;
    auto tagInfoListModel = tagDatabase->tagInfoListModel();
    QList<SendTunnelCommandState*> sendTagStateList;
    for (int i=0; i<tagInfoListModel->count(); i++) {
        auto tagInfo = tagInfoListModel->value<TagInfo*>(i);

        if (!tagInfo->selected()->rawValue().toUInt()) {
            continue;
        }

        auto sendTagState = _sendTagState(i, stateGroup);
        if (!firstSendTagState) {
            firstSendTagState = sendTagState;
        }
        sendTagStateList.append(sendTagState);
    }

    for (int i=0; i<sendTagStateList.count(); i++) {
        auto sendTagState = sendTagStateList[i];

        if (i == sendTagStateList.count() - 1) {
            sendTagState->addTransition(sendTagState, &SendTunnelCommandState::commandSucceeded, sendEndTags);
        } else {
            sendTagState->addTransition(sendTagState, &SendTunnelCommandState::commandSucceeded, sendTagStateList[i + 1]);
        }
    }

    // Send tags start -> first Send Tag
    sendStartTags->addTransition(sendStartTags, &SendTunnelCommandState::commandSucceeded, firstSendTagState);

    // Send tags end -> Setup detector list
    sendEndTags->addTransition(sendEndTags, &SendTunnelCommandState::commandSucceeded, setupDetectorList);

    // Setup detector list -> Final State
    connect(setupDetectorList, &QState::entered, this, &SendTagsHandler::_setupDetectorList);
    setupDetectorList->addTransition(setupDetectorList, &QState::entered, finalState);

    // Error handling
    connect(errorState, &QState::exited, stateGroup, [stateGroup]() {
        stateGroup->setError("SendTagsHandler error: command failed");
    });
    errorState->addTransition(errorState, &QState::entered, finalState);

    stateGroup->setInitialState(sendStartTags);

    return stateGroup;
}

SendTunnelCommandState* SendTagsHandler::_sendStartTagsState(QState* parent)
{
    StartTagsInfo_t startTagsInfo;
    startTagsInfo.header.command  = COMMAND_ID_START_TAGS;

    return new SendTunnelCommandState((uint8_t*)&startTagsInfo, sizeof(startTagsInfo), parent);
}

SendTunnelCommandState* SendTagsHandler::_sendTagState(int tagIndex, QState* parent)
{
    auto tagDatabase = TagDatabase::instance();
    auto tagInfoListModel = tagDatabase->tagInfoListModel();
    auto customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
    auto tagInfo = tagInfoListModel->value<TagInfo*>(tagIndex);

    TunnelProtocol::TagInfo_t tunnelTagInfo;
    auto tagManufacturer = tagDatabase->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());

    memset(&tunnelTagInfo, 0, sizeof(tunnelTagInfo));

    tunnelTagInfo.header.command = COMMAND_ID_TAG;
    tunnelTagInfo.id                                        = tagInfo->id()->rawValue().toUInt();
    tunnelTagInfo.frequency_hz                              = tagInfo->frequencyHz()->rawValue().toUInt();
    tunnelTagInfo.pulse_width_msecs                         = tagManufacturer->pulse_width_msecs()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse1_msecs                        = tagManufacturer->ip_msecs_1()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse2_msecs                        = tagManufacturer->ip_msecs_2()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse_uncertainty_msecs             = tagManufacturer->ip_uncertainty_msecs()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse_jitter_msecs                  = tagManufacturer->ip_jitter_msecs()->rawValue().toUInt();
    tunnelTagInfo.k                                         = customSettings->k()->rawValue().toUInt();
    tunnelTagInfo.false_alarm_probability                   = customSettings->falseAlarmProbability()->rawValue().toDouble() / 100.0;
    tunnelTagInfo.channelizer_channel_number                = tagInfo->channelizer_channel_number;
    tunnelTagInfo.channelizer_channel_center_frequency_hz   = tagInfo->channelizer_channel_center_frequency_hz;
    tunnelTagInfo.ip1_mu                                    = qQNaN();
    tunnelTagInfo.ip1_sigma                                 = qQNaN();
    tunnelTagInfo.ip2_mu                                    = qQNaN();
    tunnelTagInfo.ip2_sigma                                 = qQNaN();

    return new SendTunnelCommandState((uint8_t*)&tunnelTagInfo, sizeof(tunnelTagInfo), parent);
}

SendTunnelCommandState* SendTagsHandler::_sendEndTagsState(QState* parent)
{
    EndTagsInfo_t endTagsInfo;
    endTagsInfo.header.command = COMMAND_ID_END_TAGS;

    return new SendTunnelCommandState((uint8_t*)&endTagsInfo, sizeof(endTagsInfo), parent);
}

void SendTagsHandler::_setupDetectorList(void)
{
    DetectorList::instance()->setupFromSelectedTags();
}