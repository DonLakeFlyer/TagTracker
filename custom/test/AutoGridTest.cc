#include "AutoGridTest.h"

#include <cmath>

#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtTest/QSignalSpy>

#include "AutoGridStateMachine.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "Fact.h"
#include "GridGenerator.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "Vehicle.h"

namespace {

// Mission-position sentinels for _endOfMission rows, resolved against the generated grid
constexpr int kAtLoiterItem = -10;
constexpr int kAtRtlItem = -11;
constexpr int kMidGridItem = 3;

CustomPlugin* autoGridTestPlugin()
{
    return qobject_cast<CustomPlugin*>(CustomPlugin::instance());
}

int gridItemSeq(const GridGenerator::Result& result, MAV_CMD command)
{
    for (const GridGenerator::Item& item : result.items) {
        if (item.command == command) {
            return item.seq;
        }
    }
    return -1;
}

}  // namespace

void AutoGridTest::_takeoff()
{
    // Going airborne samples the GCS pressure sensor, which test hosts do not have
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg, QRegularExpression(QStringLiteral("[Pp]ressure")));
    _vehicle->sendMavCommand(_vehicle->defaultComponentId(), MAV_CMD_NAV_TAKEOFF, true, 0, 0, 0, 0, 0, 0,
                             50.0f /* altitude */);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->airborne(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->altitudeRelative()->rawValue().toDouble() > 10.0, 10000);
}

void AutoGridTest::_sendMissionCurrent(int wireSeq)
{
    mavlink_mission_current_t missionCurrent{};
    missionCurrent.seq = static_cast<uint16_t>(wireSeq);
    mavlink_message_t message;
    mavlink_msg_mission_current_encode_chan(static_cast<uint8_t>(_mockLink->vehicleId()), MAV_COMP_ID_AUTOPILOT1,
                                            _mockLink->outgoingMavlinkChannel(), &message, &missionCurrent);
    _mockLink->respondWithMavlinkMessage(message);
    QTRY_COMPARE_WITH_TIMEOUT(_vehicle->missionManager()->currentIndex(), wireSeq, 5000);
}

AutoGridStateMachine* AutoGridTest::_startMonitoringGrid()
{
    GridGenerator::Params params;  // spec §3 defaults
    params.origin = _vehicle->coordinate();
    params.headingDeg = 37.5;
    params.altitudeM = 30.0;
    params.acceptanceRadiusM = 3.0;
    const GridGenerator::Result result = GridGenerator::generate(params);
    if (!result.isValid()) {
        return nullptr;
    }

    auto machine = new AutoGridStateMachine(params, result, nullptr);
    machine->start();
    if (!QTest::qWaitFor([machine]() { return machine->currentStateName() == QStringLiteral("MonitorGrid"); }, 20000)) {
        machine->stopMachine();
        return nullptr;
    }
    return machine;
}

void AutoGridTest::_settingsDefaults()
{
    CustomSettings* settings = autoGridTestPlugin()->customSettings();
    QVERIFY(settings);

    struct Expected
    {
        Fact* fact;
        double defaultValue;
        double min;
        double max;
    };

    const Expected expected[] = {
        {settings->gridWidth(), 100, 10, 500},     {settings->gridDepth(), 100, 10, 500},
        {settings->gridLaneSpacing(), 20, 2, 100}, {settings->gridCruiseSpeed(), 6, 1, 12},
        {settings->gridTurnSpeed(), 3, 0, 12},     {settings->gridArcStep(), 45, 10, 90},
        {settings->gridEndLoiter(), 10, 0, 120},   {settings->gridAcceptanceRadius(), 3, 0, 20},
    };
    for (const Expected& row : expected) {
        QVERIFY2(row.fact, "missing fact");
        QCOMPARE(row.fact->rawDefaultValue().toDouble(), row.defaultValue);
        QCOMPARE(row.fact->rawMin().toDouble(), row.min);
        QCOMPARE(row.fact->rawMax().toDouble(), row.max);
    }
    QCOMPARE(settings->gridStartSide()->rawDefaultValue().toUInt(), static_cast<uint>(CustomSettings::GridStartLeft));
    QCOMPARE(settings->gridStartSide()->enumStrings(), (QStringList{QStringLiteral("Left"), QStringLiteral("Right")}));

    // The settings defaults are the generator's spec §3 defaults, except the phase 2 acceptance radius
    const GridGenerator::Params defaults;
    QCOMPARE(settings->gridWidth()->rawDefaultValue().toDouble(), defaults.widthM);
    QCOMPARE(settings->gridDepth()->rawDefaultValue().toDouble(), defaults.depthM);
    QCOMPARE(settings->gridLaneSpacing()->rawDefaultValue().toDouble(), defaults.spacingM);
    QCOMPARE(settings->gridCruiseSpeed()->rawDefaultValue().toDouble(), defaults.cruiseSpeedMS);
    QCOMPARE(settings->gridTurnSpeed()->rawDefaultValue().toDouble(), defaults.turnSpeedMS);
    QCOMPARE(settings->gridArcStep()->rawDefaultValue().toDouble(), defaults.arcStepDeg);
    QCOMPARE(settings->gridEndLoiter()->rawDefaultValue().toDouble(), defaults.endLoiterS);
}

