#pragma once

#include "DetectorInfo.h"
#include "TunnelProtocol.h"

/// uavrt_detection detector (Survey Detection flight mode). One instance per pulse rate; rate B is tagId + 1.
class UavrtDetectorInfo : public DetectorInfo
{
    Q_OBJECT

public:
    UavrtDetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k,
                      QObject* parent = nullptr);

    void handlePulse(const TunnelProtocol::PulseInfo_t& pulseInfo);

private:
    int64_t _lastPulseGroupSeqCtr = -1;
};
