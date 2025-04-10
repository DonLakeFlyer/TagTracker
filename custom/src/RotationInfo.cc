#include "RotationInfo.h"
#include "SliceInfo.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"

RotationInfo::RotationInfo(int cSlices, QObject* parent)
    : QObject           (parent)
    , _cSlices          (cSlices)
    , _pulseRateCounts  (cRates, 0)
{
    double nextHeading = 0.0;
    double sliceDegrees = 360.0 / cSlices;

    for (int i = 0; i < cSlices; ++i) {
        auto slice = new SliceInfo(i, nextHeading, sliceDegrees, this);
        _slices.append(slice);
        nextHeading += sliceDegrees;
    }
}

void RotationInfo::pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    if (pulseInfo.tag_id == 0) {
        // Detector heartbeat
        return;
    }

    qCDebug(CustomPluginLog) << "PulseInfo received - tag_id:snr:heading:cSlices" << pulseInfo.tag_id << pulseInfo.snr << pulseInfo.orientation_z << _cSlices << " _ " << Q_FUNC_INFO;

    double normalizedHeading = CustomPlugin::normalizeHeading(pulseInfo.orientation_z);

    // Find the slice that this pulse belongs to
    for (int i = 0; i < _cSlices; ++i) {
        bool sliceFound = false;
        SliceInfo* slice = _slices.value<SliceInfo*>(i);
        double halfSliceDegrees = slice->sliceDegrees() / 2.0;
        if (i == 0) {
            sliceFound = normalizedHeading >= 360.0 - halfSliceDegrees && normalizedHeading < 360.0;
            sliceFound |= normalizedHeading >= 0.0 && normalizedHeading < halfSliceDegrees;
        } else {
            double fromHeading = slice->centerHeading() - halfSliceDegrees;
            double toHeading = slice->centerHeading() + halfSliceDegrees;
            sliceFound = fromHeading <= normalizedHeading && normalizedHeading < toHeading;
        }
        if (sliceFound) {
                slice->updateMaxSNR(pulseInfo.snr);
            break;
        }
    }

    // Update the pulse rate counts
    int rateIndex = pulseInfo.tag_id % cRates;
    if (!qIsNaN(pulseInfo.snr)) {
        qCDebug(CustomPluginLog) << "Updating RotationInfo pulse rate counts" << _pulseRateCounts << " _ " << Q_FUNC_INFO;
        _pulseRateCounts[rateIndex]++;
        emit pulseRateCountsChanged();
    }

    // Update max snr for rotation
    if (qIsNaN(_maxSNR) || pulseInfo.snr > _maxSNR) {
        qCDebug(CustomPluginLog) << "Updating RotationInfo max SNR to" << pulseInfo.snr << " _ " << Q_FUNC_INFO;
        _maxSNR = pulseInfo.snr;
        emit maxSNRChanged(_maxSNR);
    }
}
