#include "RotationInfo.h"
#include "SliceInfo.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "TagDatabase.h"

#include <cmath>
#include <limits>

namespace {
static constexpr uint8_t kNoPulseDetectionStatus = 3;
static constexpr double kSameFrequencyToleranceHz = 35.0;
static constexpr double kMaxPairTimeDeltaSeconds = 0.25;
static constexpr double kSuppressionSNRGapDb = 3.0;
}

RotationInfo::RotationInfo(int cSlices, QObject* parent)
    : QObject           (parent)
    , _cSlices          (cSlices)
    , _pulseRateCounts  (cRates, 0)
{
    double nextHeading = 0.0;
    const double sliceDegrees = 360.0 / cSlices;

    for (int i = 0; i < cSlices; ++i) {
        auto slice = new SliceInfo(i, nextHeading, sliceDegrees, this);
        _slices.append(slice);
        nextHeading += sliceDegrees;
    }
}

void RotationInfo::pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo, bool enablePythonCrossRateCoalescing, double pendingPairTimeoutSeconds)
{
    if (pulseInfo.frequency_hz == 0) {
        // Detector heartbeat
        return;
    }

    qCDebug(CustomPluginLog) << "PulseInfo received - tag_id:snr:heading:cSlices" << pulseInfo.tag_id << pulseInfo.snr << pulseInfo.orientation_z << _cSlices << " _ " << Q_FUNC_INFO;

    const double normalizedHeading = CustomPlugin::normalizeHeading(pulseInfo.orientation_z);
    const int sliceIndex = _sliceIndexForHeading(normalizedHeading);

    if (sliceIndex < 0) {
        qWarning() << "No heading slice found for pulse tag_id" << pulseInfo.tag_id << "heading" << normalizedHeading << " - " << Q_FUNC_INFO;
        return;
    }

    if (!enablePythonCrossRateCoalescing) {
        _applyPulseToSlice(pulseInfo, sliceIndex);
        return;
    }

    _expireStalePendingPairs(pulseInfo.start_time_seconds, pendingPairTimeoutSeconds);

    const uint32_t baseTagId = pulseInfo.tag_id - (pulseInfo.tag_id % cRates);
    const int rateIndex = pulseInfo.tag_id % cRates;
    const quint64 key = _pairKey(baseTagId, sliceIndex);

    PendingPairResult& pendingPair = _pendingPairResults[key];
    if (qIsNaN(pendingPair.pendingSinceSeconds)) {
        pendingPair.pendingSinceSeconds = pulseInfo.start_time_seconds;
    }
    PendingRateResult& pendingRate = pendingPair.rates[rateIndex];

    pendingRate.received = true;
    pendingRate.noDetection = _isNoDetectionPulse(pulseInfo);
    pendingRate.frequencyHz = pulseInfo.frequency_hz;
    pendingRate.snr = pulseInfo.snr;
    pendingRate.startTimeSeconds = pulseInfo.start_time_seconds;
    pendingRate.pulseInfo = pulseInfo;

    if (!pendingPair.rates[0].received || !pendingPair.rates[1].received) {
        return;
    }

    _coalescePairAndApply(baseTagId, sliceIndex, pendingPair);
    _pendingPairResults.remove(key);
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

void RotationInfo::_applyPulseToSlice(const TunnelProtocol::PulseInfo_t& pulseInfo, int sliceIndex)
{
    if (_isNoDetectionPulse(pulseInfo)) {
        return;
    }

    const double sliceStrength = pulseInfo.snr;
    SliceInfo* slice = _slices.value<SliceInfo*>(sliceIndex);

    if (!slice) {
        qWarning() << "Missing slice object for index" << sliceIndex << " - " << Q_FUNC_INFO;
        return;
    }

    if (!qIsNaN(sliceStrength) && sliceStrength > 0.0) {
        slice->updateMaxSNR(sliceStrength, pulseInfo.confirmed_status, _sourceRateLabelForTagId(pulseInfo.tag_id));
    }

    _updatePulseRateCount(pulseInfo);
    _updateMaxSNR(sliceStrength);
}

void RotationInfo::_updatePulseRateCount(const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    const int rateIndex = pulseInfo.tag_id % cRates;

    if (pulseInfo.confirmed_status && !qIsNaN(pulseInfo.snr)) {
        qCDebug(CustomPluginLog) << "Updating RotationInfo pulse rate counts" << _pulseRateCounts << " _ " << Q_FUNC_INFO;
        _pulseRateCounts[rateIndex]++;
        emit pulseRateCountsChanged();
    }
}

void RotationInfo::_updateMaxSNR(double sliceStrength)
{
    if (!qIsNaN(sliceStrength) && sliceStrength > 0.0) {
        if (qIsNaN(_maxSNR) || sliceStrength > _maxSNR) {
            qCDebug(CustomPluginLog) << "Updating RotationInfo max SNR to" << sliceStrength << " _ " << Q_FUNC_INFO;
            _maxSNR = sliceStrength;
            emit maxSNRChanged(_maxSNR);
        }
    }
}

QString RotationInfo::_sourceRateLabelForTagId(uint32_t tagId) const
{
    const int oneBasedRateIndex = (tagId % cRates) + 1;
    const uint32_t baseTagId = tagId - (tagId % cRates);
    TagInfo* tagInfo = TagDatabase::instance()->findTagInfo(baseTagId);
    if (!tagInfo) {
        return QString::number(oneBasedRateIndex);
    }

    TagManufacturer* manufacturer = TagDatabase::instance()->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());
    if (!manufacturer) {
        return QString::number(oneBasedRateIndex);
    }

    if ((tagId % cRates) == 0) {
        const QString rate1 = manufacturer->ip_msecs_1_id()->rawValue().toString();
        return rate1.isEmpty() ? QStringLiteral("1") : rate1;
    }

    const QString rate2 = manufacturer->ip_msecs_2_id()->rawValue().toString();
    return rate2.isEmpty() ? QStringLiteral("2") : rate2;
}

