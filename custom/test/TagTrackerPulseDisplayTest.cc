#include "TagTrackerPulseDisplayTest.h"

#include "PythonDetectorInfo.h"
#include "PythonRotateAndCaptureState.h"
#include "RotationInfo.h"
#include "SetFlightModeState.h"
#include "SliceInfo.h"
#include "TagDatabase.h"
#include "TunnelProtocol.h"

#include <QtTest/QSignalSpy>

#include <limits>

void TagTrackerPulseDisplayTest::_pythonUsesSignalPower()
{
    // 0 dB above noise; must be driven by signal_psd, not snr.
    QCOMPARE(RotationInfo::displayStrength(1.0e-10, 1.0e-10), 0.0);
    QCOMPARE(RotationInfo::displayStrength(1.0e-9, 1.0e-10), 10.0);
}

void TagTrackerPulseDisplayTest::_pythonDisplaysDbAboveNoise()
{
    QCOMPARE(RotationInfo::displayStrength(1.0e-6, 1.0e-10), 40.0);
}

void TagTrackerPulseDisplayTest::_pythonNonPositivePowerDisplaysZero()
{
    QCOMPARE(RotationInfo::displayStrength(-1.9e-10, 1.0e-10), 0.0);
}

void TagTrackerPulseDisplayTest::_pythonInvalidNoiseIsNotAMeasurement()
{
    QVERIFY(qIsNaN(RotationInfo::displayStrength(1.0e-9, 0.0)));
}

void TagTrackerPulseDisplayTest::_pythonBelowNoiseDisplaysNegativeDb()
{
    // signal_psd is noise-subtracted, so a positive value below noise_psd is still a weak detection
    QCOMPARE(RotationInfo::displayStrength(1.0e-11, 1.0e-10), -10.0);
}

void TagTrackerPulseDisplayTest::_rateLabel_data()
{
    QTest::addColumn<QString>("rateA");
    QTest::addColumn<QString>("rateB");
    QTest::addColumn<int>("rateState");
    QTest::addColumn<bool>("abbreviated");
    QTest::addColumn<QString>("expected");

    using namespace TunnelProtocol;
    QTest::newRow("A full")           << "Resting" << "Moving" << int(kRateStateA)    << false << "Resting";
    QTest::newRow("B full")           << "Resting" << "Moving" << int(kRateStateB)    << false << "Moving";
    QTest::newRow("A abbreviated")    << "Resting" << "Moving" << int(kRateStateA)    << true  << "R";
    QTest::newRow("B abbreviated")    << "Resting" << "Moving" << int(kRateStateB)    << true  << "M";
    QTest::newRow("A->B always pair") << "Resting" << "Moving" << int(kRateStateAToB) << false << "R/M";
    QTest::newRow("B->A always pair") << "Resting" << "Moving" << int(kRateStateBToA) << true  << "M/R";
    QTest::newRow("empty names fall back to 1/2") << "" << "" << int(kRateStateAToB) << false << "1/2";
    QTest::newRow("empty A full")     << ""        << ""       << int(kRateStateA)    << false << "1";
    QTest::newRow("unknown state -> A") << "Resting" << "Moving" << 42                 << false << "Resting";
}

void TagTrackerPulseDisplayTest::_rateLabel()
{
    QFETCH(QString, rateA);
    QFETCH(QString, rateB);
    QFETCH(int, rateState);
    QFETCH(bool, abbreviated);
    QFETCH(QString, expected);

    QCOMPARE(TagDatabase::rateLabel(rateA, rateB, static_cast<uint8_t>(rateState), abbreviated), expected);
}

void TagTrackerPulseDisplayTest::_confirmedMeasurementReplacesPriorSliceValue()
{
    SliceInfo sliceInfo(0, 0.0, 45.0);

    sliceInfo.updateMaxSNR(1, 66.0, true, QStringLiteral("R"));
    sliceInfo.updateMaxSNR(1, 60.0, true, QStringLiteral("R"));

    // A confirmed re-measurement supersedes the provisional value; it is not a max.
    QCOMPARE(sliceInfo.displaySNR(), 60.0);
}

