#include "PythonWaitForDetectionResultState.h"
#include "FunctionState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "OperationProgress.h"
#include "TunnelProtocol.h"

#include <QFinalState>

PythonWaitForDetectionResultState::PythonWaitForDetectionResultState(
    QState* parentState, uint32_t collectionId, uint32_t sliceId)
    : CustomState   ("PythonWaitForDetectionResultState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _collectionId (collectionId)
    , _sliceId      (sliceId)
{
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
    qCDebug(CustomPluginLog) << "Waiting for COLLECTION_STATUS collection_id:" << _collectionId
                             << "slice_id:" << _sliceId;
    connect(_customPlugin, &CustomPlugin::collectionStatusReceived,
            this, &PythonWaitForDetectionResultState::_collectionStatusReceived);
    OperationProgress* progress = _customPlugin->operationProgress();
    connect(progress, &OperationProgress::stalled,
            this, &PythonWaitForDetectionResultState::_progressStalled);
    connect(progress, &OperationProgress::finished,
            this, &PythonWaitForDetectionResultState::_progressFinished);
    _listening = true;

    // The controller can report completion before the slice command is acked
    const CollectionStatus_t& last = _customPlugin->lastCollectionStatus();
    _collectionStatusReceived(last.collection_id, last.slice_id, last.status, last.error_code);
    if (!_listening) {
        return;
    }

    // A FAILED frame that landed during the slice ack wait fired finished() before we connected
    if (progress->failed() && progress->command() == COMMAND_ID_START_COLLECTION) {
        _progressFinished(COMMAND_ID_START_COLLECTION, false, progress->message());
        return;
    }
    // Without a running rotation operation no stall can fire, so nothing would end this wait
    if (!progress->running() || progress->command() != COMMAND_ID_START_COLLECTION) {
        _disconnectAll();
        setError(QStringLiteral("Rotation stalled at slice %1: no progress from controller").arg(_sliceId));
        return;
    }
    // The yaw just finished was invisible to the controller; measure the stall from here.
    progress->restartStallWatch();
}

void PythonWaitForDetectionResultState::_progressStalled(uint32_t command, const QString& lastMessage)
{
    if (!_listening || command != COMMAND_ID_START_COLLECTION) {
        return;
    }
    _disconnectAll();
    setError(QStringLiteral("Rotation stalled at slice %1: %2").arg(_sliceId).arg(lastMessage));
}

// Once the rotation operation has ended no stall can fire, so the slice would otherwise wait forever
void PythonWaitForDetectionResultState::_progressFinished(uint32_t command, bool success, const QString& message)
{
    if (!_listening || command != COMMAND_ID_START_COLLECTION) {
        return;
    }
    _disconnectAll();
    setError(QStringLiteral("Rotation %1 while waiting for slice %2: %3")
                 .arg(success ? QStringLiteral("completed") : QStringLiteral("failed")).arg(_sliceId).arg(message));
}

void PythonWaitForDetectionResultState::_collectionStatusReceived(
    uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode)
{
    Q_UNUSED(errorCode);

    if (!_listening || collectionId != _collectionId || sliceId != _sliceId) {
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
    _listening = false;
    disconnect(_customPlugin, &CustomPlugin::collectionStatusReceived,
               this, &PythonWaitForDetectionResultState::_collectionStatusReceived);
    disconnect(_customPlugin->operationProgress(), &OperationProgress::stalled,
               this, &PythonWaitForDetectionResultState::_progressStalled);
    disconnect(_customPlugin->operationProgress(), &OperationProgress::finished,
               this, &PythonWaitForDetectionResultState::_progressFinished);
}
