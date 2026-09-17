#pragma once

#include "CustomState.h"

#include <QTimer>

class CustomPlugin;

// After FINISH_COLLECTION is acked, waits for the controller's verdict: either
// the BEARING_RESULT(s), or COLLECTION_STATUS_REVISIT_REQUESTED asking for one
// more slice at revisit_heading_deg before it will finalize. Both normally
// arrive before the ack, so the stored last status/bearing are replayed on
// entry. Neither has an ACK of its own, so if nothing arrives within the
// timeout the state emits outcomeTimedOut (up to kMaxRetries times) and the
// parent re-sends FINISH_COLLECTION, which makes the controller replay the
// outcome; after that it is a protocol error, as is a revisit request when
// allowRevisit is false (the controller asks at most once).
// Leaf state: the parent must transition on revisitRequested/bearingReceived/outcomeTimedOut.
class PythonWaitForFinishOutcomeState : public CustomState
{
    Q_OBJECT

public:
    PythonWaitForFinishOutcomeState(QState* parentState, uint32_t collectionId, bool allowRevisit, int timeoutMsecs = kDefaultTimeoutMsecs);

    static constexpr int kDefaultTimeoutMsecs = 5000;
    static constexpr int kMaxRetries = 2;

signals:
    void revisitRequested(float headingDeg);
    void bearingReceived();
    void outcomeTimedOut();

private slots:
    void _collectionStatusReceived(uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode);
    void _bearingResultReceived(uint32_t collectionId);

private:
    void _startListening();
    void _disconnectAll();

    CustomPlugin*   _customPlugin   = nullptr;
    uint32_t        _collectionId   = 0;
    bool            _allowRevisit   = true;
    bool            _listening      = false;
    int             _retryCount     = 0;
    QTimer          _timeoutTimer;
};
