#pragma once

#include "QGCLoggingCategory.h"
#include "TunnelProtocol.h"
#include "QGCMAVLink.h"

#include <QObject>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(DetectorInfoLog)

class DetectorInfo : public QObject
{
    Q_OBJECT

public:
    DetectorInfo(uint32_t tagId, const QString& tagLabel, uint32_t intraPulseMsecs, uint32_t k, QObject* parent = nullptr);
    ~DetectorInfo();

    Q_PROPERTY(int      tagId               MEMBER _tagId               CONSTANT)
    Q_PROPERTY(QString  tagLabel            MEMBER _tagLabel            CONSTANT)
    Q_PROPERTY(QString  rateLabel           MEMBER _rateLabel           NOTIFY rateLabelChanged)
    Q_PROPERTY(bool     heartbeatLost       MEMBER _heartbeatLost       NOTIFY heartbeatLostChanged)
    Q_PROPERTY(double   lastPulseStrength   MEMBER _lastPulseStrength   NOTIFY lastPulseStrengthChanged)
    Q_PROPERTY(bool     lastPulseLowConfidence MEMBER _lastPulseLowConfidence NOTIFY lastPulseLowConfidenceChanged)
    Q_PROPERTY(bool     lastPulseNoPulse    MEMBER _lastPulseNoPulse    NOTIFY lastPulseNoPulseChanged)
    Q_PROPERTY(int      noPulseCount        MEMBER _noPulseCount        NOTIFY noPulseCountChanged)
    Q_PROPERTY(bool     waitingForFirstPulse MEMBER _waitingForFirstPulse NOTIFY waitingForFirstPulseChanged)

    void        handleTunnelPulse   (const mavlink_tunnel_t& tunnel);
    uint32_t    tagId               () const                        { return _tagId; }
    void        resetMaxStrength    ()                              { _maxStrength = 0.0; }
    void        resetHeartbeatCount ()                              { _heartbeatCount = 0; }
    void        resetPulseGroupCount()                              { _pulseGroupCount = 0; }
    double      maxStrength              () const                   { return _maxStrength; }
    uint32_t    heartbeatCount      () const                        { return _heartbeatCount; }
    uint32_t    pulseGroupCount     () const                        { return _pulseGroupCount; }

signals:
    void heartbeatLostChanged       ();
    void lastPulseStrengthChanged   ();
    void lastPulseLowConfidenceChanged();
    void lastPulseNoPulseChanged     ();
    void noPulseCountChanged          ();
    void rateLabelChanged             ();
    void waitingForFirstPulseChanged  ();

private:
    uint32_t        _tagId                  = 0;
    QString         _tagLabel;
    QString         _rateLabel;
    uint32_t        _intraPulseMsecs        = 0;
    uint32_t        _k                      = 0;
    bool            _heartbeatLost          = true;
    double          _lastPulseStrength      = 0.0;
    bool            _lastPulseLowConfidence = false;
    int             _lastPulseGroupSeqCtr   = -1;
    bool            _lastPulseNoPulse       = false;
    bool            _waitingForFirstPulse   = true;
    int             _noPulseCount           = 0;
    uint32_t        _heartbeatTimerInterval = 0;
    QTimer          _heartbeatTimeoutTimer;
    double          _maxStrength            = 0.0;
    uint32_t        _heartbeatCount         = 0;
    uint32_t        _pulseGroupCount        = 0;
};
