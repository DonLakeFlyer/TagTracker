#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

#include <QObject>

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

    void pulseInfoReceived(const TunnelProtocol::PulseInfo_t& pulseInfo);

signals:
    void pulseRateCountsChanged(void);
    void maxSNRChanged(double maxSNR);

private:
    int                 _cSlices = 0;
    QmlObjectListModel  _slices;
    QList<int>          _pulseRateCounts;
    double              _maxSNR = 0.0;

    static const int cRates = 2;
};