#pragma once

#include "CustomState.h"

#include <QTimer>

class CustomPlugin;

// After FINISH_COLLECTION is acked, waits for the controller's verdict: either
// the BEARING_RESULT(s), or COLLECTION_STATUS_REVISIT_REQUESTED asking for one
// more slice at revisit_heading_deg before it will finalize. Both normally
// arrive before the ack, so the stored last status/bearing are replayed on
// entry. Neither has an ACK of its own. The controller's rotation
// OPERATION_PROGRESS tells us when to give up waiting: once it reports the
// rotation COMPLETE (finalize done, so the outcome was already sent) a short
// grace period covers outcome frames trailing it on the link; if instead its
// step stalls (revisit requested and step parked, or the controller hung) the
// stall window already dwarfs that grace, so there is no further wait. Either
// way outcomeTimedOut is then emitted (up to kMaxRetries times) so the parent
// re-sends FINISH_COLLECTION, which makes the controller replay the outcome;
// after that it is a protocol error, as is a revisit request when allowRevisit
// is false (the controller asks at most once).
// Leaf state: the parent must transition on revisitRequested/bearingReceived/outcomeTimedOut.
class PythonWaitForFinishOutcomeState : public CustomState
{
    Q_OBJECT

public:
    PythonWaitForFinishOutcomeState(QState* parentState, uint32_t collectionId, bool allowRevisit, int graceMsecs = kDefaultGraceMsecs);

    // Outcome frames precede the progress COMPLETE frame; this covers reordering on the link.
    static constexpr int kDefaultGraceMsecs = 2000;
    static constexpr int kMaxRetries = 2;

signals:
    void revisitRequested(float headingDeg);
    void bearingReceived();
    void outcomeTimedOut();

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);
    void _bearingResultReceived(uint32_t collectionId);
    void _progressFinished(uint32_t command, bool success, const QString& message);
    void _progressStalled(uint32_t command, const QString& lastMessage);

private:
    void _startListening();
    void _disconnectAll();
    void _outcomeMissing(const QString& reason);

    CustomPlugin*   _customPlugin   = nullptr;
    uint32_t        _collectionId   = 0;
    bool            _allowRevisit   = true;
    bool            _listening      = false;
    int             _retryCount     = 0;
    QTimer          _graceTimer;
};
