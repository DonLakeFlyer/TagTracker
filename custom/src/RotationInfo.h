#pragma once

#include <QObject>
#include <QString>

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

/// Per-rotation heading-slice signal strengths and the controller-computed bearing (Python detector only).
class RotationInfo : public QObject
{
    Q_OBJECT

public:
    RotationInfo(int cSlices, QObject* parent = nullptr);

    // Operator-facing outcome of a finished rotation
    enum BearingState {
        NothingHeard,   // no bearing: nothing detected, or no lock candidate fitted the pattern
        Unconfirmed,    // bearing from a lock seen on one heading only
        Confirmed,      // lock seen independently on >= 2 headings (or the revisit slice)
    };
    Q_ENUM(BearingState)

    Q_PROPERTY(QmlObjectListModel* slices READ slices CONSTANT)
    Q_PROPERTY(QList<int> pulseRateCounts READ pulseRateCounts NOTIFY pulseRateCountsChanged)
    Q_PROPERTY(double maxSNR READ maxSNR NOTIFY maxSNRChanged)
    Q_PROPERTY(double bearingDeg READ bearingDeg NOTIFY bearingChanged)
    Q_PROPERTY(double bearingRSquared READ bearingRSquared NOTIFY bearingChanged)
    Q_PROPERTY(bool bearingValid READ bearingValid NOTIFY bearingChanged)
    Q_PROPERTY(bool bearingConfirmed READ bearingConfirmed NOTIFY bearingChanged)
    Q_PROPERTY(bool bearingReceived READ bearingReceived NOTIFY bearingChanged)
    Q_PROPERTY(BearingState bearingState READ bearingState NOTIFY bearingChanged)
    Q_PROPERTY(QString bearingStateText READ bearingStateText NOTIFY bearingChanged)
    Q_PROPERTY(int bearingSector READ bearingSector NOTIFY bearingChanged)

    QmlObjectListModel* slices(void) { return &_slices; }

    QList<int> pulseRateCounts(void) const { return _pulseRateCounts; }

    double maxSNR(void) const { return _maxSNR; }

    double bearingDeg(void) const { return _bearingDeg; }

    double bearingRSquared(void) const { return _bearingRSquared; }

    bool bearingValid(void) const { return _bearingValid; }

    // The winning lock was found independently on >= 2 headings (or on the revisit slice)
    bool bearingConfirmed(void) const { return _bearingConfirmed; }

    // A BEARING_RESULT has arrived for this rotation (so "nothing heard" is a result, not a wait)
    bool bearingReceived(void) const { return _bearingReceived; }

    BearingState bearingState(void) const;
    QString bearingStateText(void) const;

    // Index of the rose slice the bearing falls in (same frame as the slices), -1 when no bearing
    int bearingSector(void) const;
    // Slice index for a heading in the rose frame; sliceCount <= 0 gives -1
    static int sectorForHeading(double headingDeg, int sliceCount);

    void pulseReceived(const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
    void setBearingResult(float bearingDeg, float rSquared, uint32_t nValidSlices, float bestSNR, bool confirmed);

    /// Slice strength: signal_psd as dB above the reported noise floor. NaN when noise_psd is invalid
    /// (not a measurement); 0 when the noise-subtracted power is <= 0 (still a locked measurement).
    static double displayStrength(double signalPsd, double noisePsd);

signals:
    void pulseRateCountsChanged(void);
    void maxSNRChanged(double maxSNR);
    void bearingChanged(void);

private:
    static constexpr int cRates = 2;

    int _sliceIndexForHeading(double normalizedHeading);
    void _applyPulseToSlice(const TunnelProtocol::PythonPulseInfo_t& pulseInfo, int sliceIndex);
    void _updateMaxSNR();

    int _cSlices = 0;
    QmlObjectListModel _slices;
    QList<int> _pulseRateCounts;
    double _maxSNR = qQNaN();

    double _bearingDeg = qQNaN();
    double _bearingRSquared = qQNaN();
    bool _bearingValid = false;
    bool _bearingConfirmed = false;
    bool _bearingReceived = false;
};
