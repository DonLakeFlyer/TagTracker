#pragma once

#include "CustomStateMachine.h"

#include <QObject>
#include <QState>

class SendTunnelCommandState;

class SendTagsHandler : public QObject
{
    Q_OBJECT

public:
    SendTagsHandler(QObject* parent = nullptr);

    static SendTagsHandler* instance();

    CustomState* createSendTagsState(QState* parent);

signals:
    void _sendNextTagCommandSkippingUnselectedTag();
    void _sendNextTagCommandNoMoreTags();

private slots:
    void _setupDetectorList();

private:
    SendTunnelCommandState* _sendStartTagsState(QState* parent);
    SendTunnelCommandState* _sendEndTagsState(QState* parent);
    SendTunnelCommandState* _sendTagState(int tagIndex, QState* parent);

int _nextTagIndexToSend = 0;
};
