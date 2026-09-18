#pragma once

#include "QGCMAVLink.h"
#include "CustomState.h"

#include <QTimer>

class Vehicle;

class SendTunnelCommandState : public CustomState
{
    Q_OBJECT

public:
    static constexpr int kDefaultAckTimeoutMs = 2000;

    SendTunnelCommandState(const QString& stateName, QState* parentState, uint8_t* payload, size_t payloadSize, int ackTimeoutMs = kDefaultAckTimeoutMs);
    ~SendTunnelCommandState();

    static QString commandIdToText(uint32_t command);
    /// Fresh per new command; retries of that command reuse it so the
    /// controller can replay the original ACK. Randomly seeded per process so a
    /// restarted GCS cannot collide with ids the controller still remembers.
    static uint32_t nextRequestId();

signals:
    void commandSucceeded();

private slots:
    void _ackResponseTimedOut();
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _startCommand();
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
    uint32_t    _requestId = 0;
    int         _retryCount = 0;
    int         _maxRetries = 2;
};