quint64 RotationInfo::_pairKey(uint32_t baseTagId, int sliceIndex) const
{
    return (static_cast<quint64>(baseTagId) << 32) | static_cast<quint64>(sliceIndex);
}

int RotationInfo::_sliceIndexFromPairKey(quint64 key) const
{
    return static_cast<int>(key & 0xffffffffu);
}

uint32_t RotationInfo::_baseTagIdFromPairKey(quint64 key) const
{
    return static_cast<uint32_t>(key >> 32);
}

bool RotationInfo::_isNoDetectionPulse(const TunnelProtocol::PulseInfo_t& pulseInfo) const
{
    return pulseInfo.detection_status == kNoPulseDetectionStatus;
}

void RotationInfo::_coalescePairAndApply(uint32_t baseTagId, int sliceIndex, const PendingPairResult& pendingPair)
{
    const PendingRateResult& rate0 = pendingPair.rates[0];
    const PendingRateResult& rate1 = pendingPair.rates[1];

    if (rate0.noDetection && rate1.noDetection) {
        qCDebug(CustomPluginLog) << "Cross-rate coalescing: both rates no-detection for tag" << baseTagId << "slice" << sliceIndex;
        return;
    }

    if (rate0.noDetection != rate1.noDetection) {
        const PendingRateResult& detectedRate = rate0.noDetection ? rate1 : rate0;
        qCDebug(CustomPluginLog) << "Cross-rate coalescing: keep single detection for tag" << baseTagId << "slice" << sliceIndex << "snr" << detectedRate.snr;
        _applyPulseToSlice(detectedRate.pulseInfo, sliceIndex);
        return;
    }

    const double frequencyDelta = std::abs(static_cast<double>(rate0.frequencyHz) - static_cast<double>(rate1.frequencyHz));
    const bool sameFrequency = frequencyDelta <= kSameFrequencyToleranceHz;

    double timeDelta = std::numeric_limits<double>::infinity();
    if (!qIsNaN(rate0.startTimeSeconds) && !qIsNaN(rate1.startTimeSeconds)) {
        timeDelta = std::abs(rate0.startTimeSeconds - rate1.startTimeSeconds);
    }
    const bool timeAligned = timeDelta <= kMaxPairTimeDeltaSeconds;

    if (!sameFrequency || !timeAligned || qIsNaN(rate0.snr) || qIsNaN(rate1.snr)) {
        qCDebug(CustomPluginLog) << "Cross-rate coalescing: keep both for tag" << baseTagId
                                 << "slice" << sliceIndex
                                 << "frequencyDelta" << frequencyDelta
                                 << "timeDelta" << timeDelta;
        _applyPulseToSlice(rate0.pulseInfo, sliceIndex);
        _applyPulseToSlice(rate1.pulseInfo, sliceIndex);
        return;
    }

    const double snrGap = std::abs(rate0.snr - rate1.snr);
    if (snrGap < kSuppressionSNRGapDb) {
        qCDebug(CustomPluginLog) << "Cross-rate coalescing: same frequency but near-tie, keep both for tag" << baseTagId
                                 << "slice" << sliceIndex
                                 << "snrGap" << snrGap;
        _applyPulseToSlice(rate0.pulseInfo, sliceIndex);
        _applyPulseToSlice(rate1.pulseInfo, sliceIndex);
        return;
    }

    if (rate0.pulseInfo.confirmed_status != rate1.pulseInfo.confirmed_status) {
        const PendingRateResult& confirmedRate = rate0.pulseInfo.confirmed_status ? rate0 : rate1;
        const PendingRateResult& lowConfidenceRate = rate0.pulseInfo.confirmed_status ? rate1 : rate0;
        qCDebug(CustomPluginLog) << "Cross-rate coalescing: keep confirmed result for tag" << baseTagId
                                 << "slice" << sliceIndex
                                 << "confirmedSnr" << confirmedRate.snr
                                 << "lowConfidenceSnr" << lowConfidenceRate.snr;
        _applyPulseToSlice(confirmedRate.pulseInfo, sliceIndex);
        return;
    }

    const PendingRateResult& strongerRate = rate0.snr >= rate1.snr ? rate0 : rate1;
    const PendingRateResult& weakerRate = rate0.snr >= rate1.snr ? rate1 : rate0;
    qCDebug(CustomPluginLog) << "Cross-rate coalescing: suppress weaker rate for tag" << baseTagId
                             << "slice" << sliceIndex
                             << "strongerSnr" << strongerRate.snr
                             << "weakerSnr" << weakerRate.snr
                             << "snrGap" << snrGap;
    _applyPulseToSlice(strongerRate.pulseInfo, sliceIndex);
}

