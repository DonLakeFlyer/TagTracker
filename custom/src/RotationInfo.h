#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

#include <QObject>
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

    void pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo);
    void fitBearing(void);

signals:
    void pulseRateCountsChanged(void);
    void maxSNRChanged(double maxSNR);
    void bearingChanged(void);

private:
    static constexpr int cRates = 2;

    int _sliceIndexForHeading(double normalizedHeading);
    void _applyPulseToSlice(const TunnelProtocol::PulseInfo_t& pulseInfo, int sliceIndex);
    void _updatePulseRateCount(const TunnelProtocol::PulseInfo_t& pulseInfo);
    void _updateMaxSNR(double sliceStrength);
    QString _sourceRateLabelForTagId(uint32_t tagId) const;
    QString _rateLabelFromGroupInd(const TunnelProtocol::PulseInfo_t& pulseInfo) const;
    bool _isNoDetectionPulse(const TunnelProtocol::PulseInfo_t& pulseInfo) const;

    static double _antennaPattern(double thetaDeg, double phiDeg, double A, double B);

    int                 _cSlices = 0;
    QmlObjectListModel  _slices;
    QList<int>          _pulseRateCounts;
    double              _maxSNR = qQNaN();

    // Bearing fit results
    double              _bearingDeg         = qQNaN();
    double              _bearingUncertainty = qQNaN();
    double              _bearingRSquared    = qQNaN();
    bool                _bearingAmbiguous   = false;
    bool                _bearingValid       = false;
};
