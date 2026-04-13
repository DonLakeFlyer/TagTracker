#include "RotationInfo.h"
#include "SliceInfo.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "TagDatabase.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

void RotationInfo::pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    if (pulseInfo.frequency_hz == 0) {
        // Detector heartbeat
        return;
    }

    qCDebug(CustomPluginLog) << "PulseInfo received - tag_id:snr:heading:cSlices" << pulseInfo.tag_id << pulseInfo.snr << pulseInfo.yaw_deg << _cSlices << " _ " << Q_FUNC_INFO;

    const double normalizedHeading = CustomPlugin::normalizeHeading(pulseInfo.yaw_deg);
    const int sliceIndex = _sliceIndexForHeading(normalizedHeading);

    if (sliceIndex < 0) {
        qWarning() << "No heading slice found for pulse tag_id" << pulseInfo.tag_id << "heading" << normalizedHeading << " - " << Q_FUNC_INFO;
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
        CustomSettings* customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
        const bool isPythonMode = customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
        const QString rateLabel = isPythonMode ? _rateLabelFromGroupInd(pulseInfo) : _sourceRateLabelForTagId(pulseInfo.tag_id);
        slice->updateMaxSNR(sliceStrength, pulseInfo.confirmed_status, rateLabel);
    }

    _updatePulseRateCount(pulseInfo);
    _updateMaxSNR(sliceStrength);
}

void RotationInfo::_updatePulseRateCount(const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    CustomSettings* customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
    const bool isPythonMode = customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    const int rateIndex = isPythonMode ? (pulseInfo.group_ind == 0 ? 0 : 1) : (pulseInfo.tag_id % cRates);

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

QString RotationInfo::_rateLabelFromGroupInd(const TunnelProtocol::PulseInfo_t& pulseInfo) const
{
    TagInfo* tagInfo = TagDatabase::instance()->findTagInfo(pulseInfo.tag_id);
    TagManufacturer* manufacturer = tagInfo ? TagDatabase::instance()->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt()) : nullptr;
    const QString rateA = manufacturer ? manufacturer->ip_msecs_1_id()->rawValue().toString() : QString();
    const QString rateB = manufacturer ? manufacturer->ip_msecs_2_id()->rawValue().toString() : QString();
    const QString letterA = rateA.isEmpty() ? QStringLiteral("1") : rateA.left(1);
    const QString letterB = rateB.isEmpty() ? QStringLiteral("2") : rateB.left(1);

    CustomSettings* customSettings = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
    const uint32_t k = customSettings->pythonK()->rawValue().toUInt();

    if (pulseInfo.group_ind == 0) {
        return rateA.isEmpty() ? QStringLiteral("1") : rateA;
    } else if (pulseInfo.group_ind == 1) {
        return rateB.isEmpty() ? QStringLiteral("2") : rateB;
    } else if (pulseInfo.group_ind < k) {
        return letterA + QStringLiteral("/") + letterB;
    } else {
        return letterB + QStringLiteral("/") + letterA;
    }
}

bool RotationInfo::_isNoDetectionPulse(const TunnelProtocol::PulseInfo_t& pulseInfo) const
{
    return pulseInfo.detection_status == kNoPulseDetectionStatus;
}

// RA-2AK measured antenna pattern in dB, normalized to 0 dB at boresight.
// Eyeballed from the Telonics RA-2A reception radiation pattern polar plot.
// 10° steps from 0° (front) to 180° (back). Pattern is symmetric so we mirror for 180-360°.
static constexpr double kPatternDb[] = {
    //  0°     10°     20°     30°     40°     50°     60°     70°     80°     90°
     0.0,    0.0,   -0.5,   -1.0,   -2.5,   -5.0,  -10.5,  -14.5,  -20.5,  -27.5,
    // 100°   110°   120°   130°   140°   150°   160°   170°   180°
   -20.0,  -17.5,  -14.5,  -12.5,  -13.5,  -10.5,  -10.5,  -10.0,  -10.0
};
static constexpr int kPatternSize = sizeof(kPatternDb) / sizeof(kPatternDb[0]); // 19 entries, 0-180 in 10° steps

