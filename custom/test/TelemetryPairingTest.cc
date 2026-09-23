#include "TelemetryPairingTest.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimeZone>
#include <QtCore/QtEndian>

#include "PulseLogSidecar.h"
#include "TelemetryPairing.h"

namespace {

constexpr quint64 UsecsPerMinute = 60ull * 1'000'000;

// A day ago, so no timestamp looks like it is from the future and gets byte-swapped
quint64 baseUsecs()
{
    static const quint64 base =
        static_cast<quint64>(QDateTime::currentDateTimeUtc().addDays(-1).toMSecsSinceEpoch()) * 1000;
    return base;
}

enum class Frame
{
    V1,
    V2,
    V2Signed,
};

QByteArray record(quint64 timestampUsecs, Frame frame, quint8 payloadLength = 9)
{
    QByteArray bytes(8, '\0');
    qToBigEndian(timestampUsecs, bytes.data());
    switch (frame) {
        case Frame::V1:
            bytes.append(char(0xFE));
            bytes.append(char(payloadLength));
            bytes.append(QByteArray(4 + payloadLength + 2, 'a'));
            break;
        case Frame::V2:
        case Frame::V2Signed:
            bytes.append(char(0xFD));
            bytes.append(char(payloadLength));
            bytes.append(char(frame == Frame::V2Signed ? 0x01 : 0x00));
            bytes.append(QByteArray(7 + payloadLength + 2 + (frame == Frame::V2Signed ? 13 : 0), 'b'));
            break;
    }
    return bytes;
}

// One record a minute from start to end inclusive
QByteArray recording(quint64 startUsecs, quint64 endUsecs)
{
    QByteArray bytes;
    for (quint64 t = startUsecs; t <= endUsecs; t += UsecsPerMinute) {
        bytes.append(record(t, Frame::V2));
    }
    return bytes;
}

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

// A connection folder as CSVLogManager leaves it, one pulse log and sidecar per run start
QString makeFolder(const QString& logDir, const QString& name, const QList<quint64>& runStartsUsecs)
{
    const QString folder = QDir(logDir).filePath(name);
    QDir().mkpath(folder);
    for (int i = 0; i < runStartsUsecs.count(); i++) {
        const QDateTime start = QDateTime::fromMSecsSinceEpoch(runStartsUsecs[i] / 1000, QTimeZone::UTC);
        const QString pulseLog = QStringLiteral("Pulse-%1.csv").arg(i);
        writeFile(QDir(folder).filePath(pulseLog), QByteArray());
        PulseLogSidecar::write(QDir(folder).filePath(QStringLiteral("Pulse-%1.json").arg(i)),
                               PulseLogSidecar::create(pulseLog, start, 1, QStringLiteral("test")));
    }
    return folder;
}

QStringList telemetryNames(const QString& folder)
{
    QStringList names;
    for (const QString& sidecarName :
         QDir(folder).entryList({QStringLiteral("Pulse-*.json")}, QDir::Files, QDir::Name)) {
        QJsonObject sidecar;
        PulseLogSidecar::read(QDir(folder).filePath(sidecarName), sidecar);
        names.append(sidecar[PulseLogSidecar::keyTelemetryFile].toString());
    }
    return names;
}

QStringList tlogsIn(const QString& folder)
{
    return QDir(folder).entryList({QStringLiteral("*.tlog")}, QDir::Files, QDir::Name);
}

}  // namespace

void TelemetryPairingTest::_readSpanWalksMixedRecords()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const quint64 t = baseUsecs();

    QByteArray bytes;
    bytes.append(record(t, Frame::V2));
    bytes.append(record(t + 1000, Frame::V1));
    bytes.append(record(t + 2000, Frame::V2Signed));
    bytes.append(record(t + 3000, Frame::V2, 0));
    // Unframed bytes, as a multi-message write from the GCS side can leave
    bytes.append(QByteArray(37, 'z'));
    bytes.append(record(t + 5 * UsecsPerMinute, Frame::V2, 255));

    const QString path = dir.filePath(QStringLiteral("mixed.tlog"));
    QVERIFY(writeFile(path, bytes));

    QCOMPARE(TelemetryPairing::readStartUsecs(path), t);
    const TelemetryPairing::Span span = TelemetryPairing::readSpan(path);
    QVERIFY(span.isValid());
    QCOMPARE(span.startUsecs, t);
    QCOMPARE(span.endUsecs, t + 5 * UsecsPerMinute);
}

