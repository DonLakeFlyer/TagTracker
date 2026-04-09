#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

#include <QObject>
#include <QHash>
#include <QString>

class RotationInfo : public QObject
{
    Q_OBJECT

public:
    RotationInfo(int cSlices, QObject* parent = nullptr);

    Q_PROPERTY(QmlObjectListModel*  slices              READ slices             CONSTANT)
    Q_PROPERTY(QList<int>           pulseRateCounts     READ pulseRateCounts    NOTIFY pulseRateCountsChanged)
    Q_PROPERTY(double               maxSNR              READ maxSNR             NOTIFY maxSNRChanged)
    Q_PROPERTY(double               bearingDeg          READ bearingDeg         NOTIFY bearingChanged)
    Q_PROPERTY(double               bearingUncertainty  READ bearingUncertainty NOTIFY bearingChanged)
    Q_PROPERTY(double               bearingRSquared     READ bearingRSquared    NOTIFY bearingChanged)
    Q_PROPERTY(bool                 bearingAmbiguous    READ bearingAmbiguous   NOTIFY bearingChanged)
    Q_PROPERTY(bool                 bearingValid        READ bearingValid       NOTIFY bearingChanged)

    QmlObjectListModel* slices(void) { return &_slices; }
    QList<int> pulseRateCounts(void) const { return _pulseRateCounts; }
    double maxSNR(void) const { return _maxSNR; }

    double bearingDeg(void) const { return _bearingDeg; }
    double bearingUncertainty(void) const { return _bearingUncertainty; }
    double bearingRSquared(void) const { return _bearingRSquared; }
    bool   bearingAmbiguous(void) const { return _bearingAmbiguous; }
    bool   bearingValid(void) const { return _bearingValid; }

    void pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo, bool enablePythonCrossRateCoalescing, double pendingPairTimeoutSeconds);
    void fitBearing(void);

signals:
    void pulseRateCountsChanged(void);
    void maxSNRChanged(double maxSNR);
    void bearingChanged(void);

private:
    static constexpr int cRates = 2;

    struct PendingRateResult {
        bool                            received = false;
        bool                            noDetection = false;
        uint32_t                        frequencyHz = 0;
        double                          snr = qQNaN();
        double                          startTimeSeconds = qQNaN();
        TunnelProtocol::PulseInfo_t     pulseInfo = {};
    };

    struct PendingPairResult {
        double pendingSinceSeconds = qQNaN();
        PendingRateResult rates[cRates];
    };

    int _sliceIndexForHeading(double normalizedHeading);
    void _applyPulseToSlice(const TunnelProtocol::PulseInfo_t& pulseInfo, int sliceIndex);
    void _updatePulseRateCount(const TunnelProtocol::PulseInfo_t& pulseInfo);
    void _updateMaxSNR(double sliceStrength);
    QString _sourceRateLabelForTagId(uint32_t tagId) const;
    quint64 _pairKey(uint32_t baseTagId, int sliceIndex) const;
    int _sliceIndexFromPairKey(quint64 key) const;
    uint32_t _baseTagIdFromPairKey(quint64 key) const;
    bool _isNoDetectionPulse(const TunnelProtocol::PulseInfo_t& pulseInfo) const;
    void _coalescePairAndApply(uint32_t baseTagId, int sliceIndex, const PendingPairResult& pendingPair);
    void _expireStalePendingPairs(double currentStartTimeSeconds, double pendingPairTimeoutSeconds);

    static double _antennaPattern(double thetaDeg, double phiDeg, double A, double B);

    int                 _cSlices = 0;
    QmlObjectListModel  _slices;
    QList<int>          _pulseRateCounts;
    double              _maxSNR = qQNaN();
    QHash<quint64, PendingPairResult> _pendingPairResults;

    // Bearing fit results
    double              _bearingDeg         = qQNaN();
    double              _bearingUncertainty = qQNaN();
    double              _bearingRSquared    = qQNaN();
    bool                _bearingAmbiguous   = false;
    bool                _bearingValid       = false;
};
