#pragma once

#include "CustomStateMachine.h"

class SendTunnelCommandState;

class SendTagsState : public CustomState
{
    Q_OBJECT

public:
    SendTagsState(QState* parent);

private:
    SendTunnelCommandState* _sendStartTagsState(QState* parent);
    SendTunnelCommandState* _sendEndTagsState(QState* parent);
    SendTunnelCommandState* _sendTagState(int tagIndex, QState* parent);
    void _setupDetectorList();

    int _nextTagIndexToSend = 0;
};
