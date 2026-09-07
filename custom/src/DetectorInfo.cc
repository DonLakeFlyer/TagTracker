#include "DetectorInfo.h"

#include <algorithm>

QGC_LOGGING_CATEGORY(DetectorInfoLog, "DetectorInfoLog")

DetectorInfo::DetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k,
                           QObject* parent)
    : QObject(parent), _tagId(tagId), _tagLabel(tagLabel)
{
    _heartbeatTimerInterval = ((k + 1) * intraPulseMsecs) + 1000;

    qCDebug(DetectorInfoLog) << "tagId:tagLabel:intraPulseMsecs:k:heartbeatInterval" << _tagId << _tagLabel
                             << intraPulseMsecs << k << _heartbeatTimerInterval;

    _heartbeatTimeoutTimer.setSingleShot(true);
    _heartbeatTimeoutTimer.setInterval(_heartbeatTimerInterval);
    _heartbeatTimeoutTimer.callOnTimeout(this, [this]() {
        _heartbeatLost = true;
        emit heartbeatLostChanged();
    });
}

void DetectorInfo::startHeartbeatWatchdog()
{
    _watchdogArmed = true;
    // A detector that never sends its first heartbeat must still be flagged, but the
    // controller may take up to 30 s to bring Python detectors to READY before the first one.
    constexpr uint32_t kStartupGraceMsecs = 35000;
    _heartbeatTimeoutTimer.start(static_cast<int>(std::max(_heartbeatTimerInterval, kStartupGraceMsecs)));
}

void DetectorInfo::_heartbeatReceived()
{
    qCDebug(DetectorInfoLog) << "HEARTBEAT from Detector id" << _tagId;
    // Stale heartbeats from a prior collection's detector must not arm the watchdog early
    if (!_watchdogArmed) {
        return;
    }
    _heartbeatTimeoutTimer.start(static_cast<int>(_heartbeatTimerInterval));
    if (_heartbeatLost) {
        _heartbeatLost = false;
        emit heartbeatLostChanged();
    }
}

void DetectorInfo::_pulseReceived(double snr, bool lowConfidence, bool newGroup)
{
    const double clampedSNR = qIsNaN(snr) ? 0.0 : std::max(0.0, snr);
    _lastPulseStrength = newGroup ? clampedSNR : std::max(clampedSNR, _lastPulseStrength);

    if (_lastPulseLowConfidence != lowConfidence) {
        _lastPulseLowConfidence = lowConfidence;
        emit lastPulseLowConfidenceChanged();
    }
    if (_waitingForFirstPulse) {
        _waitingForFirstPulse = false;
        emit waitingForFirstPulseChanged();
    }
    if (_lastPulseNoPulse) {
        _lastPulseNoPulse = false;
        emit lastPulseNoPulseChanged();
    }
    if (_noPulseCount > 0) {
        _noPulseCount = 0;
        emit noPulseCountChanged();
    }

    emit lastPulseStrengthChanged();
}

void DetectorInfo::_noPulseReceived()
{
    qCDebug(DetectorInfoLog) << "NO_PULSE from Detector id" << _tagId;
    if (!_lastPulseNoPulse) {
        _lastPulseNoPulse = true;
        emit lastPulseNoPulseChanged();
    }
    _noPulseCount++;
    emit noPulseCountChanged();
    if (_waitingForFirstPulse) {
        _waitingForFirstPulse = false;
        emit waitingForFirstPulseChanged();
    }
}

void DetectorInfo::_setRateLabel(const QString& rateLabel)
{
    if (_rateLabel != rateLabel) {
        _rateLabel = rateLabel;
        emit rateLabelChanged();
    }
}
