#include "SendTunnelCommandState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"
#include "DetectorList.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "AudioOutput.h"

using namespace TunnelProtocol;

SendTunnelCommandState::SendTunnelCommandState(const QString& stateName, QState* parentState, uint8_t* payload, size_t payloadSize, int ackTimeoutMs)
    : CustomState   (stateName, parentState)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _payload      (new uint8_t[payloadSize])
    , _payloadSize  (payloadSize)
{
    memcpy(_payload, payload, payloadSize);

    _ackResponseTimer.setSingleShot(true);
    _ackResponseTimer.setInterval(ackTimeoutMs);

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
        return QStringLiteral("Save Logs (SD Card)");
    case COMMAND_ID_CLEAN_LOGS:
        return QStringLiteral("Clean Logs");
    case COMMAND_ID_AIRSPY_STATUS:
        return QStringLiteral("Airspy Status");
    case COMMAND_ID_START_COLLECTION:
        return QStringLiteral("Start Collection");
    case COMMAND_ID_START_COLLECTION_SLICE:
        return QStringLiteral("Start Collection Slice");
    case COMMAND_ID_FINISH_COLLECTION:
        return QStringLiteral("Finish Collection");
    case COMMAND_ID_BEARING_RESULT:
        return QStringLiteral("Bearing Result");
    case COMMAND_ID_COLLECTION_STATUS:
        return QStringLiteral("Collection Status");
    default:
        return QStringLiteral("Unknown command: %1").arg(vhfCommandId);
    }
}

void SendTunnelCommandState::_sendTunnelCommand()
{
    HeaderInfo_t tunnelHeader {};
    memcpy(&tunnelHeader, _payload, sizeof(tunnelHeader));
    _sentTunnelCommand = tunnelHeader.command;

    auto customPlugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    if (!customPlugin || !customPlugin->protocolCompatible()) {
        const QString commandText = commandIdToText(_sentTunnelCommand);
        const uint32_t controllerVersion = customPlugin ? customPlugin->controllerProtocolVersion() : 0;
        QString message;
        if (controllerVersion != 0 && controllerVersion != TUNNEL_PROTOCOL_VERSION) {
            message = QStringLiteral("Cannot send %1: TagTracker protocol version %2 does not match controller version %3.")
                          .arg(commandText)
                          .arg(TUNNEL_PROTOCOL_VERSION)
                          .arg(controllerVersion);
        } else if (controllerVersion == 0) {
            message = QStringLiteral("Cannot send %1 until a compatible controller heartbeat is received.").arg(commandText);
        } else {
            message = QStringLiteral("Cannot send %1: controller heartbeat lost.").arg(commandText);
        }
        qCWarning(CustomStateMachineLog) << message;
        setError(message);
        return;
    }

    WeakLinkInterfacePtr weakPrimaryLink = _vehicle->vehicleLinkManager()->primaryLink();

    if (!weakPrimaryLink.expired()) {
        connect(&_ackResponseTimer, &QTimer::timeout, this, &SendTunnelCommandState::_ackResponseTimedOut);
        connect(_vehicle, &Vehicle::mavlinkMessageReceived, this, &SendTunnelCommandState::_mavlinkMessageReceived);

        SharedLinkInterfacePtr  sharedLink  = weakPrimaryLink.lock();
        MAVLinkProtocol*        mavlink     = MAVLinkProtocol::instance();
        mavlink_message_t       msg;
        mavlink_tunnel_t        tunnel;

        memset(&tunnel, 0, sizeof(tunnel));

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

        qCDebug(CustomStateMachineLog) << "SendTunnelCommandState::_sendTunnelCommand: Sending tunnel command - " << commandIdToText(_sentTunnelCommand);

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

        qCDebug(CustomStateMachineLog) << "Tunnel command ack received - command:result" << commandIdToText(ack.command) << ack.result;
        if (ack.result == COMMAND_RESULT_SUCCESS) {
            emit commandSucceeded();
        } else {
            // Command failed
            if (sentTunnelCommand == COMMAND_ID_START_DETECTION) {
                // Special case for start detection failure, we want to clear the detector list
                DetectorList::instance()->clearDetectors();
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
        qCWarning(CustomStateMachineLog) << "SendTunnelCommandState::_handleTunnelCommandAck: Received unexpected command id ack expected:actual" <<
                      commandIdToText(_sentTunnelCommand) <<
                      commandIdToText(ack.command);
    }
}

void SendTunnelCommandState::_ackResponseTimedOut(void)
{
    QString message = QStringLiteral("%1 failed. No response from vehicle after %2 retries.").arg(commandIdToText(_sentTunnelCommand)).arg(_retryCount);

    _disconnectAll();

    if (_retryCount < _maxRetries) {
        qCDebug(CustomStateMachineLog) << message << "Retrying...";
        _retryCount++;
        _sendTunnelCommand();
    } else {
        qCWarning(CustomStateMachineLog) << message;
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
