#include "PythonDetectorInfo.h"

#include "TagDatabase.h"

using namespace TunnelProtocol;

PythonDetectorInfo::PythonDetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k,
                                       QObject* parent)
    : DetectorInfo(tagId, tagLabel, intraPulseMsecs, k, parent)
{}

void PythonDetectorInfo::handlePulse(const PythonPulseInfo_t& pulseInfo)
{
    if (pulseInfo.tag_id != tagId()) {
        return;
    }

    if (pulseInfo.frequency_hz == 0) {
        _heartbeatReceived();
    } else if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
        _noPulseReceived();
    } else {
        const bool lowConfidence = !pulseInfo.confirmed_status;
        qCDebug(DetectorInfoLog) << (lowConfidence ? "LOW_CONFIDENCE" : "CONFIRMED")
                                 << "tag_id:frequency_hz:cycle:snr:score_ratio:noise_psd:rate_state" << pulseInfo.tag_id
                                 << pulseInfo.frequency_hz << pulseInfo.cycle_counter << pulseInfo.snr
                                 << pulseInfo.score_ratio << pulseInfo.noise_psd << pulseInfo.rate_state;

        // Python reports one value per cycle, so every report is its own group
        _pulseReceived(pulseInfo.snr, lowConfidence, true /* newGroup */);
        _setRateLabel(
            TagDatabase::instance()->rateLabel(pulseInfo.tag_id, pulseInfo.rate_state, true /* abbreviated */));
    }
}
