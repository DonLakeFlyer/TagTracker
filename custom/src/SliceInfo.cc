#include "SliceInfo.h"
#include "CustomLoggingCategory.h"

SliceInfo::SliceInfo(int sliceIndex, double centerHeading, double sliceDegrees, QObject* parent)
    : QObject       (parent)
    , _sliceIndex   (sliceIndex)
    , _centerHeading(centerHeading)
    , _sliceDegrees (sliceDegrees)
{
    //qDebug() << "SliceInfo constructor - index:centerHeading:sliceDegrees" << _sliceIndex << _centerHeading << _sliceDegrees << " _ " << Q_FUNC_INFO;
}

double SliceInfo::displaySNR(void) const
{
    return qIsNaN(_maxSNR) ? _maxLowConfidenceSNR : _maxSNR;
}

QString SliceInfo::displaySource(void) const
{
    const QString source = qIsNaN(_maxSNR) ? _maxLowConfidenceSourceRateLabel : _maxSNRSourceRateLabel;
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    // Compound rate-switch labels (e.g. "R/M") are already abbreviated
    if (trimmed.contains(QLatin1Char('/'))) {
        return trimmed;
    }
    return trimmed.left(1);
}

bool SliceInfo::lowConfidenceOnly(void) const
{
    return qIsNaN(_maxSNR) && !qIsNaN(_maxLowConfidenceSNR);
}

void SliceInfo::updateMaxSNR(uint32_t tagId, double snr, bool confirmedPulse, const QString& sourceRateLabel)
{
    qCDebug(CustomPluginLog) << "sliceIndex:" << _sliceIndex
                             << "tag_id:" << tagId
                             << "snr:" << snr
                             << "confirmed:" << confirmedPulse;

    const double oldDisplaySNR = displaySNR();
    const QString oldDisplaySource = displaySource();
    const bool oldLowConfidenceOnly = lowConfidenceOnly();

    if (confirmedPulse) {
        // Latest confirmed value per tag wins: a locked re-measurement supersedes that
        // tag's provisional value, matching the controller's (tag_id, slice_id) upsert.
        auto it = _confirmedByTag.find(tagId);
        if (it != _confirmedByTag.end() && it->snr == snr && it->sourceRateLabel == sourceRateLabel) {
            return;
        }

        qCDebug(CustomPluginLog) << "Updating confirmed SNR sliceIndex:" << _sliceIndex
                                 << "centerHeading:" << _centerHeading
                                 << "sliceDegrees:" << _sliceDegrees
                                 << "tag_id:" << tagId
                                 << "snr:" << snr;
        _confirmedByTag.insert(tagId, {snr, sourceRateLabel});
        _recomputeConfirmedMax();
    } else {
        if (!qIsNaN(_maxLowConfidenceSNR) && snr <= _maxLowConfidenceSNR) {
            return;
        }

        qCDebug(CustomPluginLog) << "Updating low-confidence max SNR sliceIndex:" << _sliceIndex
                                 << "centerHeading:" << _centerHeading
                                 << "sliceDegrees:" << _sliceDegrees
                                 << "snr:" << snr;
        _maxLowConfidenceSNR = snr;
        _maxLowConfidenceSourceRateLabel = sourceRateLabel;
    }

    const double newDisplaySNR = displaySNR();
    const QString newDisplaySource = displaySource();
    const bool newLowConfidenceOnly = lowConfidenceOnly();

    if (oldDisplaySNR != newDisplaySNR) {
        emit displaySNRChanged(newDisplaySNR);
    }
    if (oldDisplaySource != newDisplaySource) {
        emit displaySourceChanged(newDisplaySource);
    }
    if (oldLowConfidenceOnly != newLowConfidenceOnly) {
        emit lowConfidenceOnlyChanged(newLowConfidenceOnly);
    }
}

void SliceInfo::_recomputeConfirmedMax()
{
    double newMax = qQNaN();
    QString newLabel;
    for (auto it = _confirmedByTag.cbegin(); it != _confirmedByTag.cend(); ++it) {
        if (qIsNaN(newMax) || it->snr > newMax) {
            newMax = it->snr;
            newLabel = it->sourceRateLabel;
        }
    }
    if (newMax != _maxSNR || newLabel != _maxSNRSourceRateLabel) {
        _maxSNR = newMax;
        _maxSNRSourceRateLabel = newLabel;
        emit maxSNRChanged(_maxSNR);
    }
}
