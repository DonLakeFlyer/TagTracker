#include "PythonWaitForFinishOutcomeState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "OperationProgress.h"
#include "TunnelProtocol.h"

using namespace TunnelProtocol;

PythonWaitForFinishOutcomeState::PythonWaitForFinishOutcomeState(
    QState* parentState, uint32_t collectionId, bool allowRevisit, int graceMsecs)
    : CustomState   ("PythonWaitForFinishOutcomeState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _collectionId (collectionId)
    , _allowRevisit (allowRevisit)
{
    _graceTimer.setSingleShot(true);
    _graceTimer.setInterval(graceMsecs);
    connect(&_graceTimer, &QTimer::timeout, this, [this] () {
        _outcomeMissing(QStringLiteral("rotation no longer running"));
    });

    // Leaf state: the parent branches on revisitRequested/bearingReceived. No
    // transitions may be added here on those signals, or they shadow the parent's
    // (Qt takes one transition per signal event, innermost first).
    connect(this, &QState::entered, this, &PythonWaitForFinishOutcomeState::_startListening);
    connect(this, &QState::exited,  this, &PythonWaitForFinishOutcomeState::_disconnectAll);
}

void PythonWaitForFinishOutcomeState::_startListening()
{
    qCDebug(CustomPluginLog) << "Waiting for finish outcome collection_id:" << _collectionId;
    connect(_customPlugin, &CustomPlugin::collectionStatusReceived,
            this, &PythonWaitForFinishOutcomeState::_collectionStatusReceived);
    connect(_customPlugin, &CustomPlugin::bearingResultReceived,
            this, &PythonWaitForFinishOutcomeState::_bearingResultReceived);
    OperationProgress* progress = _customPlugin->operationProgress();
    connect(progress, &OperationProgress::finished, this, &PythonWaitForFinishOutcomeState::_progressFinished);
    connect(progress, &OperationProgress::stalled,  this, &PythonWaitForFinishOutcomeState::_progressStalled);
    _listening = true;

    // Both normally precede the FINISH_COLLECTION ack: replay what already arrived.
    const CollectionStatus_t& last = _customPlugin->lastCollectionStatus();
    _collectionStatusReceived(last.collection_id, last.slice_id, last.status, last.error_code);
    _bearingResultReceived(_customPlugin->lastBearingCollectionId());
    if (!_listening) {
        return;
    }
    // A FAILED frame that landed during the FINISH ack wait fired finished() before we connected
    if (progress->failed() && progress->command() == COMMAND_ID_START_COLLECTION) {
        _progressFinished(COMMAND_ID_START_COLLECTION, false, progress->message());
        return;
    }
    // Once the rotation operation has ended or its frames were lost, nothing else
    // will fire; the grace timer then re-sends FINISH so the controller replays the outcome.
    if (progress->running() && progress->command() == COMMAND_ID_START_COLLECTION) {
        progress->restartStallWatch();
    } else {
        _graceTimer.start();
    }
    qCDebug(CustomPluginLog) << "No stored finish outcome collection_id:" << _collectionId;
}

void PythonWaitForFinishOutcomeState::_progressFinished(uint32_t command, bool success, const QString& message)
{
    if (!_listening || command != COMMAND_ID_START_COLLECTION) {
        return;
    }
    if (!success) {
        _disconnectAll();
        setError(QStringLiteral("Rotation failed on the controller for collection %1: %2").arg(_collectionId).arg(message));
        return;
    }
    _graceTimer.start();
}

void PythonWaitForFinishOutcomeState::_progressStalled(uint32_t command, const QString& lastMessage)
{
    if (!_listening || command != COMMAND_ID_START_COLLECTION) {
        return;
    }
    _outcomeMissing(QStringLiteral("rotation progress stalled at: %1").arg(lastMessage));
}

void PythonWaitForFinishOutcomeState::_outcomeMissing(const QString& reason)
{
    if (!_listening) {
        return;
    }
    _disconnectAll();
    if (_retryCount < kMaxRetries) {
        ++_retryCount;
        qCWarning(CustomPluginLog) << "No finish outcome (" << reason << "), re-sending FINISH_COLLECTION collection_id:" << _collectionId
                                   << "retry:" << _retryCount;
        emit outcomeTimedOut();
        return;
    }
    setError(QStringLiteral("No bearing result or revisit request from controller for collection %1 after %2 retries (%3)")
                 .arg(_collectionId).arg(_retryCount).arg(reason));
}

void PythonWaitForFinishOutcomeState::_collectionStatusReceived(
    uint32_t collectionId, uint32_t sliceId, uint32_t status, uint32_t errorCode)
{
    Q_UNUSED(sliceId);
    Q_UNUSED(errorCode);
    if (!_listening || collectionId != _collectionId || status != COLLECTION_STATUS_REVISIT_REQUESTED) {
        return;
    }
    const float headingDeg = _customPlugin->lastCollectionStatus().revisit_heading_deg;
    _disconnectAll();
    if (!_allowRevisit) {
        setError(QStringLiteral("Controller requested a second confirmation revisit for collection %1")
                     .arg(_collectionId));
        return;
    }
    qCDebug(CustomPluginLog) << "Controller requested confirmation revisit collection_id:" << _collectionId
                             << "revisit_heading_deg:" << headingDeg;
    emit revisitRequested(headingDeg);
}

void PythonWaitForFinishOutcomeState::_bearingResultReceived(uint32_t collectionId)
{
    if (!_listening || collectionId != _collectionId) {
        return;
    }
    qCDebug(CustomPluginLog) << "Finish outcome BEARING_RESULT collection_id:" << _collectionId;
    _disconnectAll();
    emit bearingReceived();
}

void PythonWaitForFinishOutcomeState::_disconnectAll()
{
    _listening = false;
    _graceTimer.stop();
    disconnect(_customPlugin, &CustomPlugin::collectionStatusReceived,
               this, &PythonWaitForFinishOutcomeState::_collectionStatusReceived);
    disconnect(_customPlugin, &CustomPlugin::bearingResultReceived,
               this, &PythonWaitForFinishOutcomeState::_bearingResultReceived);
    OperationProgress* progress = _customPlugin->operationProgress();
    disconnect(progress, &OperationProgress::finished, this, &PythonWaitForFinishOutcomeState::_progressFinished);
    disconnect(progress, &OperationProgress::stalled,  this, &PythonWaitForFinishOutcomeState::_progressStalled);
}