void AutoGridTest::_normalizeHeading_data()
{
    QTest::addColumn<double>("heading");
    QTest::addColumn<double>("expected");

    QTest::newRow("-0.4") << -0.4 << 359.6;
    QTest::newRow("360") << 360.0 << 0.0;
    QTest::newRow("0") << 0.0 << 0.0;
    QTest::newRow("359.9") << 359.9 << 359.9;
    QTest::newRow("725") << 725.0 << 5.0;
    QTest::newRow("-360") << -360.0 << 0.0;
    QTest::newRow("tiny negative") << -1e-15 << 0.0;
}

void AutoGridTest::_normalizeHeading()
{
    QFETCH(double, heading);
    QFETCH(double, expected);

    const double normalized = CustomPlugin::normalizeHeading(heading);
    QVERIFY((normalized >= 0.0) && (normalized < 360.0));
    QVERIFY2(std::abs(normalized - expected) < 1e-9, qPrintable(QString::number(normalized, 'g', 17)));
}

void AutoGridTest::_classifyFlightModeChange_data()
{
    QTest::addColumn<QString>("flightMode");
    QTest::addColumn<bool>("missionStarted");
    QTest::addColumn<bool>("atMissionEnd");
    QTest::addColumn<int>("expected");

    using Change = AutoGridStateMachine::ModeChange;
    const QString mission = QStringLiteral("Mission");
    const QString rtl = QStringLiteral("Safe Recovery");
    const QString hold = QStringLiteral("Hold");
    const QString position = QStringLiteral("Position");

    QTest::newRow("Mission before start") << mission << false << false << int(Change::Continue);
    QTest::newRow("Mission mid-grid") << mission << true << false << int(Change::Continue);
    QTest::newRow("Return at end") << rtl << true << true << int(Change::Complete);
    QTest::newRow("Hold at end") << hold << true << true << int(Change::EndHold);
    QTest::newRow("Position at end") << position << true << true << int(Change::Cancel);
    QTest::newRow("Return mid-grid") << rtl << true << false << int(Change::Cancel);
    QTest::newRow("Hold mid-grid") << hold << true << false << int(Change::Cancel);
    QTest::newRow("Position mid-grid") << position << true << false << int(Change::Cancel);
    QTest::newRow("Land mid-grid") << QStringLiteral("Land") << true << false << int(Change::Cancel);
    // Before Mission is reached the current item is the previous mission's, so the end rules don't apply
    QTest::newRow("Return before start") << rtl << false << true << int(Change::Cancel);
    QTest::newRow("Hold before start") << hold << false << true << int(Change::Cancel);
    QTest::newRow("Position before start") << position << false << false << int(Change::Cancel);
}

