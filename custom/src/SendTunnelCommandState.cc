#include "SendTunnelCommandState.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "AudioOutput.h"

using namespace TunnelProtocol;

SendTunnelCommandState::SendTunnelCommandState(uint8_t* payload, size_t payloadSize, QState* parent)
    : CustomState   (parent)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _payload      (new uint8_t[payloadSize])
    , _payloadSize  (payloadSize)
{
    memcpy(_payload, payload, payloadSize);

    _ackResponseTimer.setSingleShot(true);
    _ackResponseTimer.setInterval(2000);

    connect(this, &QState::entered, this, &SendTunnelCommandState::_sendTunnelCommand);

    connect(&_ackResponseTimer, &QTimer::timeout, this, &SendTunnelCommandState::_ackResponseTimedOut);
}

SendTunnelCommandState::~SendTunnelCommandState()
{
    delete[] _payload;
}

void SendTunnelCommandState::_mavlinkMessageReceived(const mavlink_message_t& message)
{
    if (message.msgid == MAVLINK_MSG_ID_TUNNEL) {
        mavlink_tunnel_t tunnel;

        mavlink_msg_tunnel_decode(&message, &tunnel);

        HeaderInfo_t header;
        memcpy(&header, tunnel.payload, sizeof(header));

        if (header.command == COMMAND_ID_ACK) {
            _handleTunnelCommandAck(tunnel);
        }
    }
}

QString SendTunnelCommandState::commandIdToText(uint32_t vhfCommandId)
{
    switch (vhfCommandId) {
    case COMMAND_ID_TAG:
        return QStringLiteral("Send Tag");
    case COMMAND_ID_START_TAGS:
        return QStringLiteral("Start Tag Send");
    case COMMAND_ID_END_TAGS:
        return QStringLiteral("End Tag Send");
    case COMMAND_ID_PULSE:
        return QStringLiteral("Pulse");
    case COMMAND_ID_RAW_CAPTURE:
        return QStringLiteral("Raw Capture");
    case COMMAND_ID_START_DETECTION:
        return QStringLiteral("Start Detection");
    case COMMAND_ID_STOP_DETECTION:
        return QStringLiteral("Stop Detection");
    case COMMAND_ID_SAVE_LOGS:
        return QStringLiteral("Save Logs");
    case COMMAND_ID_CLEAN_LOGS:
        return QStringLiteral("Clean Logs");
    default:
        return QStringLiteral("Unknown command: %1").arg(vhfCommandId);
    }
}

void SendTunnelCommandState::_sendTunnelCommand()
{
    WeakLinkInterfacePtr weakPrimaryLink = _vehicle->vehicleLinkManager()->primaryLink();

    if (!weakPrimaryLink.expired()) {
        connect(_vehicle, &Vehicle::mavlinkMessageReceived, this, &SendTunnelCommandState::_mavlinkMessageReceived);

        SharedLinkInterfacePtr  sharedLink  = weakPrimaryLink.lock();
        MAVLinkProtocol*        mavlink     = MAVLinkProtocol::instance();
        mavlink_message_t       msg;
        mavlink_tunnel_t        tunnel;

        memset(&tunnel, 0, sizeof(tunnel));

        HeaderInfo_t tunnelHeader;
        memcpy(&tunnelHeader, _payload, sizeof(tunnelHeader));

        _sentTunnelCommand = tunnelHeader.command;
        _ackResponseTimer.start();

        memcpy(tunnel.payload, _payload, _payloadSize);

        tunnel.target_system    = _vehicle->id();
        tunnel.target_component = MAV_COMP_ID_ONBOARD_COMPUTER;
        tunnel.payload_type     = MAV_TUNNEL_PAYLOAD_TYPE_UNKNOWN;
        tunnel.payload_length   = _payloadSize;

        mavlink_msg_tunnel_encode_chan(
                    static_cast<uint8_t>(mavlink->getSystemId()),
                    static_cast<uint8_t>(mavlink->getComponentId()),
                    sharedLink->mavlinkChannel(),
                    &msg,
                    &tunnel);

        qCDebug(CustomPluginLog) << "SendTunnelCommandState::_sendTunnelCommand: Sending tunnel command - " << commandIdToText(_sentTunnelCommand);

        _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
}

void SendTunnelCommandState::_handleTunnelCommandAck(const mavlink_tunnel_t& tunnel)
{
    AckInfo_t ack;

    memcpy(&ack, tunnel.payload, sizeof(ack));

    if (ack.command == _sentTunnelCommand) {
        disconnect(_vehicle, &Vehicle::mavlinkMessageReceived, this, &SendTunnelCommandState::_mavlinkMessageReceived);

        _sentTunnelCommand = 0;
        _ackResponseTimer.stop();

        qCDebug(CustomPluginLog) << "Tunnel command ack received - command:result" << commandIdToText(ack.command) << ack.result;
        if (ack.result == COMMAND_RESULT_SUCCESS) {
            emit commandSucceeded();
        } else {
            QString message = QStringLiteral("%1 failed. Bad command result: %2").arg(commandIdToText(_sentTunnelCommand).arg(ack.result));
            setError(message);
        }
    } else {
        qWarning() << "SendTunnelCommandState::_handleTunnelCommandAck: Received unexpected command id ack expected:actual" <<
                      commandIdToText(_sentTunnelCommand) <<
                      commandIdToText(ack.command);
    }
}

void SendTunnelCommandState::_ackResponseTimedOut(void)
{
    _sentTunnelCommand = 0;

    QString message = QStringLiteral("%1 failed. No response from vehicle.").arg(commandIdToText(_sentTunnelCommand));
    setError(message);
}

