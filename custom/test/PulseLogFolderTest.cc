#include "PulseLogFolderTest.h"

#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

#include "CSVLogManager.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "Fact.h"
#include "PulseLogSidecar.h"
#include "TunnelProtocol.h"
#include "Vehicle.h"

namespace {

// Pulse log names carry milliseconds; two runs in the same millisecond would share a name
void waitForNextMillisecond()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QTRY_VERIFY_WITH_TIMEOUT(QDateTime::currentMSecsSinceEpoch() > now, TestTimeout::shortMs());
}

QStringList entries(const QString& folder, const QString& filter)
{
    return QDir(folder).entryList({filter}, QDir::Files, QDir::Name);
}

// The tunnel heartbeat the controller sends every few seconds
mavlink_message_t controllerHeartbeat(uint16_t status)
{
    TunnelProtocol::Heartbeat_t heartbeat {};
    heartbeat.header.command = COMMAND_ID_HEARTBEAT;
    heartbeat.protocol_version = TUNNEL_PROTOCOL_VERSION;
    heartbeat.system_id = HEARTBEAT_SYSTEM_ID_MAVLINKCONTROLLER;
    heartbeat.status = status;

    mavlink_tunnel_t tunnel {};
    tunnel.payload_length = sizeof(heartbeat);
    memcpy(tunnel.payload, &heartbeat, sizeof(heartbeat));
    mavlink_message_t message;
    mavlink_msg_tunnel_encode(1, MAV_COMP_ID_ONBOARD_COMPUTER, &message, &tunnel);
    return message;
}

QJsonObject readSidecar(const QString& path)
{
    QJsonObject sidecar;
    if (!PulseLogSidecar::read(path, sidecar)) {
        return QJsonObject();
    }
    return sidecar;
}

}  // namespace

void PulseLogFolderTest::_twoRunsShareOneFolder()
{
    _connectMockLink();
    QVERIFY(_vehicle);

    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());

    manager.csvStartFullPulseLog();
    const QString folder = manager.connectionFolder();
    QVERIFY(!folder.isEmpty());
    manager.csvStopFullPulseLog();
    waitForNextMillisecond();
    manager.csvStartFullPulseLog();
    manager.csvStopFullPulseLog();

    QCOMPARE(manager.connectionFolder(), folder);
    QCOMPARE(QDir(logDir.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count(), 1);

    const QStringList pulseLogs = entries(folder, QStringLiteral("Pulse-*.csv"));
    const QStringList sidecars = entries(folder, QStringLiteral("Pulse-*.json"));
    QCOMPARE(pulseLogs.count(), 2);
    QCOMPARE(sidecars.count(), 2);

    // The folder is named from the first pulse log's timestamp
    QCOMPARE(QStringLiteral("Pulse-%1.csv").arg(QFileInfo(folder).fileName()), pulseLogs[0]);

    QDateTime previousStop;
    for (int i = 0; i < 2; i++) {
        const QJsonObject sidecar = readSidecar(QDir(folder).filePath(sidecars[i]));
        QCOMPARE(sidecar[PulseLogSidecar::keyPulseLog].toString(), pulseLogs[i]);
        QCOMPARE(sidecar[PulseLogSidecar::keyVehicleId].toInt(), _vehicle->id());
        QCOMPARE(sidecar[PulseLogSidecar::keyAppVersion].toString(), QCoreApplication::applicationVersion());
        QVERIFY(sidecar[PulseLogSidecar::keyTelemetryFile].isNull());

        const QDateTime start = PulseLogSidecar::parseUtc(sidecar[PulseLogSidecar::keyStartUtc]);
        const QDateTime stop = PulseLogSidecar::parseUtc(sidecar[PulseLogSidecar::keyStopUtc]);
        QVERIFY(start.isValid());
        QVERIFY(stop.isValid());
        QCOMPARE(start.timeSpec(), Qt::UTC);
        QVERIFY(start <= stop);
        if (previousStop.isValid()) {
            QVERIFY(previousStop <= start);
        }
        previousStop = stop;
    }
}

void PulseLogFolderTest::_openRunSidecarHasNoStop()
{
    _connectMockLink();

    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());

    // Offloading mid-run must find the pulse log and its sidecar already in the folder
    manager.csvStartFullPulseLog();
    const QString folder = manager.connectionFolder();
    const QStringList pulseLogs = entries(folder, QStringLiteral("Pulse-*.csv"));
    const QStringList sidecars = entries(folder, QStringLiteral("Pulse-*.json"));
    QCOMPARE(pulseLogs.count(), 1);
    QCOMPARE(sidecars.count(), 1);

    const QJsonObject sidecar = readSidecar(QDir(folder).filePath(sidecars[0]));
    QVERIFY(PulseLogSidecar::parseUtc(sidecar[PulseLogSidecar::keyStartUtc]).isValid());
    QVERIFY(sidecar[PulseLogSidecar::keyStopUtc].isNull());

    manager.csvStopFullPulseLog();
}