void TelemetryPairingTest::_readSpanIgnoresTruncatedTail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const quint64 t = baseUsecs();

    QByteArray bytes = recording(t, t + 10 * UsecsPerMinute);
    // A crash can cut the last record short; its timestamp must not count
    bytes.append(record(t + 20 * UsecsPerMinute, Frame::V2).left(14));

    const QString path = dir.filePath(QStringLiteral("truncated.tlog"));
    QVERIFY(writeFile(path, bytes));

    const TelemetryPairing::Span span = TelemetryPairing::readSpan(path);
    QCOMPARE(span.startUsecs, t);
    QCOMPARE(span.endUsecs, t + 10 * UsecsPerMinute);

    const QString empty = dir.filePath(QStringLiteral("empty.tlog"));
    QVERIFY(writeFile(empty, QByteArray()));
    QCOMPARE(TelemetryPairing::readStartUsecs(empty), quint64(0));
    QVERIFY(!TelemetryPairing::readSpan(empty).isValid());
}

void TelemetryPairingTest::_pairsByContentNotName()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    // Names chosen so that sorting or parsing them as times would pick the wrong file
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("a-earlier.tlog")),
                      recording(t - 120 * UsecsPerMinute, t - 60 * UsecsPerMinute)));
    const QByteArray ours = recording(t, t + 60 * UsecsPerMinute);
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("z-ours.tlog")), ours));
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("m-later.tlog")),
                      recording(t + 120 * UsecsPerMinute, t + 180 * UsecsPerMinute)));

    const QString folder =
        makeFolder(logDir.path(), QStringLiteral("flight"), {t + 10 * UsecsPerMinute, t + 30 * UsecsPerMinute});

    QCOMPARE(TelemetryPairing::pairFolder(folder, telemetryDir.path()), TelemetryPairing::Result::Paired);
    QCOMPARE(tlogsIn(folder), QStringList({QStringLiteral("z-ours.tlog")}));
    QCOMPARE(telemetryNames(folder), QStringList({QStringLiteral("z-ours.tlog"), QStringLiteral("z-ours.tlog")}));

    QFile copy(QDir(folder).filePath(QStringLiteral("z-ours.tlog")));
    QVERIFY(copy.open(QIODevice::ReadOnly));
    QCOMPARE(copy.readAll(), ours);
    QVERIFY(QFile::exists(telemetryDir.filePath(QStringLiteral("z-ours.tlog"))));
}

void TelemetryPairingTest::_groundTestLeavesTelemetryEmpty()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    // Only an earlier flight's file exists; this connection's was deleted because the vehicle never armed
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("earlier.tlog")),
                      recording(t - 120 * UsecsPerMinute, t - 60 * UsecsPerMinute)));
    const QString folder = makeFolder(logDir.path(), QStringLiteral("ground"), {t + 5 * UsecsPerMinute});

    QCOMPARE(TelemetryPairing::pairFolder(folder, telemetryDir.path()), TelemetryPairing::Result::NoTelemetry);
    QVERIFY(tlogsIn(folder).isEmpty());
    QCOMPARE(telemetryNames(folder), QStringList({QString()}));
}

void TelemetryPairingTest::_twoFlightsOnOneRecording()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    // A battery swap without disconnecting: one recording spans both flights
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("both.tlog")), recording(t, t + 90 * UsecsPerMinute)));
    const QString folder = makeFolder(logDir.path(), QStringLiteral("swap"),
                                      {t + 5 * UsecsPerMinute, t + 20 * UsecsPerMinute, t + 70 * UsecsPerMinute});

    QCOMPARE(TelemetryPairing::pairFolder(folder, telemetryDir.path()), TelemetryPairing::Result::Paired);
    QCOMPARE(tlogsIn(folder), QStringList({QStringLiteral("both.tlog")}));
    QCOMPARE(telemetryNames(folder), QStringList(3, QStringLiteral("both.tlog")));
}