void AutoGridTest::_classifyFlightModeChange()
{
    QFETCH(QString, flightMode);
    QFETCH(bool, missionStarted);
    QFETCH(bool, atMissionEnd);
    QFETCH(int, expected);

    const auto change = AutoGridStateMachine::classifyFlightModeChange(
        flightMode, missionStarted, atMissionEnd, QStringLiteral("Mission"), QStringLiteral("Safe Recovery"),
        QStringLiteral("Hold"));
    QCOMPARE(int(change), expected);
}

void AutoGridTest::_isAtMissionEnd()
{
    GridGenerator::Params params;
    params.origin = QGeoCoordinate(47.0, 8.0);
    params.altitudeM = 30.0;
    const GridGenerator::Result result = GridGenerator::generate(params);
    QVERIFY(result.isValid());

    const int loiterSeq = gridItemSeq(result, MAV_CMD_NAV_LOITER_TIME);
    const int rtlSeq = gridItemSeq(result, MAV_CMD_NAV_RETURN_TO_LAUNCH);
    QCOMPARE(loiterSeq, int(result.items.count()) - 2);
    QCOMPARE(rtlSeq, int(result.items.count()) - 1);

    QVERIFY(!AutoGridStateMachine::isAtMissionEnd(result, -1));
    QVERIFY(!AutoGridStateMachine::isAtMissionEnd(result, 0));
    QVERIFY(!AutoGridStateMachine::isAtMissionEnd(result, loiterSeq - 1));
    QVERIFY(AutoGridStateMachine::isAtMissionEnd(result, loiterSeq));
    QVERIFY(AutoGridStateMachine::isAtMissionEnd(result, rtlSeq));
    QVERIFY(AutoGridStateMachine::isAtMissionEnd(result, rtlSeq + 1));
    QVERIFY(!AutoGridStateMachine::isAtMissionEnd(GridGenerator::Result(), 100));
}

void AutoGridTest::_latchParams()
{
    QVERIFY(waitForParametersReady());
    // Vehicle heading is truncated to whole degrees; bias by half a degree so float error can't drop it by one
    _mockLink->setVehicleAttitudeOverrideDeg(0.0f, 0.0f, 200.5f);
    const auto restoreAttitude = qScopeGuard([this]() { _mockLink->clearVehicleAttitudeOverride(); });
    QTRY_COMPARE_WITH_TIMEOUT(_vehicle->heading()->rawValue().toDouble(), 200.0, 5000);

    CustomSettings* settings = autoGridTestPlugin()->customSettings();
    const GridGenerator::Params params = AutoGridStateMachine::latchParams(_vehicle, settings);

    QCOMPARE(params.origin, _vehicle->coordinate());
    QCOMPARE(params.headingDeg, 200.0);
    QCOMPARE(params.altitudeM, _vehicle->altitudeRelative()->rawValue().toDouble());
    QCOMPARE(params.widthM, settings->gridWidth()->rawValue().toDouble());
    QCOMPARE(params.depthM, settings->gridDepth()->rawValue().toDouble());
    QCOMPARE(params.spacingM, settings->gridLaneSpacing()->rawValue().toDouble());
    QCOMPARE(params.cruiseSpeedMS, settings->gridCruiseSpeed()->rawValue().toDouble());
    QCOMPARE(params.turnSpeedMS, settings->gridTurnSpeed()->rawValue().toDouble());
    QCOMPARE(params.arcStepDeg, settings->gridArcStep()->rawValue().toDouble());
    QCOMPARE(params.endLoiterS, settings->gridEndLoiter()->rawValue().toDouble());
    QCOMPARE(params.acceptanceRadiusM, settings->gridAcceptanceRadius()->rawValue().toDouble());
    // PX4MockLink.params has MPC_XY_VEL_MAX 12
    QCOMPARE(params.maxSpeedMS, 12.0);
}

