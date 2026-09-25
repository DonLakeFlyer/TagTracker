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

    if (pulseInfo.confirmed_status) {
        // The strength bar shows the max pulse within each K group
        const bool newGroup = _lastPulseGroupSeqCtr != pulseInfo.group_seq_counter;
        _lastPulseGroupSeqCtr = pulseInfo.group_seq_counter;
        _pulseReceived(pulseInfo.snr, false /* lowConfidence */, newGroup);
    } else if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
        _noPulseReceived();
    }
}
