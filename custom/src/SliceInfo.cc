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

    const DisplayState old = _displayState();

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
        auto it = _lowConfidenceByTag.find(tagId);
        if (it != _lowConfidenceByTag.end() && snr <= it->snr) {
            return;
        }

        qCDebug(CustomPluginLog) << "Updating low-confidence max SNR sliceIndex:" << _sliceIndex
                                 << "centerHeading:" << _centerHeading
                                 << "sliceDegrees:" << _sliceDegrees
                                 << "tag_id:" << tagId
                                 << "snr:" << snr;
        _lowConfidenceByTag.insert(tagId, {snr, sourceRateLabel});
        _recomputeLowConfidenceMax();
    }

    _emitDisplayChanges(old);
}

void SliceInfo::clearTag(uint32_t tagId)
{
    if (!_confirmedByTag.contains(tagId) && !_lowConfidenceByTag.contains(tagId)) {
        return;
    }

    qCDebug(CustomPluginLog) << "Clearing tag SNR sliceIndex:" << _sliceIndex << "tag_id:" << tagId;

    const DisplayState old = _displayState();

    if (_confirmedByTag.remove(tagId)) {
        _recomputeConfirmedMax();
    }
    if (_lowConfidenceByTag.remove(tagId)) {
        _recomputeLowConfidenceMax();
    }

    _emitDisplayChanges(old);
}

SliceInfo::DisplayState SliceInfo::_displayState() const
{
    return {displaySNR(), displaySource(), lowConfidenceOnly()};
}

void SliceInfo::_emitDisplayChanges(const DisplayState& old)
{
    const DisplayState now = _displayState();
    // NaN != NaN: compare emptiness explicitly so a cleared slice still notifies.
    if (qIsNaN(old.snr) != qIsNaN(now.snr) || (!qIsNaN(now.snr) && old.snr != now.snr)) {
        emit displaySNRChanged(now.snr);
    }
    if (old.source != now.source) {
        emit displaySourceChanged(now.source);
    }
    if (old.lowConfidenceOnly != now.lowConfidenceOnly) {
        emit lowConfidenceOnlyChanged(now.lowConfidenceOnly);
    }
}

SliceInfo::TagValue SliceInfo::_maxOf(const QHash<uint32_t, TagValue>& values)
{
    TagValue max {qQNaN(), QString()};
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (qIsNaN(max.snr) || it->snr > max.snr) {
            max = *it;
        }
    }
    return max;
}

void SliceInfo::_recomputeLowConfidenceMax()
{
    const TagValue max = _maxOf(_lowConfidenceByTag);
    _maxLowConfidenceSNR = max.snr;
    _maxLowConfidenceSourceRateLabel = max.sourceRateLabel;
}

void SliceInfo::_recomputeConfirmedMax()
{
    const TagValue max = _maxOf(_confirmedByTag);
    const double newMax = max.snr;
    const QString& newLabel = max.sourceRateLabel;
    if (newMax != _maxSNR || newLabel != _maxSNRSourceRateLabel) {
        _maxSNR = newMax;
        _maxSNRSourceRateLabel = newLabel;
        emit maxSNRChanged(_maxSNR);
    }
}
