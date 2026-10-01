#include "PulseLogFolderTest.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonObject>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>

#include "CSVLogManager.h"
#include "PulseLogSidecar.h"
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

UT_REGISTER_TEST(PulseLogFolderTest, TestLabel::Integration, TestLabel::Vehicle)
