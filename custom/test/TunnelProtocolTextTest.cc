#include "TunnelProtocolTextTest.h"

#include <cstdio>
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
    snprintf(ack.message, sizeof(ack.message), "%s", "no detectors");

    QCOMPARE(format(Direction::Received, ack),
             QStringLiteral("ACK received: command: START_COLLECTION request_id: 42 result: COMMAND_RESULT_FAILURE message: \"no detectors\""));

    // The header request_id is shown only when set; an empty text field is omitted
    ack.header.request_id = 7;
    ack.message[0]        = '\0';
    QCOMPARE(format(Direction::Sent, ack),
             QStringLiteral("ACK sent: request_id: 7 command: START_COLLECTION request_id: 42 result: COMMAND_RESULT_FAILURE"));
}

void TunnelProtocolTextTest::_formatsStartIqRecording()
{
    StartIqRecording_t start{};
    start.header.command            = COMMAND_ID_START_IQ_RECORDING;
    start.radio_center_frequency_hz = 147970000;
    start.gain                      = 21;

    const QString text = format(Direction::Sent, start);
    QVERIFY2(text.startsWith(QStringLiteral("START_IQ_RECORDING sent:")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("radio_center_frequency_hz: 147970000")), qPrintable(text));
}

void TunnelProtocolTextTest::_formatsTagNames()
{
    TagInfo_t tag{};
    tag.header.command = COMMAND_ID_TAG;
    tag.id             = 2;
    snprintf(tag.name, sizeof(tag.name), "%s", "Tag Two");
    snprintf(tag.rate1_name, sizeof(tag.rate1_name), "%s", "Moving");

    const QString text = format(Direction::Sent, tag);
    QVERIFY2(text.contains(QStringLiteral("name: \"Tag Two\"")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("rate1_name: \"Moving\"")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("rate2_name")), qPrintable(text));   // empty text is omitted

    snprintf(tag.rate2_name, sizeof(tag.rate2_name), "%s", "Still");
    const QString withRate2 = format(Direction::Sent, tag);
    QVERIFY2(withRate2.contains(QStringLiteral("rate2_name: \"Still\"")), qPrintable(withRate2));
}

void TunnelProtocolTextTest::_copyClipped_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QByteArray>("expected");

    // Destination holds 5 bytes plus the terminator
    QTest::newRow("fits")                  << QStringLiteral("abc")                 << QByteArray("abc");
    QTest::newRow("exactly full")          << QStringLiteral("abcde")               << QByteArray("abcde");
    QTest::newRow("ascii one over")        << QStringLiteral("abcdef")              << QByteArray("abcde");
    QTest::newRow("2-byte straddles")      << QStringLiteral("abcd\u00e9")          << QByteArray("abcd");
    QTest::newRow("2-byte ends at limit")  << QStringLiteral("abc\u00e9x")         << QByteArray("abc\xc3\xa9");
    QTest::newRow("3-byte straddles")      << QStringLiteral("abc\u20ac")           << QByteArray("abc");
    QTest::newRow("4-byte straddles")      << QString::fromUtf8("ab\xf0\x9f\x98\x80") << QByteArray("ab");
    QTest::newRow("empty")                 << QString()                            << QByteArray();
}

void TunnelProtocolTextTest::_copyClipped()
{
    QFETCH(QString, text);
    QFETCH(QByteArray, expected);

    char dst[6];
    memset(dst, 'z', sizeof(dst));
    TunnelProtocolText::copyClipped(dst, text);

    QCOMPARE(QByteArray(dst, static_cast<qsizetype>(strnlen(dst, sizeof(dst)))), expected);
    QVERIFY(strnlen(dst, sizeof(dst)) < sizeof(dst));
    for (size_t i = static_cast<size_t>(expected.size()); i < sizeof(dst); i++) {
        QCOMPARE(dst[i], '\0');
    }
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
    heartbeat.cpu_throttle_flags = CPU_THROTTLE_FLAG_THERMAL | CPU_THROTTLE_FLAG_UNDERVOLTAGE;
    heartbeat.cpu_throttle_since_boot_flags = CPU_THROTTLE_FLAG_UNDERVOLTAGE;
    const QString text = format(Direction::Received, heartbeat);
    QVERIFY2(text.contains(QStringLiteral("HEARTBEAT_SYSTEM_ID_UNKNOWN(77)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("HEARTBEAT_STATUS_UNKNOWN(88)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("cpu_throttle_flags: CPU_THROTTLE_FLAG_THERMAL|CPU_THROTTLE_FLAG_UNDERVOLTAGE")),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("cpu_throttle_since_boot_flags: CPU_THROTTLE_FLAG_UNDERVOLTAGE")), qPrintable(text));
    heartbeat.cpu_throttle_flags = 0;
    QVERIFY(format(Direction::Received, heartbeat).contains(QStringLiteral("cpu_throttle_flags: NONE")));
    heartbeat.cpu_throttle_since_boot_flags = 0;
    QVERIFY(format(Direction::Received, heartbeat).contains(QStringLiteral("cpu_throttle_since_boot_flags: NONE")));
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
