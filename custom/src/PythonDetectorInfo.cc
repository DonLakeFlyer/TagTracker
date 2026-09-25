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

    if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
        _noPulseReceived();
    } else {
        const bool lowConfidence = !pulseInfo.confirmed_status;

        // Python reports one value per cycle, so every report is its own group
        _pulseReceived(pulseInfo.snr, lowConfidence, true /* newGroup */);
        _setRateLabel(
            TagDatabase::instance()->rateLabel(pulseInfo.tag_id, pulseInfo.rate_state, true /* abbreviated */));
    }
}
