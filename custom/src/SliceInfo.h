#pragma once

#include <QObject>
#include <QString>

class SliceInfo : public QObject
{
    Q_OBJECT

public:
    SliceInfo(int sliceIndex, double centerHeading, double sliceDegrees, QObject* parent = nullptr);

    Q_PROPERTY(double centerHeading READ centerHeading  CONSTANT)
    Q_PROPERTY(double sliceDegrees  READ sliceDegrees   CONSTANT)
    Q_PROPERTY(double maxSNR        READ maxSNR         NOTIFY maxSNRChanged)
    Q_PROPERTY(double displaySNR    READ displaySNR     NOTIFY displaySNRChanged)
    Q_PROPERTY(QString displaySource READ displaySource  NOTIFY displaySourceChanged)
    Q_PROPERTY(bool   lowConfidenceOnly READ lowConfidenceOnly NOTIFY lowConfidenceOnlyChanged)

    double centerHeading(void) const { return _centerHeading; }
    double sliceDegrees(void) const { return _sliceDegrees; }
    double maxSNR(void) const { return _maxSNR; }
    double displaySNR(void) const;
    QString displaySource(void) const;
    bool lowConfidenceOnly(void) const;
    void updateMaxSNR(double snr, bool confirmedPulse, const QString& sourceRateLabel);

signals:
    void maxSNRChanged(double maxSNR);
    void displaySNRChanged(double displaySNR);
    void displaySourceChanged(const QString& displaySource);
    void lowConfidenceOnlyChanged(bool lowConfidenceOnly);

private:
    int     _sliceIndex     = 0;
    double  _centerHeading  = 0.0;
    double  _sliceDegrees   = 0.0;
    double  _maxSNR         = qQNaN();
    double  _maxLowConfidenceSNR = qQNaN();
    QString _maxSNRSourceRateLabel;
    QString _maxLowConfidenceSourceRateLabel;
};
