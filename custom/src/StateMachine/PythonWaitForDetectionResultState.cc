#include "PythonWaitForDetectionResultState.h"
#include "FunctionState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"

#include <QFinalState>

PythonWaitForDetectionResultState::PythonWaitForDetectionResultState(
    QState* parentState, uint32_t collectionId, uint32_t sliceId, int timeoutMsecs)
    : CustomState   ("PythonWaitForDetectionResultState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _collectionId (collectionId)
    , _sliceId      (sliceId)
{
    _timeoutTimer.setSingleShot(true);
    if (timeoutMsecs > 0) {
        _timeoutTimer.setInterval(timeoutMsecs);
        connect(&_timeoutTimer, &QTimer::timeout, this, [this] () {
            _disconnectAll();
            setError("Timeout waiting for Python detection result");
        });
    }

    // An inner FunctionState is used as the initial state so that _startListening()
    // is called the moment this composite state is entered.  Once all expected
    // detectors have reported (or if the list is empty), resultsReceived is emitted
    // which drives the transition to finalState.
    auto listenState = new FunctionState("StartListening", this, [this] () { _startListening(); });
    auto finalState  = new QFinalState(this);

    listenState->addTransition(this, &PythonWaitForDetectionResultState::resultsReceived, finalState);

    // Ensure we always clean up the signal connection even if the state machine
    // is cancelled or errors out while we are waiting
    connect(this, &QState::exited, this, &PythonWaitForDetectionResultState::_disconnectAll);

    setInitialState(listenState);
}

void PythonWaitForDetectionResultState::_startListening()
{
    qCDebug(CustomStateMachineLog) << "Python: waiting for collection slice"
                                   << _collectionId << _sliceId << Q_FUNC_INFO;
    connect(_customPlugin, &CustomPlugin::collectionStatusReceived,
            this, &PythonWaitForDetectionResultState::_collectionStatusReceived);

    if (_timeoutTimer.interval() > 0) {
        _timeoutTimer.start();
    }

    // The controller can report completion before the slice command is acked
    const CollectionStatus_t& last = _customPlugin->lastCollectionStatus();
    _collectionStatusReceived(last.collection_id, last.slice_id, last.status, last.error_code);
}

void PythonWaitForDetectionResultState::_collectionStatusReceived(
    uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode)
{
    Q_UNUSED(errorCode);

    if (collectionId != _collectionId || sliceId != _sliceId) {
        return;
    }
    // Collection-wide FAILED is handled by PythonRotateAndCaptureState
    if (status == COLLECTION_STATUS_SLICE_COMPLETE) {
        _disconnectAll();
        emit resultsReceived();
    }
}

void PythonWaitForDetectionResultState::_disconnectAll()
{
    _timeoutTimer.stop();
    disconnect(_customPlugin, &CustomPlugin::collectionStatusReceived,
               this, &PythonWaitForDetectionResultState::_collectionStatusReceived);
}
