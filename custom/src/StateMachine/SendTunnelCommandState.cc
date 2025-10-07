#include "SendTunnelCommandState.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"
#include "DetectorList.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "AudioOutput.h"

using namespace TunnelProtocol;

SendTunnelCommandState::SendTunnelCommandState(const QString& stateName, QState* parentState, uint8_t* payload, size_t payloadSize)
    : CustomState   (stateName, parentState)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _payload      (new uint8_t[payloadSize])
    , _payloadSize  (payloadSize)
{
    memcpy(_payload, payload, payloadSize);

    _ackResponseTimer.setSingleShot(true);
    _ackResponseTimer.setInterval(2000);

    connect(this, &QState::entered, this, &SendTunnelCommandState::_sendTunnelCommand);
    connect(this, &QState::exited, this, &SendTunnelCommandState::_disconnectAll);
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
        connect(&_ackResponseTimer, &QTimer::timeout, this, &SendTunnelCommandState::_ackResponseTimedOut);
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
        auto sentTunnelCommand = _sentTunnelCommand;

        _disconnectAll();

        qCDebug(CustomPluginLog) << "Tunnel command ack received - command:result" << commandIdToText(ack.command) << ack.result;
        if (ack.result == COMMAND_RESULT_SUCCESS) {
            emit commandSucceeded();
        } else {
            // Command failed
            if (sentTunnelCommand == COMMAND_ID_START_DETECTION) {
                // Special case for start detection failure, we want to clear the detector list
                DetectorList::instance()->clear();
            }
            if (ack.message[0] != 0) {
                // Failed with detailed message
                QString message = QStringLiteral("%1 failed. %2").arg(commandIdToText(sentTunnelCommand)).arg(ack.message);
                setError(message);
            } else {
                // Generic failure with no detailed message
                QString message = QStringLiteral("%1 failed. Bad command result: %2").arg(commandIdToText(sentTunnelCommand)).arg(_commandResultToString(ack.result));
                setError(message);
            }
        }
    } else {
        qWarning() << "SendTunnelCommandState::_handleTunnelCommandAck: Received unexpected command id ack expected:actual" <<
                      commandIdToText(_sentTunnelCommand) <<
                      commandIdToText(ack.command);
    }
}

void SendTunnelCommandState::_ackResponseTimedOut(void)
{
    QString message = QStringLiteral("%1 failed. No response from vehicle after %1 retries.").arg(commandIdToText(_sentTunnelCommand)).arg(_retryCount);

    _disconnectAll();

    if (_retryCount < _maxRetries) {
        qCDebug(CustomPluginLog) << message << "Retrying...";
        _retryCount++;
        _sendTunnelCommand();
    } else {
        qCWarning(CustomPluginLog) << message;
        setError(message);
    }
}

void SendTunnelCommandState::_disconnectAll()
{
    _ackResponseTimer.stop();
    _sentTunnelCommand = 0;
    disconnect(&_ackResponseTimer, &QTimer::timeout, this, &SendTunnelCommandState::_ackResponseTimedOut);
    disconnect(_vehicle, &Vehicle::mavlinkMessageReceived, this, &SendTunnelCommandState::_mavlinkMessageReceived);
}

QString SendTunnelCommandState::_commandResultToString(uint32_t result)
{
    switch (result) {
    case COMMAND_RESULT_SUCCESS:
        return QStringLiteral("Success");
    case COMMAND_RESULT_FAILURE:
        return QStringLiteral("Failure");
    default:
        return QStringLiteral("Unknown result: %1").arg(result);
    }
}