void PulseLogFolderTest::_disconnectStartsNewFolder()
{
    _connectMockLink();

    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());
    manager.init();
    QSignalSpy closedSpy(&manager, &CSVLogManager::connectionFolderClosed);

    manager.csvStartFullPulseLog();
    manager.csvStopFullPulseLog();
    const QString firstFolder = manager.connectionFolder();

    _disconnectMockLink();
    QTRY_COMPARE_WITH_TIMEOUT(closedSpy.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(closedSpy[0][0].toString(), firstFolder);
    QVERIFY(manager.connectionFolder().isEmpty());

    _connectMockLink();
    waitForNextMillisecond();
    manager.csvStartFullPulseLog();
    manager.csvStopFullPulseLog();
    QVERIFY(!manager.connectionFolder().isEmpty());
    QVERIFY(manager.connectionFolder() != firstFolder);
    QCOMPARE(QDir(logDir.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count(), 2);
}

void PulseLogFolderTest::_disconnectClosesOpenLogs()
{
    _connectMockLink();

    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());
    manager.init();
    QSignalSpy closedSpy(&manager, &CSVLogManager::connectionFolderClosed);

    // Detection still running when the vehicle goes
    manager.csvStartFullPulseLog();
    manager.csvStartRotationPulseLog();
    const QString firstFolder = manager.connectionFolder();

    _disconnectMockLink();
    QTRY_COMPARE_WITH_TIMEOUT(closedSpy.count(), 1, TestTimeout::mediumMs());
    const QStringList sidecars = entries(firstFolder, QStringLiteral("Pulse-*.json"));
    QCOMPARE(sidecars.count(), 1);
    QVERIFY(!readSidecar(QDir(firstFolder).filePath(sidecars[0]))[PulseLogSidecar::keyStopUtc].isNull());

    // The next connection's logs open in a new folder rather than finding the old ones still open
    _connectMockLink();
    waitForNextMillisecond();
    manager.csvStartFullPulseLog();
    manager.csvStartRotationPulseLog();
    const QString secondFolder = manager.connectionFolder();
    QVERIFY(!secondFolder.isEmpty());
    QVERIFY(secondFolder != firstFolder);
    QCOMPARE(entries(secondFolder, QStringLiteral("Pulse-*.csv")).count(), 1);
    QCOMPARE(entries(secondFolder, QStringLiteral("Rotation-*.csv")).count(), 1);
    manager.csvStopRotationPulseLog();
    manager.csvStopFullPulseLog();
}

void PulseLogFolderTest::_rotationLogsNumberedPerFolder()
{
    _connectMockLink();

    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());

    manager.csvStartFullPulseLog();
    const QString firstFolder = manager.connectionFolder();
    for (int i = 0; i < 2; i++) {
        manager.csvStartRotationPulseLog();
        manager.csvStopRotationPulseLog();
    }
    manager.csvStopFullPulseLog();
    QCOMPARE(entries(firstFolder, QStringLiteral("Rotation-*.csv")),
             QStringList({QStringLiteral("Rotation-1.csv"), QStringLiteral("Rotation-2.csv")}));

    manager.connectionEnded();
    waitForNextMillisecond();
    manager.csvStartFullPulseLog();
    manager.csvStartRotationPulseLog();
    manager.csvStopRotationPulseLog();
    manager.csvStopFullPulseLog();

    const QString secondFolder = manager.connectionFolder();
    QVERIFY(secondFolder != firstFolder);
    QCOMPARE(entries(secondFolder, QStringLiteral("Rotation-*.csv")), QStringList({QStringLiteral("Rotation-1.csv")}));
    // The earlier connection's rotation logs are left alone
    QCOMPARE(entries(firstFolder, QStringLiteral("Rotation-*.csv")).count(), 2);
}

