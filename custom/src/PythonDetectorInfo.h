#pragma once

#include "DetectorInfo.h"
#include "TunnelProtocol.h"

/// Python pulse_detector detector (rotation flight modes). One instance per tag handles both pulse rates.
class PythonDetectorInfo : public DetectorInfo
{
    Q_OBJECT

public:
    PythonDetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k,
                       QObject* parent = nullptr);

    void handlePulse(const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
};