void TelemetryPairingTest::_duplicateCopiesPairWithFirstName()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    const QByteArray bytes = recording(t, t + 60 * UsecsPerMinute);
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("flight.tlog")), bytes));
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("flight.1.tlog")), bytes));
    const QString folder = makeFolder(logDir.path(), QStringLiteral("dup"), {t + 10 * UsecsPerMinute});

    QCOMPARE(TelemetryPairing::pairFolder(folder, telemetryDir.path()), TelemetryPairing::Result::Paired);
    QCOMPARE(tlogsIn(folder), QStringList({QStringLiteral("flight.1.tlog")}));
}

void TelemetryPairingTest::_differentMatchesAreAmbiguous()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("one.tlog")), recording(t, t + 60 * UsecsPerMinute)));
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("two.tlog")),
                      recording(t - 10 * UsecsPerMinute, t + 120 * UsecsPerMinute)));
    const QString folder = makeFolder(logDir.path(), QStringLiteral("ambiguous"), {t + 10 * UsecsPerMinute});

    expectLogMessage("CustomPluginLog", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Several different telemetry files")));
    QCOMPARE(TelemetryPairing::pairFolder(folder, telemetryDir.path()), TelemetryPairing::Result::Ambiguous);
    verifyExpectedLogMessage();
    QVERIFY(tlogsIn(folder).isEmpty());
    QCOMPARE(telemetryNames(folder), QStringList({QString()}));
}

void TelemetryPairingTest::_pairAllSkipsOpenFolderAndRepeatsSafely()
{
    QTemporaryDir telemetryDir;
    QTemporaryDir logDir;
    QVERIFY(telemetryDir.isValid() && logDir.isValid());
    const quint64 t = baseUsecs();

    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("first.tlog")), recording(t, t + 60 * UsecsPerMinute)));
    QVERIFY(writeFile(telemetryDir.filePath(QStringLiteral("second.tlog")),
                      recording(t + 120 * UsecsPerMinute, t + 180 * UsecsPerMinute)));
    const QString first = makeFolder(logDir.path(), QStringLiteral("first"), {t + 10 * UsecsPerMinute});
    const QString second = makeFolder(logDir.path(), QStringLiteral("second"), {t + 130 * UsecsPerMinute});
    const QString ground = makeFolder(logDir.path(), QStringLiteral("ground"), {t + 90 * UsecsPerMinute});
    QDir().mkpath(QDir(logDir.path()).filePath(QStringLiteral("not-a-pulse-folder")));

    TelemetryPairing::Summary summary = TelemetryPairing::pairAll(logDir.path(), telemetryDir.path(), second);
    QCOMPARE(summary.paired, 1);
    QCOMPARE(summary.noTelemetry, 1);
    QCOMPARE(tlogsIn(first), QStringList({QStringLiteral("first.tlog")}));
    QVERIFY(tlogsIn(second).isEmpty());
    QVERIFY(tlogsIn(ground).isEmpty());

    summary = TelemetryPairing::pairAll(logDir.path(), telemetryDir.path());
    QCOMPARE(summary.paired, 1);
    QCOMPARE(summary.noTelemetry, 1);
    QCOMPARE(tlogsIn(second), QStringList({QStringLiteral("second.tlog")}));

    // Everything paired stays paired and nothing is copied twice
    summary = TelemetryPairing::pairAll(logDir.path(), telemetryDir.path());
    QCOMPARE(summary.paired, 0);
    QCOMPARE(summary.noTelemetry, 1);
    QCOMPARE(tlogsIn(first), QStringList({QStringLiteral("first.tlog")}));
    QCOMPARE(telemetryNames(first), QStringList({QStringLiteral("first.tlog")}));
}

UT_REGISTER_TEST(TelemetryPairingTest, TestLabel::Unit)
