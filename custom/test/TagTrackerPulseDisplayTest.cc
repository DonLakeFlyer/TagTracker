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

    QTest::newRow("no prior, 8 slices spreads")      << 8  << qQNaN() << 0.0   << QList<int>{0, 2, 4, 6, 1, 3, 5, 7};
    QTest::newRow("no prior, 16 slices sequential")  << 16 << qQNaN() << 0.0   << QList<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    QTest::newRow("prior north alternates outward")  << 8  << 0.0     << 0.0   << QList<int>{0, 1, 7, 2, 6, 3, 5, 4};
    QTest::newRow("prior east")                      << 8  << 90.0    << 0.0   << QList<int>{2, 3, 1, 4, 0, 5, 7, 6};
    QTest::newRow("prior rounds to nearest slice")   << 8  << 100.0   << 0.0   << QList<int>{2, 3, 1, 4, 0, 5, 7, 6};
    QTest::newRow("antenna offset shifts first")     << 8  << 90.0    << 45.0  << QList<int>{3, 4, 2, 5, 1, 6, 0, 7};
    QTest::newRow("negative sum wraps below 0")      << 8  << 10.0    << -45.0 << QList<int>{7, 0, 6, 1, 5, 2, 4, 3};
    QTest::newRow("near 360 wraps to slice 0")       << 8  << 350.0   << 0.0   << QList<int>{0, 1, 7, 2, 6, 3, 5, 4};
    QTest::newRow("sum past 360 wraps")              << 8  << 350.0   << 45.0  << QList<int>{1, 2, 0, 3, 7, 4, 6, 5};
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
    QTest::newRow("finite, too few slices") << 123.4f << 2u << false;
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

UT_REGISTER_TEST(TagTrackerPulseDisplayTest, TestLabel::Unit)