void TagTrackerPulseDisplayTest::_confirmedMeasurementsAggregateAcrossTags()
{
    SliceInfo sliceInfo(0, 0.0, 45.0);

    sliceInfo.updateMaxSNR(1, 66.0, true, QStringLiteral("R"));
    sliceInfo.updateMaxSNR(2, 50.0, true, QStringLiteral("M"));
    QCOMPARE(sliceInfo.displaySNR(), 66.0);
    QCOMPARE(sliceInfo.displaySource(), QStringLiteral("R"));

    // Re-measuring tag 1 only replaces tag 1; tag 2 is untouched and the max is re-derived
    sliceInfo.updateMaxSNR(1, 40.0, true, QStringLiteral("R"));
    QCOMPARE(sliceInfo.displaySNR(), 50.0);
    QCOMPARE(sliceInfo.displaySource(), QStringLiteral("M"));
}

void TagTrackerPulseDisplayTest::_startupWaitIsNotHeartbeatFailure()
{
    PythonDetectorInfo detectorInfo(2, QStringLiteral("Test tag"), 1333, 20);

    QVERIFY(!detectorInfo.property("heartbeatLost").toBool());
    QVERIFY(detectorInfo.property("waitingForFirstPulse").toBool());
}

void TagTrackerPulseDisplayTest::_heartbeatWatchdogArmsOnlyWhenStarted()
{
    // Python detectors are created before takeoff but only start heartbeating after
    // START_COLLECTION, so the watchdog must stay disarmed until explicitly started.
    constexpr uint32_t intraPulseMsecs = 1333;
    constexpr uint32_t k = 3;
    constexpr int cadenceTimeoutMsecs = (k + 1) * intraPulseMsecs + 1000;
    constexpr int startupGraceMsecs = 35000;
    // Coarse QTimer may round the deadline by up to 5%
    constexpr auto withSlack = [](int msecs) { return msecs + msecs / 20; };

    PythonDetectorInfo detectorInfo(2, QStringLiteral("Test tag"), intraPulseMsecs, k);
    QVERIFY(!detectorInfo.heartbeatWatchdogActive());

    // A stale heartbeat before arming must not start the timer
    TunnelProtocol::PythonPulseInfo_t heartbeat {};
    heartbeat.tag_id = 2;
    heartbeat.frequency_hz = 0;
    detectorInfo.handlePulse(heartbeat);
    QVERIFY(!detectorInfo.heartbeatWatchdogActive());

    detectorInfo.startHeartbeatWatchdog();
    QVERIFY(detectorInfo.heartbeatWatchdogActive());
    QVERIFY(detectorInfo.heartbeatWatchdogRemainingMsecs() > withSlack(cadenceTimeoutMsecs));
    QVERIFY(detectorInfo.heartbeatWatchdogRemainingMsecs() <= withSlack(startupGraceMsecs));

    // First heartbeat drops the watchdog to the tag cadence
    detectorInfo.handlePulse(heartbeat);
    QVERIFY(detectorInfo.heartbeatWatchdogActive());
    QVERIFY(detectorInfo.heartbeatWatchdogRemainingMsecs() <= withSlack(cadenceTimeoutMsecs));
    QVERIFY(!detectorInfo.property("heartbeatLost").toBool());
}

void TagTrackerPulseDisplayTest::_flightModeChangeTimeoutToleratesSlowLink()
{
    // A mode-change ack over a lossy radio, or a slow SITL clock, easily
    // exceeds 2 s; a false timeout here triggers RTLOnError mid-flight.
    QVERIFY(SetFlightModeState::kTimeoutMsecs >= 10000);
}

