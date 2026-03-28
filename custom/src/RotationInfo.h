#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

#include <QObject>
#include <QHash>

class RotationInfo : public QObject
{
    Q_OBJECT

public:
    RotationInfo(int cSlices, QObject* parent = nullptr);

    Q_PROPERTY(QmlObjectListModel*  slices          READ slices             CONSTANT)
    Q_PROPERTY(QList<int>           pulseRateCounts READ pulseRateCounts    NOTIFY pulseRateCountsChanged)
    Q_PROPERTY(double               maxSNR          READ maxSNR             NOTIFY maxSNRChanged)

    QmlObjectListModel* slices(void) { return &_slices; }
    QList<int> pulseRateCounts(void) const { return _pulseRateCounts; }
    double maxSNR(void) const { return _maxSNR; }

    void pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo, bool enablePythonCrossRateCoalescing, double pendingPairTimeoutSeconds);

signals:
    void pulseRateCountsChanged(void);
    void maxSNRChanged(double maxSNR);

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

    int                 _cSlices = 0;
    QmlObjectListModel  _slices;
    QList<int>          _pulseRateCounts;
    double              _maxSNR = qQNaN();
    QHash<quint64, PendingPairResult> _pendingPairResults;
};
