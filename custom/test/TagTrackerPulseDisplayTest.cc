#include "TagTrackerPulseDisplayTest.h"

#include "DetectorInfo.h"
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

void TagTrackerPulseDisplayTest::_confirmedMeasurementReplacesPriorSliceValue()
{
    SliceInfo sliceInfo(0, 0.0, 45.0);

    sliceInfo.updateMaxSNR(66.0, true, QStringLiteral("R"));
    sliceInfo.updateMaxSNR(60.0, true, QStringLiteral("R"));

    // A confirmed re-measurement supersedes the provisional value; it is not a max.
    QCOMPARE(sliceInfo.displaySNR(), 60.0);
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

UT_REGISTER_TEST(TagTrackerPulseDisplayTest, TestLabel::Unit)
