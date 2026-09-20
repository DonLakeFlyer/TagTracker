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

    bool    active()    const { return _active; }
    QString title()     const { return _title; }
    QString message()   const { return _message; }
    double  fraction()  const;
    bool    running()   const { return _state == OPERATION_STATE_RUNNING; }
    bool    failed()    const { return _state == OPERATION_STATE_FAILED; }

    void handleFrame(const TunnelProtocol::OperationProgress_t& frame);
    /// Hides the card immediately (controller heartbeat lost).
    void reset();
    /// Test hook: shorten the stale / linger timeouts.
    void setTimeoutsForTest(int staleMSecs, int lingerMSecs);

    static QString titleForCommand(uint32_t command);

signals:
    void changed();

private:
    void _hide();

    int _staleMSecs = 5000;  // > 4 missed 1 Hz re-sends
    int _lingerMSecs = 30000;

    bool     _active  = false;
    uint32_t _command = 0;
    uint32_t _requestId = 0;
    uint32_t _state   = 0;
    uint32_t _step    = 0;
    uint32_t _stepCount = 0;
    QString  _title;
    QString  _message;
    QTimer   _hideTimer;
};
