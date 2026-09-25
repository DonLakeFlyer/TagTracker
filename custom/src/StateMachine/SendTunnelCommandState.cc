#include "SendTunnelCommandState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"
#include "TunnelProtocolText.h"
#include "DetectorList.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "VehicleLinkManager.h"
#include "LinkInterface.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "AudioOutput.h"

#include <QRandomGenerator>

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

    connect(this, &QState::entered, this, &SendTunnelCommandState::_startCommand);
    connect(this, &QState::exited, this, &SendTunnelCommandState::_disconnectAll);
}

uint32_t SendTunnelCommandState::nextRequestId()
{
    static uint32_t next = QRandomGenerator::global()->generate();
    if (++next == 0) {
        ++next;   // 0 means "no request id" on the wire
    }
    return next;
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
    case COMMAND_ID_SET_LOG_LEVEL:
        return QStringLiteral("Set Log Level");
    default:
        return QStringLiteral("Unknown command: %1").arg(vhfCommandId);
    }
}

void SendTunnelCommandState::_startCommand()
{
    if (_payloadSize < sizeof(HeaderInfo_t) || _payloadSize > sizeof(mavlink_tunnel_t::payload)) {
        qCCritical(CustomPluginLog) << "Invalid tunnel payload size:" << _payloadSize
                                    << "max:" << sizeof(mavlink_tunnel_t::payload);
        setError(QStringLiteral("Internal error: invalid tunnel command size %1").arg(_payloadSize));
        return;
    }

    // Each entry is a new command; retries from the ACK timer keep this id.
    _retryCount = 0;
    _requestId  = nextRequestId();
    HeaderInfo_t tunnelHeader {};
    memcpy(&tunnelHeader, _payload, sizeof(tunnelHeader));
    tunnelHeader.request_id = _requestId;
    memcpy(_payload, &tunnelHeader, sizeof(tunnelHeader));
    _sendTunnelCommand();
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
        qCWarning(CustomPluginLog) << message;
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

        qCDebug(CustomPluginLog).noquote()
            << TunnelProtocolText::formatMessage(TunnelProtocolText::Direction::Sent, _payload, _payloadSize)
            << "attempt:" << (_retryCount + 1);

        _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
}

void SendTunnelCommandState::_handleTunnelCommandAck(const mavlink_tunnel_t& tunnel)
{
    AckInfo_t ack;

    memcpy(&ack, tunnel.payload, sizeof(ack));

    if (ack.request_id == _requestId && ack.command == _sentTunnelCommand) {
        auto sentTunnelCommand = _sentTunnelCommand;

        _disconnectAll();

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
        // A late ACK for an earlier attempt or command; the controller will
        // answer the current request_id separately.
        qCWarning(CustomPluginLog).noquote() << "Ignoring stale ACK expected command:"
                                             << TunnelProtocolText::commandName(_sentTunnelCommand)
                                             << "request_id:" << _requestId
                                             << "received command:" << TunnelProtocolText::commandName(ack.command)
                                             << "request_id:" << ack.request_id;
    }
}

void SendTunnelCommandState::_ackResponseTimedOut(void)
{
    QString message = QStringLiteral("%1 failed. No response from vehicle after %2 retries.").arg(commandIdToText(_sentTunnelCommand)).arg(_retryCount);
    const QString commandName = TunnelProtocolText::commandName(_sentTunnelCommand);

    _disconnectAll();

    if (_retryCount < _maxRetries) {
        qCDebug(CustomPluginLog).noquote() << "No ACK for" << commandName << "request_id:" << _requestId
                                           << "retry:" << (_retryCount + 1);
        _retryCount++;
        _sendTunnelCommand();
    } else {
        qCWarning(CustomPluginLog).noquote() << "No ACK for" << commandName << "request_id:" << _requestId
                                             << "retries:" << _retryCount;
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
