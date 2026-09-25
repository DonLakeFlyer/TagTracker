#pragma once

#include <cstddef>
#include <cstdint>

#include <QtCore/QString>

/// Log text for TunnelProtocol traffic, using the protocol's own command, field and constant names.
namespace TunnelProtocolText {

enum class Direction
{
    Received,
    Sent,
};

QString commandName(uint32_t command);
QString collectionStatusName(uint32_t status);
QString collectionErrorName(uint32_t errorCode);

/// Decodes a complete tunnel payload into "COMMAND_NAME received: field: value ..." form.
QString formatMessage(Direction direction, const uint8_t* payload, size_t length);

}  // namespace TunnelProtocolText
