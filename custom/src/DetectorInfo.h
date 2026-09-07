#pragma once

#include <QObject>
#include <QTimer>

#include "QGCLoggingCategory.h"

Q_DECLARE_LOGGING_CATEGORY(DetectorInfoLog)

/// Per-detector state shown in the fly view (strength bar, heartbeat, rate label).
/// Subclasses decode the detector-specific tunnel pulse message.
class DetectorInfo : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(int tagId MEMBER _tagId CONSTANT)
    Q_PROPERTY(QString tagLabel MEMBER _tagLabel CONSTANT)
    Q_PROPERTY(QString rateLabel MEMBER _rateLabel NOTIFY rateLabelChanged)
    Q_PROPERTY(bool heartbeatLost MEMBER _heartbeatLost NOTIFY heartbeatLostChanged)
    Q_PROPERTY(double lastPulseStrength MEMBER _lastPulseStrength NOTIFY lastPulseStrengthChanged)
    Q_PROPERTY(bool lastPulseLowConfidence MEMBER _lastPulseLowConfidence NOTIFY lastPulseLowConfidenceChanged)
    Q_PROPERTY(bool lastPulseNoPulse MEMBER _lastPulseNoPulse NOTIFY lastPulseNoPulseChanged)
    Q_PROPERTY(int noPulseCount MEMBER _noPulseCount NOTIFY noPulseCountChanged)
    Q_PROPERTY(bool waitingForFirstPulse MEMBER _waitingForFirstPulse NOTIFY waitingForFirstPulseChanged)

    uint32_t tagId() const { return _tagId; }

    /// Arms the first-heartbeat watchdog. Called when the detector is expected to start reporting.
    void startHeartbeatWatchdog();
    bool heartbeatWatchdogActive() const { return _heartbeatTimeoutTimer.isActive(); }
    int  heartbeatWatchdogRemainingMsecs() const { return _heartbeatTimeoutTimer.remainingTime(); }

signals:
    void heartbeatLostChanged();
    void lastPulseStrengthChanged();
    void lastPulseLowConfidenceChanged();
    void lastPulseNoPulseChanged();
    void noPulseCountChanged();
    void rateLabelChanged();
    void waitingForFirstPulseChanged();

protected:
    /// k: pulses per detector cycle; the heartbeat timeout is (k + 1) intra-pulse periods.
    DetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k, QObject* parent);

    void _heartbeatReceived();
    /// newGroup: first pulse of a new K group resets the running max; otherwise the group max is kept.
    void _pulseReceived(double snr, bool lowConfidence, bool newGroup);
    void _noPulseReceived();
    void _setRateLabel(const QString& rateLabel);

private:
    uint32_t _tagId = 0;
    QString _tagLabel;
    QString _rateLabel;
    bool _heartbeatLost = false;
    double _lastPulseStrength = 0.0;
    bool _lastPulseLowConfidence = false;
    bool _lastPulseNoPulse = false;
    bool _waitingForFirstPulse = true;
    int _noPulseCount = 0;
    uint32_t _heartbeatTimerInterval = 0;
    bool _watchdogArmed = false;
    QTimer _heartbeatTimeoutTimer;
};
