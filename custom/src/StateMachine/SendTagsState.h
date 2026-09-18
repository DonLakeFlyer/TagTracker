#pragma once

#include "CustomState.h"

class SendTunnelCommandState;

class SendTagsState : public CustomState
{
    Q_OBJECT

public:
    SendTagsState(QState* parent);

private:
    SendTunnelCommandState* _sendStartTagsState(QState* parent);
    SendTunnelCommandState* _sendEndTagsState(QState* parent);
    SendTunnelCommandState* _sendTagState(int tagIndex, uint32_t uploadIndex, QState* parent);
    void _setupDetectorList();

    uint32_t _uploadId = 0;     // identifies this START_TAGS..END_TAGS set to the controller
    uint32_t _tagCount = 0;     // selected tags in the set
};
