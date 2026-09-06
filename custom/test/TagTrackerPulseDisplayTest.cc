#include "TagTrackerPulseDisplayTest.h"

#include "DetectorInfo.h"
#include "PythonRotateAndCaptureState.h"
#include "RotationInfo.h"
#include "SetFlightModeState.h"
#include "SliceInfo.h"

void TagTrackerPulseDisplayTest::_pythonUsesSignalPower()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.snr = 0.0;
    pulseInfo.group_snr = 1.0e-10;
    pulseInfo.noise_psd = 1.0e-10;

    // 0 dB above noise; must still be driven by group_snr, not snr.
    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, true), 0.0);
    pulseInfo.group_snr = 1.0e-9;
    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, true), 10.0);
}

void TagTrackerPulseDisplayTest::_legacyUsesSNR()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.snr = 12.5;
    pulseInfo.group_snr = 0.0025;

    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, false), 12.5);
}

void TagTrackerPulseDisplayTest::_pythonDisplaysDbAboveNoise()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.snr = 0.0;
    pulseInfo.group_snr = 1.0e-6;
    pulseInfo.noise_psd = 1.0e-10;

    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, true), 40.0);
}

void TagTrackerPulseDisplayTest::_pythonNonPositivePowerDisplaysZero()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.group_snr = -1.9e-10;
    pulseInfo.noise_psd = 1.0e-10;

    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, true), 0.0);
}

void TagTrackerPulseDisplayTest::_pythonInvalidNoiseIsNotAMeasurement()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.group_snr = 1.0e-9;
    pulseInfo.noise_psd = 0.0;

    QVERIFY(qIsNaN(RotationInfo::pulseStrengthForDisplay(pulseInfo, true)));
}

void TagTrackerPulseDisplayTest::_pythonBelowNoiseDisplaysNegativeDb()
{
    TunnelProtocol::PulseInfo_t pulseInfo {};
    pulseInfo.group_snr = 1.0e-11;
    pulseInfo.noise_psd = 1.0e-10;

    // group_snr is noise-subtracted, so a positive value below noise_psd is still a weak detection
    QCOMPARE(RotationInfo::pulseStrengthForDisplay(pulseInfo, true), -10.0);
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
    DetectorInfo detectorInfo(2, QStringLiteral("Test tag"), 1333, 20);

    QVERIFY(!detectorInfo.property("heartbeatLost").toBool());
    QVERIFY(detectorInfo.property("waitingForFirstPulse").toBool());
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

UT_REGISTER_TEST(TagTrackerPulseDisplayTest, TestLabel::Unit)