void TagTrackerPulseDisplayTest::_sliceVisitOrder_data()
{
    QTest::addColumn<int>("divisions");
    QTest::addColumn<double>("priorBearingDeg");
    QTest::addColumn<double>("antennaOffsetDeg");
    QTest::addColumn<QList<int>>("expected");

    QTest::newRow("no prior, 8 slices clockwise")    << 8  << qQNaN() << 0.0   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7};
    QTest::newRow("no prior, 16 slices clockwise")   << 16 << qQNaN() << 0.0   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    QTest::newRow("prior north starts at 0")         << 8  << 0.0     << 0.0   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7};
    QTest::newRow("prior east starts at 2")          << 8  << 90.0    << 0.0   << QList<int>{2, 3, 4, 5, 6, 7, 0, 1};
    QTest::newRow("prior rounds to nearest slice")   << 8  << 100.0   << 0.0   << QList<int>{2, 3, 4, 5, 6, 7, 0, 1};
    QTest::newRow("antenna offset shifts first")     << 8  << 90.0    << 45.0  << QList<int>{3, 4, 5, 6, 7, 0, 1, 2};
    QTest::newRow("negative sum wraps below 0")      << 8  << 10.0    << -45.0 << QList<int>{7, 0, 1, 2, 3, 4, 5, 6};
    QTest::newRow("near 360 wraps to slice 0")       << 8  << 350.0   << 0.0   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7};
    QTest::newRow("sum past 360 wraps")              << 8  << 350.0   << 45.0  << QList<int>{1, 2, 3, 4, 5, 6, 7, 0};
}

void TagTrackerPulseDisplayTest::_sliceVisitOrder()
{
    QFETCH(int, divisions);
    QFETCH(double, priorBearingDeg);
    QFETCH(double, antennaOffsetDeg);
    QFETCH(QList<int>, expected);

    QCOMPARE(PythonRotateAndCaptureState::sliceVisitOrder(divisions, priorBearingDeg, antennaOffsetDeg), expected);
}

void TagTrackerPulseDisplayTest::_bearingResultValidity_data()
{
    QTest::addColumn<float>("bearingDeg");
    QTest::addColumn<uint32_t>("nValidSlices");
    QTest::addColumn<bool>("expectedValid");

    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    QTest::newRow("finite, enough slices")  << 123.4f << 3u << true;
    // The controller applies its confidence floor; one detected heading is still a bearing.
    QTest::newRow("finite, one slice")      << 123.4f << 1u << true;
    QTest::newRow("finite, no slices")      << 123.4f << 0u << false;
    QTest::newRow("NaN sentinel")           << nan    << 5u << false;
    QTest::newRow("+Inf")                   << inf    << 5u << false;
    QTest::newRow("-Inf")                   << -inf   << 5u << false;
}

void TagTrackerPulseDisplayTest::_bearingResultValidity()
{
    QFETCH(float, bearingDeg);
    QFETCH(uint32_t, nValidSlices);
    QFETCH(bool, expectedValid);

    RotationInfo rotationInfo(8);
    QSignalSpy bearingSpy(&rotationInfo, &RotationInfo::bearingChanged);

    rotationInfo.setBearingResult(bearingDeg, 0.9f, nValidSlices, 20.0f, /*confirmed*/ false);

    QCOMPARE(rotationInfo.bearingValid(), expectedValid);
    QCOMPARE(rotationInfo.bearingConfirmed(), false);
    QCOMPARE(bearingSpy.count(), 1);
    if (expectedValid) {
        QCOMPARE(rotationInfo.bearingDeg(), static_cast<double>(bearingDeg));
        QCOMPARE(rotationInfo.bearingRSquared(), static_cast<double>(0.9f));
    }

    // Confirmed-only change must still notify the UI; confirmation never survives an invalid bearing
    rotationInfo.setBearingResult(bearingDeg, 0.9f, nValidSlices, 20.0f, /*confirmed*/ true);
    QCOMPARE(rotationInfo.bearingConfirmed(), expectedValid);
    QCOMPARE(bearingSpy.count(), 2);
}

void TagTrackerPulseDisplayTest::_bearingState()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    RotationInfo waiting(8);
    QVERIFY(!waiting.bearingReceived());
    QCOMPARE(waiting.bearingState(), RotationInfo::NothingHeard);
    QCOMPARE(waiting.bearingSector(), -1);

    RotationInfo nothing(8);
    nothing.setBearingResult(nan, 0.0f, 0u, 0.0f, false);
    QVERIFY(nothing.bearingReceived());
    QCOMPARE(nothing.bearingState(), RotationInfo::NothingHeard);
    QCOMPARE(nothing.bearingStateText(), QStringLiteral("nothing heard"));
    QCOMPARE(nothing.bearingSector(), -1);

    // Candidates seen but none fitted the pattern: bearing is NaN even with detections
    RotationInfo rejected(8);
    rejected.setBearingResult(nan, 0.1f, 4u, 12.0f, false);
    QCOMPARE(rejected.bearingState(), RotationInfo::NothingHeard);

    RotationInfo unconfirmed(8);
    unconfirmed.setBearingResult(100.0f, 0.5f, 1u, 12.0f, false);
    QCOMPARE(unconfirmed.bearingState(), RotationInfo::Unconfirmed);
    QCOMPARE(unconfirmed.bearingStateText(), QStringLiteral("unconfirmed"));
    QCOMPARE(unconfirmed.bearingSector(), 2);

    RotationInfo confirmed(8);
    confirmed.setBearingResult(100.0f, 0.9f, 3u, 20.0f, true);
    QCOMPARE(confirmed.bearingState(), RotationInfo::Confirmed);
    QCOMPARE(confirmed.bearingStateText(), QStringLiteral("confirmed"));
    QCOMPARE(confirmed.bearingSector(), 2);
}