void AutoGridTest::_uploadsSetsCurrentAndStartsMission()
{
    _mockLink->clearReceivedMavlinkMessageCounts();
    _mockLink->clearReceivedMavCommandCounts();

    QPointer<AutoGridStateMachine> machine = _startMonitoringGrid();
    QVERIFY(machine);
    const auto cleanupMachine = qScopeGuard([&machine]() { delete machine.data(); });
    const GridGenerator::Result& result = machine->result();

    // The vehicle got exactly the generated items, in order, without the home placeholder
    const QList<MissionItem*>& written = _vehicle->missionManager()->missionItems();
    QCOMPARE(written.count(), result.items.count());
    for (qsizetype i = 0; i < written.count(); i++) {
        const GridGenerator::Item& expected = result.items[i];
        const MissionItem* item = written[i];
        QCOMPARE(item->sequenceNumber(), expected.seq);
        QCOMPARE(item->command(), expected.command);
        QCOMPARE(item->frame(), expected.frame);
        QCOMPARE(item->param1(), expected.param1);
        QCOMPARE(item->param2(), expected.param2);
        QCOMPARE(item->param4(), expected.param4);
        QCOMPARE(item->param5(), expected.param5);
        QCOMPARE(item->param6(), expected.param6);
        QCOMPARE(item->param7(), expected.param7);
    }
    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_ITEM_INT), int(result.items.count()));
    mavlink_message_t message;
    QVERIFY(_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_MISSION_COUNT, message));
    QCOMPARE(int(mavlink_msg_mission_count_get_count(&message)), int(result.items.count()));

    // Wire item 0 made current: the command, or its MISSION_SET_CURRENT fallback when unsupported
    // (MockLink's PX4 gets the message: QGC treats the command as unsupported there)
    QTRY_VERIFY_WITH_TIMEOUT(_mockLink->receivedMavCommandCount(MAV_CMD_DO_SET_MISSION_CURRENT) +
                                     _mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_SET_CURRENT) ==
                                 1,
                             15000);
    if (_mockLink->lastReceivedMavlinkMessage(MAVLINK_MSG_ID_MISSION_SET_CURRENT, message)) {
        QCOMPARE(int(mavlink_msg_mission_set_current_get_seq(&message)), 0);
    }

    QCOMPARE(_vehicle->flightMode(), _vehicle->missionFlightMode());
    QVERIFY(machine->inProgress());
}

void AutoGridTest::_endOfMission_data()
{
    QTest::addColumn<int>("currentItem");
    QTest::addColumn<QStringList>("flightModes");  // "hold" and "rtl" are the vehicle's own mode names
    QTest::addColumn<bool>("complete");

    QTest::newRow("Return at end loiter") << kAtLoiterItem << QStringList{"rtl"} << true;
    QTest::newRow("Return at RTL item") << kAtRtlItem << QStringList{"rtl"} << true;
    QTest::newRow("transient end Hold, then Return") << kAtLoiterItem << QStringList{"hold", "rtl"} << true;
    QTest::newRow("Position mid-grid") << kMidGridItem << QStringList{"Position"} << false;
    QTest::newRow("Hold mid-grid") << kMidGridItem << QStringList{"hold"} << false;
    QTest::newRow("Return mid-grid") << kMidGridItem << QStringList{"rtl"} << false;
    QTest::newRow("Position during end loiter") << kAtLoiterItem << QStringList{"Position"} << false;
    QTest::newRow("end Hold, then Position") << kAtLoiterItem << QStringList{"hold", "Position"} << false;
}

void AutoGridTest::_endOfMission()
{
    QFETCH(int, currentItem);
    QFETCH(QStringList, flightModes);
    QFETCH(bool, complete);

    QPointer<AutoGridStateMachine> machine = _startMonitoringGrid();
    QVERIFY(machine);
    const auto cleanupMachine = qScopeGuard([&machine]() { delete machine.data(); });

    if (currentItem == kAtLoiterItem) {
        currentItem = gridItemSeq(machine->result(), MAV_CMD_NAV_LOITER_TIME);
    } else if (currentItem == kAtRtlItem) {
        currentItem = gridItemSeq(machine->result(), MAV_CMD_NAV_RETURN_TO_LAUNCH);
    }
    _sendMissionCurrent(currentItem);

    QSignalSpy finishedSpy(machine.data(), &QStateMachine::finished);
    QSignalSpy stoppedSpy(machine.data(), &QStateMachine::stopped);
    for (const QString& flightMode : flightModes) {
        QVERIFY(machine);
        const QString mode = (flightMode == QStringLiteral("hold"))  ? _vehicle->pauseFlightMode()
                             : (flightMode == QStringLiteral("rtl")) ? _vehicle->rtlFlightMode()
                                                                     : flightMode;
        emit _vehicle->flightModeChanged(mode);
    }

    if (complete) {
        QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty(), 5000);
        QVERIFY(stoppedSpy.isEmpty());
    } else {
        // A cancelled machine deletes itself
        QTRY_VERIFY_WITH_TIMEOUT(machine.isNull(), 5000);
        QVERIFY(finishedSpy.isEmpty());
    }
}