// Interpolate the measured pattern at an arbitrary angle offset from boresight (degrees).
// Returns the pattern value in linear (power) scale, normalized so boresight = 1.0.
static double _patternLinear(double offsetDeg)
{
    // Normalize to 0-360 then fold to 0-180 (symmetric)
    double angle = std::fmod(offsetDeg, 360.0);
    if (angle < 0.0) angle += 360.0;
    if (angle > 180.0) angle = 360.0 - angle;

    // Interpolate in the 10° step table
    const double indexF = angle / 10.0;
    const int idx0 = static_cast<int>(indexF);
    const int idx1 = std::min(idx0 + 1, kPatternSize - 1);
    const double frac = indexF - idx0;

    const double db = kPatternDb[idx0] * (1.0 - frac) + kPatternDb[idx1] * frac;
    return std::pow(10.0, db / 10.0);
}

// Antenna pattern model using measured RA-2AK lookup table:
//   SNR(θ) = A · pattern_linear(θ - φ) + B
double RotationInfo::_antennaPattern(double thetaDeg, double phiDeg, double A, double B)
{
    return A * _patternLinear(thetaDeg - phiDeg) + B;
}

void RotationInfo::fitBearing(void)
{
    if (_cSlices < 3) {
        return;
    }

    // Collect (heading, SNR) pairs from slices
    QVector<double> headings(_cSlices);
    QVector<double> snrValues(_cSlices);
    int validCount = 0;

    for (int i = 0; i < _cSlices; ++i) {
        SliceInfo* slice = _slices.value<SliceInfo*>(i);
        if (!slice) {
            continue;
        }
        headings[i]  = slice->centerHeading();
        const double snr = slice->displaySNR();
        snrValues[i] = qIsNaN(snr) ? 0.0 : snr;
        if (snrValues[i] > 0.0) {
            validCount++;
        }
    }

    // Need at least 3 slices with data for a 3-parameter fit
    if (validCount < 3) {
        qCDebug(CustomPluginLog) << "Bearing fit: insufficient data, only" << validCount << "slices with detections";
        return;
    }

    // Initial estimates for A and B from data range
    double maxVal = snrValues[0];
    double minVal = snrValues[0];
    for (int i = 1; i < _cSlices; ++i) {
        if (snrValues[i] > maxVal) {
            maxVal = snrValues[i];
        }
        if (snrValues[i] < minVal) {
            minVal = snrValues[i];
        }
    }

    double A = maxVal - minVal;
    double B = minVal;

    if (A <= 0.0) {
        qCDebug(CustomPluginLog) << "Bearing fit: no SNR contrast";
        return;
    }

    // Brute-force scan for best initial phi at 1° resolution.
    // The RA-2AK pattern has sharp nulls, so the LM solver can get stuck
    // at a local minimum if started at the nearest slice heading.
    double phi = 0.0;
    {
        double bestCost = std::numeric_limits<double>::max();
        for (int deg = 0; deg < 360; ++deg) {
            const double testPhi = static_cast<double>(deg);
            double testCost = 0.0;
            for (int i = 0; i < _cSlices; ++i) {
                const double predicted = A * _patternLinear(headings[i] - testPhi) + B;
                const double r = snrValues[i] - predicted;
                testCost += r * r;
            }
            if (testCost < bestCost) {
                bestCost = testCost;
                phi = testPhi;
            }
        }
    }

    // Levenberg-Marquardt refinement of [phi, A, B]
    // Model: SNR(θ) = A · pattern_linear(θ - φ) + B
    // dPhi is computed numerically since the pattern is a lookup table
    static constexpr int    kMaxIterations  = 100;
    static constexpr double kConvergenceEps = 1e-6;
    static constexpr double kNumDiffStep    = 0.5;  // degrees for numerical derivative
    static constexpr double kMaxPhiStep     = 15.0; // max bearing change per iteration (degrees)

    double lambda = 1.0; // LM damping factor

    // Compute initial cost
    double cost = 0.0;
    for (int i = 0; i < _cSlices; ++i) {
        const double predicted = A * _patternLinear(headings[i] - phi) + B;
        const double r = snrValues[i] - predicted;
        cost += r * r;
    }

    for (int iter = 0; iter < kMaxIterations; ++iter) {
        double JtJ[3][3] = {};
        double Jtr[3]    = {};

        for (int i = 0; i < _cSlices; ++i) {
            const double offset   = headings[i] - phi;
            const double pVal     = _patternLinear(offset);
            const double predicted = A * pVal + B;
            const double residual  = snrValues[i] - predicted;

            // Numerical derivative of pattern w.r.t. phi (central difference)
            const double pPlus  = _patternLinear(offset + kNumDiffStep);
            const double pMinus = _patternLinear(offset - kNumDiffStep);
            // d/dphi: shifting phi up means offset decreases
            const double dPhi = -A * (pMinus - pPlus) / (2.0 * kNumDiffStep);
            const double dA   = pVal;
            const double dB   = 1.0;

            const double J[3] = { dPhi, dA, dB };

            for (int r = 0; r < 3; ++r) {
                Jtr[r] += J[r] * residual;
                for (int c = 0; c < 3; ++c) {
                    JtJ[r][c] += J[r] * J[c];
                }
            }
        }

        // Add LM damping to diagonal
        for (int k = 0; k < 3; ++k) {
            JtJ[k][k] *= (1.0 + lambda);
        }

        // Solve 3x3 system JtJ * delta = Jtr using Cramer's rule
        const double det =
            JtJ[0][0] * (JtJ[1][1] * JtJ[2][2] - JtJ[1][2] * JtJ[2][1]) -
            JtJ[0][1] * (JtJ[1][0] * JtJ[2][2] - JtJ[1][2] * JtJ[2][0]) +
            JtJ[0][2] * (JtJ[1][0] * JtJ[2][1] - JtJ[1][1] * JtJ[2][0]);

        if (std::abs(det) < 1e-15) {
            qCDebug(CustomPluginLog) << "Bearing fit: singular matrix at iteration" << iter;
            break;
        }

        const double invDet = 1.0 / det;

        double delta[3];
        delta[0] = invDet * (
            Jtr[0] * (JtJ[1][1] * JtJ[2][2] - JtJ[1][2] * JtJ[2][1]) -
            JtJ[0][1] * (Jtr[1] * JtJ[2][2] - JtJ[1][2] * Jtr[2]) +
            JtJ[0][2] * (Jtr[1] * JtJ[2][1] - JtJ[1][1] * Jtr[2]));
        delta[1] = invDet * (
            JtJ[0][0] * (Jtr[1] * JtJ[2][2] - JtJ[1][2] * Jtr[2]) -
            Jtr[0] * (JtJ[1][0] * JtJ[2][2] - JtJ[1][2] * JtJ[2][0]) +
            JtJ[0][2] * (JtJ[1][0] * Jtr[2] - Jtr[1] * JtJ[2][0]));
        delta[2] = invDet * (
            JtJ[0][0] * (JtJ[1][1] * Jtr[2] - Jtr[1] * JtJ[2][1]) -
            JtJ[0][1] * (JtJ[1][0] * Jtr[2] - Jtr[1] * JtJ[2][0]) +
            Jtr[0] * (JtJ[1][0] * JtJ[2][1] - JtJ[1][1] * JtJ[2][0]));

        // Clamp phi step to prevent overshooting past sharp nulls
        if (std::abs(delta[0]) > kMaxPhiStep) {
            const double scale = kMaxPhiStep / std::abs(delta[0]);
            delta[0] *= scale;
            delta[1] *= scale;
            delta[2] *= scale;
        }

        // Trial update
        const double phiTrial = phi + delta[0];
        const double ATrial   = std::max(A + delta[1], 0.0);
        const double BTrial   = B + delta[2];

        // Compute trial cost
        double trialCost = 0.0;
        for (int i = 0; i < _cSlices; ++i) {
            const double predicted = ATrial * _patternLinear(headings[i] - phiTrial) + BTrial;
            const double r = snrValues[i] - predicted;
            trialCost += r * r;
        }

        if (trialCost < cost) {
            // Accept step, reduce damping
            phi  = phiTrial;
            A    = ATrial;
            B    = BTrial;
            cost = trialCost;
            lambda *= 0.5;
            if (lambda < 1e-7) lambda = 1e-7;
        } else {
            // Reject step, increase damping
            lambda *= 4.0;
            if (lambda > 1e7) {
                qCDebug(CustomPluginLog) << "Bearing fit: lambda overflow at iteration" << iter;
                break;
            }
            continue; // retry with more damping
        }

        const double stepSize = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
        if (stepSize < kConvergenceEps) {
            qCDebug(CustomPluginLog) << "Bearing fit: converged at iteration" << iter;
            break;
        }
    }

    // Normalize phi to 0-360
    phi = std::fmod(phi, 360.0);
    if (phi < 0.0) {
        phi += 360.0;
    }

    // Compute R²
    double ssRes  = 0.0;
    double ssTot  = 0.0;
    double mean   = 0.0;
    for (int i = 0; i < _cSlices; ++i) {
        mean += snrValues[i];
    }
    mean /= _cSlices;

    for (int i = 0; i < _cSlices; ++i) {
        const double predicted = _antennaPattern(headings[i], phi, A, B);
        const double residual  = snrValues[i] - predicted;
        ssRes += residual * residual;
        ssTot += (snrValues[i] - mean) * (snrValues[i] - mean);
    }

    const double rSquared = (ssTot > 0.0) ? (1.0 - ssRes / ssTot) : 0.0;

    // Estimate bearing uncertainty from the inverse of J^T J diagonal
    double JtJ_final[3][3] = {};
    for (int i = 0; i < _cSlices; ++i) {
        const double offset = headings[i] - phi;
        const double pVal   = _patternLinear(offset);
        const double pPlus  = _patternLinear(offset + kNumDiffStep);
        const double pMinus = _patternLinear(offset - kNumDiffStep);
        const double dPhi   = -A * (pMinus - pPlus) / (2.0 * kNumDiffStep);
        const double dA     = pVal;
        const double dB     = 1.0;
        const double J[3]   = { dPhi, dA, dB };

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                JtJ_final[r][c] += J[r] * J[c];
            }
        }
    }

    double uncertainty = qQNaN();
    const int dof = _cSlices - 3;
    if (dof > 0) {
        const double sigma2 = ssRes / dof;
        const double detFinal =
            JtJ_final[0][0] * (JtJ_final[1][1] * JtJ_final[2][2] - JtJ_final[1][2] * JtJ_final[2][1]) -
            JtJ_final[0][1] * (JtJ_final[1][0] * JtJ_final[2][2] - JtJ_final[1][2] * JtJ_final[2][0]) +
            JtJ_final[0][2] * (JtJ_final[1][0] * JtJ_final[2][1] - JtJ_final[1][1] * JtJ_final[2][0]);

        if (std::abs(detFinal) > 1e-15) {
            const double cof00 = JtJ_final[1][1] * JtJ_final[2][2] - JtJ_final[1][2] * JtJ_final[2][1];
            const double varPhi = sigma2 * cof00 / detFinal;
            if (varPhi > 0.0) {
                uncertainty = std::sqrt(varPhi);
            }
        }
    }

    // Check for 180° ambiguity: compare front lobe peak to back lobe contribution
    // With the measured pattern, the back lobe is -10 dB (factor ~0.1 in linear).
    // If the fitted amplitude relative to noise is too low, the back lobe SNR
    // is comparable to the front, making the bearing ambiguous.
    const double frontSNR = A * _patternLinear(0.0) + B;
    const double backSNR  = A * _patternLinear(180.0) + B;
    const bool ambiguous  = (backSNR > 0.0 && frontSNR > 0.0) ? (frontSNR / backSNR < 2.0) : false;

    // Store results
    _bearingDeg         = phi;
    _bearingUncertainty = uncertainty;
    _bearingRSquared    = rSquared;
    _bearingAmbiguous   = ambiguous;
    _bearingValid       = true;

    qCDebug(CustomPluginLog) << "Bearing fit result: bearing" << _bearingDeg
                             << "uncertainty" << _bearingUncertainty
                             << "R²" << _bearingRSquared
                             << "ambiguous" << _bearingAmbiguous
                             << "A" << A << "B" << B
                             << "frontSNR" << frontSNR << "backSNR" << backSNR;

    emit bearingChanged();
}

void RotationInfo::setBearingResult(float bearingDeg, float rSquared, uint32_t nValidSlices, float bestSNR)
{
    _bearingDeg         = static_cast<double>(bearingDeg);
    _bearingRSquared    = static_cast<double>(rSquared);
    _bearingUncertainty = qQNaN();
    _bearingAmbiguous   = false;
    _bearingValid       = nValidSlices >= 3;

    qCDebug(CustomPluginLog) << "BearingResult applied: bearing" << _bearingDeg
                             << "R²" << _bearingRSquared
                             << "nValidSlices" << nValidSlices
                             << "bestSNR" << bestSNR
                             << "valid" << _bearingValid;

    emit bearingChanged();
}
