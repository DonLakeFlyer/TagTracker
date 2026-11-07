#pragma once

#include <cstddef>
#include <cstdint>

#include <QtCore/QString>

/// Text helpers for TunnelProtocol payloads: log formatting using the protocol's own names, and text field filling.
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

/// Copies at most size-1 UTF-8 bytes without splitting a character; zero-fills the rest, so always null-terminated.
void copyClipped(char* dst, size_t size, const QString& text);

template <size_t N>
void copyClipped(char (&dst)[N], const QString& text)
{
    copyClipped(dst, N, text);
}

}  // namespace TunnelProtocolText
