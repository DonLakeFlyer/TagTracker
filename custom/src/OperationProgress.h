#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include "TunnelProtocol.h"

/// Mirrors the controller's single long-running operation (OPERATION_PROGRESS
/// tunnel frames) for the fly view progress card. The controller re-sends the
/// RUNNING frame at 1 Hz, so a missing update for a few seconds means the link
/// or the controller is gone and the card is hidden; COMPLETE / FAILED linger
/// briefly so the operator sees the outcome.
///
/// A rotation is one operation under START_COLLECTION whose step advances only
/// on real work (detector ready/armed, seconds of IQ collected, slice complete).
/// A RUNNING frame whose step has not moved for stallMSecs therefore means the
/// rotation is hung, and stalled() is emitted so the state machine can cancel.
class OperationProgress : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool     active      READ active      NOTIFY changed)
    Q_PROPERTY(QString  title       READ title       NOTIFY changed)
    Q_PROPERTY(QString  message     READ message     NOTIFY changed)
    Q_PROPERTY(double   fraction    READ fraction    NOTIFY changed)   ///< 0..1, or -1 when indeterminate
    Q_PROPERTY(bool     running     READ running     NOTIFY changed)
    Q_PROPERTY(bool     failed      READ failed      NOTIFY changed)

public:
    explicit OperationProgress(QObject* parent = nullptr);

    bool     active()    const { return _active; }
    uint32_t command()   const { return _command; }
    QString  title()     const { return _title; }
    QString  message()   const { return _message; }
    double   fraction()  const;
    bool     running()   const { return _state == OPERATION_STATE_RUNNING; }
    bool     failed()    const { return _state == OPERATION_STATE_FAILED; }

    void handleFrame(const TunnelProtocol::OperationProgress_t& frame);
    /// Hides the card immediately (controller heartbeat lost).
    void reset();
    /// Gives a RUNNING operation a fresh stall window. Called when the GCS
    /// starts waiting on the controller after a phase (yaw) the controller
    /// could not see and during which the step legitimately stood still.
    void restartStallWatch();
    /// Test hook: shorten the stale / linger / stall timeouts.
    void setTimeoutsForTest(int staleMSecs, int lingerMSecs, int stallMSecs = 0);

    static QString titleForCommand(uint32_t command);

    static constexpr int kDefaultStallMSecs = 10000;

signals:
    void changed();
    /// A RUNNING rotation's step has not advanced for stallMSecs (or its frames stopped).
    void stalled(uint32_t command, const QString& lastMessage);
    /// The operation reached COMPLETE or FAILED.
    void finished(uint32_t command, bool success, const QString& message);

private:
    void _hide();
    void _stall();

    int _staleMSecs = 5000;  // > 4 missed 1 Hz re-sends
    int _lingerMSecs = 30000;
    int _stallMSecs = kDefaultStallMSecs;

    bool     _active  = false;
    uint32_t _command = 0;
    uint32_t _requestId = 0;
    uint32_t _state   = 0;
    uint32_t _step    = 0;
    uint32_t _stepCount = 0;
    QString  _title;
    QString  _message;
    QTimer   _hideTimer;
    QTimer   _stallTimer;
};
