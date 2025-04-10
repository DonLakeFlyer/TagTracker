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

void SliceInfo::updateMaxSNR(double snr)
{
    qCDebug(CustomPluginLog) << "SliceInfo updateMaxSNR called - index:snr" << _sliceIndex << snr << " _ " << Q_FUNC_INFO;
    if (qIsNaN(_maxSNR) || snr > _maxSNR) {
        qCDebug(CustomPluginLog) << "Updating SliceInfo max SNR - index:centerHeading:sliceDegress:maxSnr" << _sliceIndex << _centerHeading << _sliceDegrees << snr << " _ " << Q_FUNC_INFO;
        _maxSNR = snr;
        emit maxSNRChanged(_maxSNR);
    }
}