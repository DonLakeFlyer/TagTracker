#include "TunnelProtocolTextTest.h"

#include <cstring>

#include <QtTest/QTest>

#include "TunnelProtocol.h"
#include "TunnelProtocolText.h"

using namespace TunnelProtocol;
using TunnelProtocolText::Direction;

namespace {

template <typename T>
QString format(Direction direction, const T& message, size_t length = sizeof(T))
{
    return TunnelProtocolText::formatMessage(direction, reinterpret_cast<const uint8_t*>(&message), length);
}

}  // namespace

void TunnelProtocolTextTest::_formatsAck()
{
    AckInfo_t ack{};
    ack.header.command    = COMMAND_ID_ACK;
    ack.header.request_id = 0;
    ack.command           = COMMAND_ID_START_COLLECTION;
    ack.request_id        = 42;
    ack.result            = COMMAND_RESULT_FAILURE;
    strncpy(ack.message, "no detectors", sizeof(ack.message) - 1);

    QCOMPARE(format(Direction::Received, ack),
             QStringLiteral("ACK received: command: START_COLLECTION request_id: 42 result: COMMAND_RESULT_FAILURE message: \"no detectors\""));

    // The header request_id is shown only when set; an empty text field is omitted
    ack.header.request_id = 7;
    ack.message[0]        = '\0';
    QCOMPARE(format(Direction::Sent, ack),
             QStringLiteral("ACK sent: request_id: 7 command: START_COLLECTION request_id: 42 result: COMMAND_RESULT_FAILURE"));
}

void TunnelProtocolTextTest::_payloadShorterThanHeader()
{
    const uint8_t payload[3] = {};
    QCOMPARE(TunnelProtocolText::formatMessage(Direction::Received, payload, sizeof(payload)),
             QStringLiteral("Tunnel message received: payload_length: 3 expected at least: %1").arg(sizeof(HeaderInfo_t)));
}

void TunnelProtocolTextTest::_payloadSizeMismatch()
{
    Heartbeat_t heartbeat{};
    heartbeat.header.command = COMMAND_ID_HEARTBEAT;

    const QString truncated = format(Direction::Received, heartbeat, sizeof(heartbeat) - 1);
    QCOMPARE(truncated, QStringLiteral("HEARTBEAT received: payload_length: %1 expected: %2")
                            .arg(sizeof(heartbeat) - 1).arg(sizeof(heartbeat)));

    uint8_t oversized[sizeof(heartbeat) + 4] = {};
    memcpy(oversized, &heartbeat, sizeof(heartbeat));
    QCOMPARE(TunnelProtocolText::formatMessage(Direction::Received, oversized, sizeof(oversized)),
             QStringLiteral("HEARTBEAT received: payload_length: %1 expected: %2")
                 .arg(sizeof(oversized)).arg(sizeof(heartbeat)));
}

void TunnelProtocolTextTest::_unknownCommandAndValues()
{
    HeaderInfo_t header{};
    header.command = 999;
    QCOMPARE(format(Direction::Received, header),
             QStringLiteral("COMMAND_ID_UNKNOWN(999) received: payload_length: %1").arg(sizeof(header)));

    Heartbeat_t heartbeat{};
    heartbeat.header.command   = COMMAND_ID_HEARTBEAT;
    heartbeat.protocol_version = TUNNEL_PROTOCOL_VERSION;
    heartbeat.system_id        = 77;
    heartbeat.status           = 88;
    const QString text = format(Direction::Received, heartbeat);
    QVERIFY2(text.contains(QStringLiteral("HEARTBEAT_SYSTEM_ID_UNKNOWN(77)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("HEARTBEAT_STATUS_UNKNOWN(88)")), qPrintable(text));
}

void TunnelProtocolTextTest::_unterminatedTextIsBounded()
{
    AckInfo_t ack{};
    ack.header.command = COMMAND_ID_ACK;
    ack.command        = COMMAND_ID_HEARTBEAT;
    ack.result         = COMMAND_RESULT_SUCCESS;
    memset(ack.message, 'x', sizeof(ack.message));

    const QString text = format(Direction::Received, ack);
    const QString expectedMessage = QStringLiteral("\"%1\"").arg(QString(sizeof(ack.message), QLatin1Char('x')));
    QVERIFY2(text.endsWith(QStringLiteral(" message: ") + expectedMessage), qPrintable(text));
}

UT_REGISTER_TEST(TunnelProtocolTextTest, TestLabel::Unit)