void AutoGridTest::_cancelSendsNothingMore()
{
    QPointer<AutoGridStateMachine> machine = _startMonitoringGrid();
    QVERIFY(machine);
    const auto cleanupMachine = qScopeGuard([&machine]() { delete machine.data(); });

    _mockLink->clearReceivedMavlinkMessageCounts();
    _mockLink->clearReceivedMavCommandCounts();

    emit _vehicle->flightModeChanged(QStringLiteral("Position"));
    QTRY_VERIFY_WITH_TIMEOUT(machine.isNull(), 5000);

    // A late mode report must not wake anything up either
    emit _vehicle->flightModeChanged(_vehicle->rtlFlightMode());

    QSignalSpy sendSpy(_vehicle->missionManager(), &MissionManager::sendComplete);
    QVERIFY(!sendSpy.wait(2000));

    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_SET_MODE), 0);
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_SET_MODE), 0);
    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_COUNT), 0);
    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_ITEM_INT), 0);
    QCOMPARE(_mockLink->receivedMavlinkMessageCount(MAVLINK_MSG_ID_MISSION_SET_CURRENT), 0);
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_SET_MISSION_CURRENT), 0);
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_NAV_RETURN_TO_LAUNCH), 0);
    QCOMPARE(_mockLink->receivedMavCommandCount(MAV_CMD_DO_REPOSITION), 0);
}

void AutoGridTest::_gridAndRotationExclusive()
{
    _takeoff();
    CustomPlugin* plugin = autoGridTestPlugin();
    QVERIFY(plugin);
    QVERIFY(!plugin->gridInProgress());
    QVERIFY(!plugin->rotationInProgress());

    // Grid first: every rotation entry point is refused
    QSignalSpy gridSpy(plugin, &CustomPlugin::gridInProgressChanged);
    plugin->startAutoGrid();
    QVERIFY(plugin->gridInProgress());
    QCOMPARE(gridSpy.count(), 1);

    expectAppMessage(QRegularExpression(QStringLiteral("A grid is in progress")));
    plugin->startRotation();
    verifyExpectedLogMessage();
    expectAppMessage(QRegularExpression(QStringLiteral("A grid is in progress")));
    plugin->calibrationSweep();
    verifyExpectedLogMessage();
    expectAppMessage(QRegularExpression(QStringLiteral("A grid is already in progress")));
    plugin->startAutoGrid();
    verifyExpectedLogMessage();
    QVERIFY(!plugin->rotationInProgress());

    // A pilot mode change ends the grid and frees the vehicle for a rotation
    emit _vehicle->flightModeChanged(QStringLiteral("Position"));
    QTRY_VERIFY_WITH_TIMEOUT(!plugin->gridInProgress(), 5000);

    // Rotation first: the grid is refused. The fixture has no tags, which the rotation's first states report.
    ignoreLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, QRegularExpression(QStringLiteral("^No tags")));
    plugin->startRotation();
    QVERIFY(plugin->rotationInProgress());
    expectAppMessage(QRegularExpression(QStringLiteral("A rotation is in progress")));
    plugin->startAutoGrid();
    verifyExpectedLogMessage();
    QVERIFY(!plugin->gridInProgress());

    plugin->pauseRotation();
    QTRY_VERIFY_WITH_TIMEOUT(!plugin->rotationInProgress(), 5000);
}

UT_REGISTER_TEST(AutoGridTest, TestLabel::Unit)