void TagTrackerPulseDisplayTest::_bearingSector_data()
{
    QTest::addColumn<double>("headingDeg");
    QTest::addColumn<int>("sliceCount");
    QTest::addColumn<int>("expectedSector");

    // 8 slices: centres every 45 deg, boundaries at 22.5 + 45 n
    QTest::newRow("8: on-grid 0")         << 0.0    << 8 << 0;
    QTest::newRow("8: on-grid 90")        << 90.0   << 8 << 2;
    QTest::newRow("8: on-grid 315")       << 315.0  << 8 << 7;
    QTest::newRow("8: just below edge")   << 22.4   << 8 << 0;
    QTest::newRow("8: just above edge")   << 22.6   << 8 << 1;
    QTest::newRow("8: wraps to 0")        << 359.0  << 8 << 0;
    QTest::newRow("8: 330 is last slice") << 330.0  << 8 << 7;
    QTest::newRow("8: negative input")    << -45.0  << 8 << 7;
    QTest::newRow("8: over 360")          << 405.0  << 8 << 1;
    // 16 slices: centres every 22.5 deg
    QTest::newRow("16: on-grid 22.5")     << 22.5   << 16 << 1;
    QTest::newRow("16: 100")              << 100.0  << 16 << 4;
    QTest::newRow("16: wraps to 0")       << 355.0  << 16 << 0;
    QTest::newRow("no slices")            << 90.0   << 0  << -1;
    QTest::newRow("NaN heading")          << std::numeric_limits<double>::quiet_NaN() << 8 << -1;
}

void TagTrackerPulseDisplayTest::_bearingSector()
{
    QFETCH(double, headingDeg);
    QFETCH(int, sliceCount);
    QFETCH(int, expectedSector);

    QCOMPARE(RotationInfo::sectorForHeading(headingDeg, sliceCount), expectedSector);
}

void TagTrackerPulseDisplayTest::_outcomeAnnouncement()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    QCOMPARE(PythonRotateAndCaptureState::outcomeAnnouncement(nullptr),
             QStringLiteral("Rotation detection complete"));

    RotationInfo waiting(8);
    QCOMPARE(PythonRotateAndCaptureState::outcomeAnnouncement(&waiting),
             QStringLiteral("Rotation detection complete"));

    RotationInfo nothing(8);
    nothing.setBearingResult(nan, 0.0f, 0u, 0.0f, false);
    QCOMPARE(PythonRotateAndCaptureState::outcomeAnnouncement(&nothing),
             QStringLiteral("Rotation complete. Nothing heard"));

    RotationInfo unconfirmed(8);
    unconfirmed.setBearingResult(134.6f, 0.5f, 1u, 12.0f, false);
    QCOMPARE(PythonRotateAndCaptureState::outcomeAnnouncement(&unconfirmed),
             QStringLiteral("Rotation complete. Unconfirmed, sector 4, bearing 135 degrees"));

    RotationInfo confirmed(8);
    confirmed.setBearingResult(134.6f, 0.9f, 3u, 20.0f, true);
    QCOMPARE(PythonRotateAndCaptureState::outcomeAnnouncement(&confirmed),
             QStringLiteral("Rotation complete. Tag confirmed, sector 4, bearing 135 degrees"));
}

UT_REGISTER_TEST(TagTrackerPulseDisplayTest, TestLabel::Unit)
