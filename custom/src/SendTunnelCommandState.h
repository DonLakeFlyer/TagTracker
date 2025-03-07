#pragma once

#include "QGCMAVLink.h"
#include "CustomStateMachine.h"

#include <QTimer>

class Vehicle;

class SendTunnelCommandState : public FunctionState
{
    Q_OBJECT

public:
    SendTunnelCommandState(uint8_t* payload, size_t payloadSize, QState* parent = nullptr);
    ~SendTunnelCommandState();

    static QString commandIdToText(uint32_t command);

signals:
    void commandSucceeded();

private slots:
    void _ackResponseTimedOut();
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _sendTunnelCommand();

private:
    void _handleTunnelMessage (const mavlink_tunnel_t& tunnel);
    void _handleTunnelCommandAck(const mavlink_tunnel_t& tunnel);

    Vehicle*    _vehicle = nullptr;
    uint8_t*    _payload = nullptr;
    size_t      _payloadSize = 0;
    QTimer      _ackResponseTimer;
    uint32_t    _sentTunnelCommand = 0;
};
