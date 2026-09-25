#include "SendTagsState.h"
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

SendTagsState::SendTagsState(QState* parent)
    : CustomState("SendTagsState", parent)
{
    auto tagDatabase = TagDatabase::instance();

    // Do we have any selected tags?
    for (int i=0; i<tagDatabase->tagInfoListModel()->count(); i++) {
        TagInfo* tagInfo = tagDatabase->tagInfoListModel()->value<TagInfo*>(i);
        if (tagInfo->selected()->rawValue().toUInt()) {
            _tagCount++;
        }
    }
    if (_tagCount == 0) {
        qCWarning(CustomPluginLog) << "No tags are available/selected to send.";
        return;
    }
    _uploadId = SendTunnelCommandState::nextRequestId();

    auto sendLogLevel       = _sendLogLevelState(this);
    auto sendStartTags      = _sendStartTagsState(this);
    auto sendEndTags        = _sendEndTagsState(this);
    auto setupDetectorList  = new FunctionState("SetupDetectorList", this, std::bind(&SendTagsState::_setupDetectorList, this));
    auto finalState         = new QFinalState(this);

    // States for each Send tag
    QState* firstSendTagState = nullptr;
    auto tagInfoListModel = tagDatabase->tagInfoListModel();
    QList<SendTunnelCommandState*> sendTagStateList;
    for (int i=0; i<tagInfoListModel->count(); i++) {
        auto tagInfo = tagInfoListModel->value<TagInfo*>(i);

        if (!tagInfo->selected()->rawValue().toUInt()) {
            continue;
        }

        auto sendTagState = _sendTagState(i, static_cast<uint32_t>(sendTagStateList.count()), this);
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

    // Log level -> Send tags start
    sendLogLevel->addTransition(sendLogLevel, &SendTunnelCommandState::commandSucceeded, sendStartTags);

    // Send tags start -> first Send Tag
    sendStartTags->addTransition(sendStartTags, &SendTunnelCommandState::commandSucceeded, firstSendTagState);

    // Send tags end -> Setup detector list
    sendEndTags->addTransition(sendEndTags, &SendTunnelCommandState::commandSucceeded, setupDetectorList);

    // Setup detector list -> Final State
    setupDetectorList->addTransition(setupDetectorList, &QState::entered, finalState);

    this->setInitialState(sendLogLevel);
}

// A controller restart while idle is invisible to the heartbeat watchdog, so the
// level is re-asserted at the start of every session in case it fell back to default.
SendTunnelCommandState* SendTagsState::_sendLogLevelState(QState* parent)
{
    auto customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();

    SetLogLevel_t setLogLevel {};
    setLogLevel.header.command  = COMMAND_ID_SET_LOG_LEVEL;
    setLogLevel.level           = customSettings->controllerVerboseLogging()->rawValue().toBool() ? LOG_LEVEL_VERBOSE : LOG_LEVEL_DEBUG;

    return new SendTunnelCommandState("SetLogLevelCommand", parent, (uint8_t*)&setLogLevel, sizeof(setLogLevel));
}

SendTunnelCommandState* SendTagsState::_sendStartTagsState(QState* parent)
{
    StartTagsInfo_t startTagsInfo {};
    startTagsInfo.header.command  = COMMAND_ID_START_TAGS;
    startTagsInfo.upload_id       = _uploadId;
    startTagsInfo.tag_count       = _tagCount;

    return new SendTunnelCommandState("StartTagsCommand", parent, (uint8_t*)&startTagsInfo, sizeof(startTagsInfo));
}

SendTunnelCommandState* SendTagsState::_sendTagState(int tagIndex, uint32_t uploadIndex, QState* parent)
{
    auto tagDatabase = TagDatabase::instance();
    auto tagInfoListModel = tagDatabase->tagInfoListModel();
    auto customPlugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    auto customSettings = customPlugin->customSettings();
    auto tagInfo = tagInfoListModel->value<TagInfo*>(tagIndex);

    TunnelProtocol::TagInfo_t tunnelTagInfo;
    auto tagManufacturer = tagDatabase->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());

    memset(&tunnelTagInfo, 0, sizeof(tunnelTagInfo));

    tunnelTagInfo.header.command = COMMAND_ID_TAG;
    tunnelTagInfo.upload_id                                 = _uploadId;
    tunnelTagInfo.tag_index                                 = uploadIndex;
    tunnelTagInfo.id                                        = tagInfo->id()->rawValue().toUInt();
    tunnelTagInfo.frequency_hz                              = tagInfo->frequencyMHz()->rawValue().toDouble() * 1000000;
    tunnelTagInfo.pulse_width_msecs                         = tagManufacturer->pulse_width_msecs()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse1_msecs                        = tagManufacturer->ip_msecs_1()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse2_msecs                        = tagManufacturer->ip_msecs_2()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse_uncertainty_msecs             = tagManufacturer->ip_uncertainty_msecs()->rawValue().toUInt();
    tunnelTagInfo.intra_pulse_jitter_msecs                  = tagManufacturer->ip_jitter_msecs()->rawValue().toUInt();

    const bool isPythonMode = customPlugin->isPythonMode();

    tunnelTagInfo.k                                         = isPythonMode ? customSettings->pythonK()->rawValue().toUInt()
                                                                          : customSettings->k()->rawValue().toUInt();

    double falseAlarmProbability;
    if (isPythonMode) {
        switch (customSettings->pythonFalseAlarmMode()->rawValue().toUInt()) {
        case CustomSettings::Normal:
            falseAlarmProbability = CustomSettings::NormalPf;
            break;
        case CustomSettings::StressTest:
            falseAlarmProbability = CustomSettings::StressTestPf;
            break;
        case CustomSettings::Custom:
        default:
            falseAlarmProbability = customSettings->pythonFalseAlarmProbability()->rawValue().toDouble() / 100.0;
            break;
        }
    } else {
        falseAlarmProbability = customSettings->falseAlarmProbability()->rawValue().toDouble() / 100.0;
    }
    tunnelTagInfo.false_alarm_probability                   = falseAlarmProbability;
    tunnelTagInfo.channelizer_channel_number                = tagInfo->channelizer_channel_number;
    tunnelTagInfo.channelizer_channel_center_frequency_hz   = tagInfo->channelizer_channel_center_frequency_hz;
    tunnelTagInfo.ip1_mu                                    = qQNaN();
    tunnelTagInfo.ip1_sigma                                 = qQNaN();
    tunnelTagInfo.ip2_mu                                    = qQNaN();
    tunnelTagInfo.ip2_sigma                                 = qQNaN();

    return new SendTunnelCommandState("TagInfoCommand", parent, (uint8_t*)&tunnelTagInfo, sizeof(tunnelTagInfo));
}

SendTunnelCommandState* SendTagsState::_sendEndTagsState(QState* parent)
{
    EndTagsInfo_t endTagsInfo {};
    endTagsInfo.header.command = COMMAND_ID_END_TAGS;
    endTagsInfo.upload_id      = _uploadId;
    endTagsInfo.tag_count      = _tagCount;

    return new SendTunnelCommandState("EndTagsCommand", parent, (uint8_t*)&endTagsInfo, sizeof(endTagsInfo));
}

void SendTagsState::_setupDetectorList(void)
{
    DetectorList::instance()->setupFromSelectedTags();
}
