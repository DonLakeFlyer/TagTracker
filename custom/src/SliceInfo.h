#pragma once

#include <QObject>

class SliceInfo : public QObject
{
    Q_OBJECT

public:
    SliceInfo(int sliceIndex, double centerHeading, double sliceDegrees, QObject* parent = nullptr);

    Q_PROPERTY(double centerHeading READ centerHeading  CONSTANT)
    Q_PROPERTY(double sliceDegrees  READ sliceDegrees   CONSTANT)
    Q_PROPERTY(double maxSNR        READ maxSNR         NOTIFY maxSNRChanged)

    double centerHeading(void) const { return _centerHeading; }
    double sliceDegrees(void) const { return _sliceDegrees; }
    double maxSNR(void) const { return _maxSNR; }
    void updateMaxSNR(double snr);

signals:
    void maxSNRChanged(double maxSNR);

private:
    int     _sliceIndex     = 0;
    double  _centerHeading  = 0.0;
    double  _sliceDegrees   = 0.0;
    double  _maxSNR         = qQNaN();
};