void RotationInfo::_expireStalePendingPairs(double currentStartTimeSeconds, double pendingPairTimeoutSeconds)
{
    if (qIsNaN(currentStartTimeSeconds) || qIsNaN(pendingPairTimeoutSeconds) || pendingPairTimeoutSeconds <= 0.0) {
        return;
    }

    for (auto it = _pendingPairResults.begin(); it != _pendingPairResults.end();) {
        const PendingPairResult& pendingPair = it.value();

        if (qIsNaN(pendingPair.pendingSinceSeconds) || (currentStartTimeSeconds - pendingPair.pendingSinceSeconds) < pendingPairTimeoutSeconds) {
            ++it;
            continue;
        }

        const quint64 key = it.key();
        const uint32_t baseTagId = _baseTagIdFromPairKey(key);
        const int sliceIndex = _sliceIndexFromPairKey(key);
        const PendingRateResult& rate0 = pendingPair.rates[0];
        const PendingRateResult& rate1 = pendingPair.rates[1];

        const bool rate0HasDetection = rate0.received && !rate0.noDetection;
        const bool rate1HasDetection = rate1.received && !rate1.noDetection;

        if (rate0HasDetection && !rate1.received) {
            qCDebug(CustomPluginLog) << "Cross-rate coalescing timeout: keeping unmatched rate0 detection for tag" << baseTagId << "slice" << sliceIndex;
            _applyPulseToSlice(rate0.pulseInfo, sliceIndex);
        } else if (rate1HasDetection && !rate0.received) {
            qCDebug(CustomPluginLog) << "Cross-rate coalescing timeout: keeping unmatched rate1 detection for tag" << baseTagId << "slice" << sliceIndex;
            _applyPulseToSlice(rate1.pulseInfo, sliceIndex);
        } else {
            qCDebug(CustomPluginLog) << "Cross-rate coalescing timeout: dropping stale pending pair for tag" << baseTagId << "slice" << sliceIndex;
        }

        it = _pendingPairResults.erase(it);
    }
}
