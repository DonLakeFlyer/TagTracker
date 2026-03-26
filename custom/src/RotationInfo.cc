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
    if (pulseInfo.frequency_hz == 0) {
        // Detector heartbeat
        return;
    }

    qCDebug(CustomPluginLog) << "PulseInfo received - tag_id:snr:heading:cSlices" << pulseInfo.tag_id << pulseInfo.snr << pulseInfo.orientation_z << _cSlices << " _ " << Q_FUNC_INFO;

    double sliceStrength = pulseInfo.snr;
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
            if (!qIsNaN(sliceStrength) && sliceStrength > 0.0) {
                slice->updateMaxSNR(sliceStrength, pulseInfo.confirmed_status);
            }
            break;
        }
    }

    // Update the pulse rate counts
    int rateIndex = pulseInfo.tag_id % cRates;
    if (pulseInfo.confirmed_status && !qIsNaN(pulseInfo.snr)) {
        qCDebug(CustomPluginLog) << "Updating RotationInfo pulse rate counts" << _pulseRateCounts << " _ " << Q_FUNC_INFO;
        _pulseRateCounts[rateIndex]++;
        emit pulseRateCountsChanged();
    }

    // Update max snr for rotation
    if (!qIsNaN(sliceStrength) && sliceStrength > 0.0) {
        if (qIsNaN(_maxSNR) || sliceStrength > _maxSNR) {
            qCDebug(CustomPluginLog) << "Updating RotationInfo max SNR to" << sliceStrength << " _ " << Q_FUNC_INFO;
            _maxSNR = sliceStrength;
            emit maxSNRChanged(_maxSNR);
        }
    }
}
