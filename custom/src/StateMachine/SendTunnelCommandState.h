#pragma once

#include "QGCMAVLink.h"
#include "CustomState.h"

#include <QTimer>

class Vehicle;

class SendTunnelCommandState : public CustomState
{
    Q_OBJECT

public:
    SendTunnelCommandState(const QString& stateName, QState* parentState, uint8_t* payload, size_t payloadSize);
    ~SendTunnelCommandState();

    static QString commandIdToText(uint32_t command);

signals:
    void commandSucceeded();

private slots:
    void _ackResponseTimedOut();
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _sendTunnelCommand();
    void _disconnectAll();
    QString _commandResultToString(uint32_t result);

private:
    void _handleTunnelMessage (const mavlink_tunnel_t& tunnel);
    void _handleTunnelCommandAck(const mavlink_tunnel_t& tunnel);

    Vehicle*    _vehicle = nullptr;
    uint8_t*    _payload = nullptr;
    size_t      _payloadSize = 0;
    QTimer      _ackResponseTimer;
    uint32_t    _sentTunnelCommand = 0;
    int         _retryCount = 0;
    int         _maxRetries = 2;
};
