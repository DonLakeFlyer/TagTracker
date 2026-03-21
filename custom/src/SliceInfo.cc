#include "SliceInfo.h"
#include "CustomLoggingCategory.h"

SliceInfo::SliceInfo(int sliceIndex, double centerHeading, double sliceDegrees, QObject* parent)
    : QObject       (parent)
    , _sliceIndex   (sliceIndex)
    , _centerHeading(centerHeading)
    , _sliceDegrees (sliceDegrees)
{
    //qDebug() << "SliceInfo constructor - index:centerHeading:sliceDegress" << _sliceIndex << _centerHeading << _sliceDegrees << " _ " << Q_FUNC_INFO;
}

double SliceInfo::displaySNR(void) const
{
    return qIsNaN(_maxSNR) ? _maxLowConfidenceSNR : _maxSNR;
}

bool SliceInfo::lowConfidenceOnly(void) const
{
    return qIsNaN(_maxSNR) && !qIsNaN(_maxLowConfidenceSNR);
}

void SliceInfo::updateMaxSNR(double snr, bool confirmedPulse)
{
    qCDebug(CustomPluginLog) << "SliceInfo updateMaxSNR called - index:snr:confirmed"
                             << _sliceIndex << snr << confirmedPulse << " _ " << Q_FUNC_INFO;

    const double oldDisplaySNR = displaySNR();
    const bool oldLowConfidenceOnly = lowConfidenceOnly();

    if (confirmedPulse) {
        if (!qIsNaN(_maxSNR) && snr <= _maxSNR) {
            return;
        }

        qCDebug(CustomPluginLog) << "Updating SliceInfo CONFIRMED max SNR - index:centerHeading:sliceDegress:maxSnr"
                                 << _sliceIndex << _centerHeading << _sliceDegrees << snr << " _ " << Q_FUNC_INFO;
        _maxSNR = snr;
        emit maxSNRChanged(_maxSNR);
    } else {
        if (!qIsNaN(_maxLowConfidenceSNR) && snr <= _maxLowConfidenceSNR) {
            return;
        }

        qCDebug(CustomPluginLog) << "Updating SliceInfo LOW-CONFIDENCE max SNR - index:centerHeading:sliceDegress:maxSnr"
                                 << _sliceIndex << _centerHeading << _sliceDegrees << snr << " _ " << Q_FUNC_INFO;
        _maxLowConfidenceSNR = snr;
    }

    const double newDisplaySNR = displaySNR();
    const bool newLowConfidenceOnly = lowConfidenceOnly();

    if (oldDisplaySNR != newDisplaySNR) {
        emit displaySNRChanged(newDisplaySNR);
    }
    if (oldLowConfidenceOnly != newLowConfidenceOnly) {
        emit lowConfidenceOnlyChanged(newLowConfidenceOnly);
    }
}
