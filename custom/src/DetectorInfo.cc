#include "DetectorInfo.h"
#include "CustomPlugin.h"
#include "Vehicle.h"
#include "CustomSettings.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "AppSettings.h"
#include "FlyViewSettings.h"
#include "QmlComponentInfo.h"
#include "TunnelProtocol.h"
#include <QDebug>
#include <QPointF>
#include <QLineF>
#include <QQmlEngine>

using namespace TunnelProtocol;

QGC_LOGGING_CATEGORY(DetectorInfoLog, "DetectorInfoLog")

DetectorInfo::DetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k, QObject* parent)
    : QObject           (parent)
    , _tagId            (tagId)
    , _tagLabel         (tagLabel)
    , _intraPulseMsecs  (intraPulseMsecs)
    , _k                (k)
{
    _heartbeatTimerInterval = ((_k + 1) * intraPulseMsecs) + 1000;

    qDebug() << "DetectorInfo::DetectorInfo" << _tagId << _tagLabel << _intraPulseMsecs << _k << _heartbeatTimerInterval;

    _heartbeatTimeoutTimer.setSingleShot(true);
    _heartbeatTimeoutTimer.setInterval(_heartbeatTimerInterval);
    _heartbeatTimeoutTimer.callOnTimeout([this]() {
        _heartbeatLost = true;
        emit heartbeatLostChanged();
    });

    _stalePulseStrengthTimer.setSingleShot(true);
    _stalePulseStrengthTimer.setInterval(_heartbeatTimerInterval);
    _stalePulseStrengthTimer.callOnTimeout([this]() {
        _lastPulseStale = true;
        emit lastPulseStaleChanged();
        if (_lastPulseLowConfidence) {
            _lastPulseLowConfidence = false;
            emit lastPulseLowConfidenceChanged();
        }
        if (_lastPulseNoPulse) {
            _lastPulseNoPulse = false;
            emit lastPulseNoPulseChanged();
        }
        if (_noPulseCount > 0) {
            _noPulseCount = 0;
            emit noPulseCountChanged();
        }
    });
}

DetectorInfo::~DetectorInfo()
{

}

void DetectorInfo::handleTunnelPulse(const mavlink_tunnel_t& tunnel)
{
    if (tunnel.payload_length != sizeof(PulseInfo_t)) {
        qWarning() << "handleTunnelPulse Received incorrectly sized PulseInfo payload expected:actual" <<  sizeof(PulseInfo_t) << tunnel.payload_length;
        return;
    }

    PulseInfo_t pulseInfo;
    memcpy(&pulseInfo, tunnel.payload, sizeof(pulseInfo));

    bool isDetectorHeartbeat = pulseInfo.frequency_hz == 0;
    CustomSettings* customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
    const bool isPythonMode = customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    const bool isLowConfidencePythonPulse = isPythonMode && !pulseInfo.confirmed_status && !isDetectorHeartbeat && pulseInfo.detection_status != kNoPulseDetectionStatus;

    if (pulseInfo.tag_id == _tagId) {
        if (isDetectorHeartbeat) {
            _heartbeatLost = false;
            _heartbeatCount++;
            _heartbeatTimeoutTimer.start();
            emit heartbeatLostChanged();
            qCDebug(DetectorInfoLog) << "HEARTBEAT from Detector id" << _tagId;
        } else if (pulseInfo.confirmed_status || isLowConfidencePythonPulse) {
            const bool newLowConfidence = !pulseInfo.confirmed_status;
            qCDebug(DetectorInfoLog)
                << (pulseInfo.confirmed_status ? "CONFIRMED" : "LOW_CONFIDENCE")
                << "tag_id:frequency_hz:seq_ctr:snr:stft_score:noise_psd" <<
                                        pulseInfo.tag_id <<
                                        pulseInfo.frequency_hz <<
                                        pulseInfo.group_seq_counter <<
                                        pulseInfo.snr <<
                                        pulseInfo.stft_score <<
                                        pulseInfo.noise_psd;

            // We track the max pulse in each K group, clamping SNR to 0 and ignoring NaN
            const double clampedSNR = qIsNaN(pulseInfo.snr) ? 0.0 : std::max(0.0, pulseInfo.snr);
            // Python detector doesn't send group_seq_counter — every pulse is its own group
            if (isPythonMode || _lastPulseGroupSeqCtr != pulseInfo.group_seq_counter) {
                _lastPulseGroupSeqCtr = pulseInfo.group_seq_counter;
                _pulseGroupCount++;
                _lastPulseStrength = clampedSNR;
            } else {
                _lastPulseStrength = std::max(clampedSNR, _lastPulseStrength);
            }
            _lastPulseStale = false;
            if (_lastPulseLowConfidence != newLowConfidence) {
                _lastPulseLowConfidence = newLowConfidence;
                emit lastPulseLowConfidenceChanged();
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
            emit lastPulseStaleChanged();
            _stalePulseStrengthTimer.start();

            _maxStrength = qMax(_maxStrength, pulseInfo.snr);
        } else if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
            qCDebug(DetectorInfoLog) << "NO_PULSE from Detector id" << _tagId;
            if (!_lastPulseNoPulse) {
                _lastPulseNoPulse = true;
                emit lastPulseNoPulseChanged();
            }
            _noPulseCount++;
            emit noPulseCountChanged();
            _lastPulseStale = false;
            emit lastPulseStaleChanged();
            _stalePulseStrengthTimer.start();
        }
    }
}