void PulseLogFolderTest::_noVehicleRunGetsOwnFolder()
{
    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager manager;
    manager.setLogSavePathOverride(logDir.path());
    QSignalSpy closedSpy(&manager, &CSVLogManager::connectionFolderClosed);

    manager.csvStartFullPulseLog();
    const QString folder = manager.connectionFolder();
    QVERIFY(!folder.isEmpty());
    manager.csvStopFullPulseLog();

    // No disconnect will ever come, so the run's own stop closes the folder
    QCOMPARE(closedSpy.count(), 1);
    QVERIFY(manager.connectionFolder().isEmpty());

    const QStringList sidecars = entries(folder, QStringLiteral("Pulse-*.json"));
    QCOMPARE(sidecars.count(), 1);
    const QJsonObject sidecar = readSidecar(QDir(folder).filePath(sidecars[0]));
    QVERIFY(sidecar[PulseLogSidecar::keyVehicleId].isNull());
    QVERIFY(sidecar[PulseLogSidecar::keyTelemetryFile].isNull());
}

void PulseLogFolderTest::_detectingHeartbeatResumesPulseLog()
{
    auto* plugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    QVERIFY(plugin);
    Fact* flightMode = plugin->customSettings()->detectionFlightMode();
    const QVariant previousFlightMode = flightMode->rawValue();
    QTemporaryDir logDir;
    QVERIFY(logDir.isValid());
    CSVLogManager& manager = plugin->csvLogManager();
    manager.setLogSavePathOverride(logDir.path());
    // Unit tests skip CustomPlugin::init, which is what normally watches for the disconnect
    manager.init();
    const auto restore = qScopeGuard([&]() {
        manager.csvStopFullPulseLog();
        manager.connectionEnded();
        manager.setLogSavePathOverride(QString());
        flightMode->setRawValue(previousFlightMode);
    });

    _connectMockLink();
    QVERIFY(_vehicle);

    // Python modes start and stop detection per slice, so a Detecting heartbeat there is not a lost log
    flightMode->setRawValue(CustomSettings::ManualRotation);
    plugin->mavlinkMessage(_vehicle, nullptr, controllerHeartbeat(HEARTBEAT_STATUS_DETECTING));
    QVERIFY(!manager.fullPulseLogOpen());

    // Detection already running with no log, as after a lost Start Detection reply
    flightMode->setRawValue(CustomSettings::SurveyDetection);
    expectAppMessage(QRegularExpression(QStringLiteral("pulse logging had stopped")));
    plugin->mavlinkMessage(_vehicle, nullptr, controllerHeartbeat(HEARTBEAT_STATUS_DETECTING));
    verifyExpectedLogMessage();
    QVERIFY(manager.fullPulseLogOpen());
    const QString firstFolder = manager.connectionFolder();
    QCOMPARE(entries(firstFolder, QStringLiteral("Pulse-*.csv")).count(), 1);

    // Once open, further heartbeats leave it alone
    plugin->mavlinkMessage(_vehicle, nullptr, controllerHeartbeat(HEARTBEAT_STATUS_DETECTING));
    QCOMPARE(manager.connectionFolder(), firstFolder);
    QCOMPARE(entries(firstFolder, QStringLiteral("Pulse-*.csv")).count(), 1);

    // The disconnect closes the log; the controller keeps detecting through the reconnect
    _disconnectMockLink();
    QTRY_VERIFY_WITH_TIMEOUT(manager.connectionFolder().isEmpty(), TestTimeout::mediumMs());
    QVERIFY(!manager.fullPulseLogOpen());
    _connectMockLink();
    QVERIFY(_vehicle);
    waitForNextMillisecond();

    expectAppMessage(QRegularExpression(QStringLiteral("pulse logging had stopped")));
    plugin->mavlinkMessage(_vehicle, nullptr, controllerHeartbeat(HEARTBEAT_STATUS_DETECTING));
    verifyExpectedLogMessage();
    QVERIFY(manager.fullPulseLogOpen());
    const QString secondFolder = manager.connectionFolder();
    QVERIFY(!secondFolder.isEmpty());
    QVERIFY(secondFolder != firstFolder);
    QCOMPARE(entries(secondFolder, QStringLiteral("Pulse-*.csv")).count(), 1);
    const QStringList sidecars = entries(secondFolder, QStringLiteral("Pulse-*.json"));
    QCOMPARE(sidecars.count(), 1);
    QCOMPARE(readSidecar(QDir(secondFolder).filePath(sidecars[0]))[PulseLogSidecar::keyVehicleId].toInt(),
             _vehicle->id());

    plugin->mavlinkMessage(_vehicle, nullptr, controllerHeartbeat(HEARTBEAT_STATUS_IDLE));
}

UT_REGISTER_TEST(PulseLogFolderTest, TestLabel::Integration, TestLabel::Vehicle)
