#include "UavrtDetectorInfo.h"

using namespace TunnelProtocol;

UavrtDetectorInfo::UavrtDetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k,
                                     QObject* parent)
    : DetectorInfo(tagId, tagLabel, intraPulseMsecs, k, parent)
{}

void UavrtDetectorInfo::handlePulse(const PulseInfo_t& pulseInfo)
{
    if (pulseInfo.tag_id != tagId()) {
        return;
    }

    if (pulseInfo.frequency_hz == 0) {
        _heartbeatReceived();
    } else if (pulseInfo.confirmed_status) {
        qCDebug(DetectorInfoLog) << "CONFIRMED tag_id:frequency_hz:seq_ctr:snr:stft_score:noise_psd" << pulseInfo.tag_id
                                 << pulseInfo.frequency_hz << pulseInfo.group_seq_counter << pulseInfo.snr
                                 << pulseInfo.stft_score << pulseInfo.noise_psd;

        // The strength bar shows the max pulse within each K group
        const bool newGroup = _lastPulseGroupSeqCtr != pulseInfo.group_seq_counter;
        _lastPulseGroupSeqCtr = pulseInfo.group_seq_counter;
        _pulseReceived(pulseInfo.snr, false /* lowConfidence */, newGroup);
    } else if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
        _noPulseReceived();
    }
}
