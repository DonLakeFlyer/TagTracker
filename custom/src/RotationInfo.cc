#include "RotationInfo.h"

#include <cmath>

#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "SliceInfo.h"
#include "TagDatabase.h"

using namespace TunnelProtocol;

RotationInfo::RotationInfo(int cSlices, QObject* parent)
    : QObject(parent), _cSlices(cSlices), _pulseRateCounts(cRates, 0)
{
    double nextHeading = 0.0;
    const double sliceDegrees = 360.0 / cSlices;

    for (int i = 0; i < cSlices; ++i) {
        auto slice = new SliceInfo(i, nextHeading, sliceDegrees, this);
        _slices.append(slice);
        nextHeading += sliceDegrees;
    }
}

void RotationInfo::pulseReceived(const PythonPulseInfo_t& pulseInfo)
{
    if (pulseInfo.frequency_hz == 0) {
        return;
    }

    qCDebug(CustomPluginLog) << "Pulse received - tag_id:snr:heading:cSlices" << pulseInfo.tag_id << pulseInfo.snr
                             << pulseInfo.yaw_deg << _cSlices;

    const double normalizedHeading = CustomPlugin::normalizeHeading(pulseInfo.yaw_deg);
    const int sliceIndex = _sliceIndexForHeading(normalizedHeading);

    if (sliceIndex < 0) {
        qCWarning(CustomPluginLog) << "No heading slice found for pulse tag_id" << pulseInfo.tag_id << "heading"
                                   << normalizedHeading;
        return;
    }

    _applyPulseToSlice(pulseInfo, sliceIndex);
}

int RotationInfo::_sliceIndexForHeading(double normalizedHeading)
{
    for (int i = 0; i < _cSlices; ++i) {
        bool sliceFound = false;
        SliceInfo* slice = _slices.value<SliceInfo*>(i);
        const double halfSliceDegrees = slice->sliceDegrees() / 2.0;
        if (i == 0) {
            sliceFound = normalizedHeading >= 360.0 - halfSliceDegrees && normalizedHeading < 360.0;
            sliceFound |= normalizedHeading >= 0.0 && normalizedHeading < halfSliceDegrees;
        } else {
            const double fromHeading = slice->centerHeading() - halfSliceDegrees;
            const double toHeading = slice->centerHeading() + halfSliceDegrees;
            sliceFound = fromHeading <= normalizedHeading && normalizedHeading < toHeading;
        }

        if (sliceFound) {
            return i;
        }
    }

    return -1;
}

double RotationInfo::displayStrength(double signalPsd, double noisePsd)
{
    if (noisePsd <= 0.0) {
        return qQNaN();
    }
    if (signalPsd <= 0.0) {
        return 0.0;
    }
    return 10.0 * std::log10(signalPsd / noisePsd);
}

void RotationInfo::_applyPulseToSlice(const PythonPulseInfo_t& pulseInfo, int sliceIndex)
{
    if (pulseInfo.detection_status == kNoPulseDetectionStatus) {
        return;
    }

    SliceInfo* slice = _slices.value<SliceInfo*>(sliceIndex);
    if (!slice) {
        qCWarning(CustomPluginLog) << "Missing slice object for index" << sliceIndex;
        return;
    }

    const double strength = displayStrength(pulseInfo.signal_psd, pulseInfo.noise_psd);
    if (!qIsNaN(strength)) {
        const QString rateLabel = TagDatabase::instance()->rateLabel(pulseInfo.tag_id, pulseInfo.rate_state, false /* abbreviated */);
        slice->updateMaxSNR(pulseInfo.tag_id, strength, pulseInfo.confirmed_status, rateLabel);
    }

    if (pulseInfo.confirmed_status && !qIsNaN(pulseInfo.snr)) {
        // Rate switches count toward rate B, the rate the group ended on or moved through
        const int rateIndex = pulseInfo.rate_state == kRateStateA ? 0 : 1;
        _pulseRateCounts[rateIndex]++;
        emit pulseRateCountsChanged();
    }

    _updateMaxSNR();
}

void RotationInfo::_updateMaxSNR()
{
    // Recomputed from the slices so a confirmed re-measurement that lowers a slice
    // also lowers the rose normalization instead of leaving a stale maximum.
    double newMax = qQNaN();
    for (int i = 0; i < _cSlices; ++i) {
        SliceInfo* slice = _slices.value<SliceInfo*>(i);
        if (!slice) {
            continue;
        }
        const double snr = slice->displaySNR();
        if (!qIsNaN(snr) && snr > 0.0 && (qIsNaN(newMax) || snr > newMax)) {
            newMax = snr;
        }
    }

    const bool changed = qIsNaN(newMax) != qIsNaN(_maxSNR) || (!qIsNaN(newMax) && newMax != _maxSNR);
    if (changed) {
        qCDebug(CustomPluginLog) << "Updating RotationInfo max SNR to" << newMax;
        _maxSNR = newMax;
        emit maxSNRChanged(_maxSNR);
    }
}

void RotationInfo::setBearingResult(float bearingDeg, float rSquared, uint32_t nValidSlices, float bestSNR)
{
    _bearingDeg = static_cast<double>(bearingDeg);
    _bearingRSquared = static_cast<double>(rSquared);
    // NaN (or any non-finite value): the controller compared its lock candidates
    // and none fitted the antenna pattern well enough to call a bearing.
    _bearingValid = nValidSlices >= 3 && std::isfinite(bearingDeg);

    qCDebug(CustomPluginLog) << "BearingResult applied: bearing" << _bearingDeg << "R²" << _bearingRSquared
                             << "nValidSlices" << nValidSlices << "bestSNR" << bestSNR << "valid" << _bearingValid;

    emit bearingChanged();
}
