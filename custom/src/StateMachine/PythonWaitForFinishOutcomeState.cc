#include "PythonWaitForFinishOutcomeState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"

using namespace TunnelProtocol;

PythonWaitForFinishOutcomeState::PythonWaitForFinishOutcomeState(
    QState* parentState, uint32_t collectionId, bool allowRevisit, int timeoutMsecs)
    : CustomState   ("PythonWaitForFinishOutcomeState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _collectionId (collectionId)
    , _allowRevisit (allowRevisit)
{
    _timeoutTimer.setSingleShot(true);
    _timeoutTimer.setInterval(timeoutMsecs);
    connect(&_timeoutTimer, &QTimer::timeout, this, [this] () {
        _disconnectAll();
        setError(QStringLiteral("No bearing result or revisit request from controller for collection %1")
                     .arg(_collectionId));
    });

    // Leaf state: the parent branches on revisitRequested/bearingReceived. No
    // transitions may be added here on those signals, or they shadow the parent's
    // (Qt takes one transition per signal event, innermost first).
    connect(this, &QState::entered, this, &PythonWaitForFinishOutcomeState::_startListening);
    connect(this, &QState::exited,  this, &PythonWaitForFinishOutcomeState::_disconnectAll);
}

void PythonWaitForFinishOutcomeState::_startListening()
{
    qCDebug(CustomStateMachineLog) << "Python: waiting for finish outcome of collection" << _collectionId;
    connect(_customPlugin, &CustomPlugin::collectionStatusReceived,
            this, &PythonWaitForFinishOutcomeState::_collectionStatusReceived);
    connect(_customPlugin, &CustomPlugin::bearingResultReceived,
            this, &PythonWaitForFinishOutcomeState::_bearingResultReceived);
    _listening = true;
    _timeoutTimer.start();

    // Both normally precede the FINISH_COLLECTION ack: replay what already arrived.
    const CollectionStatus_t& last = _customPlugin->lastCollectionStatus();
    _collectionStatusReceived(last.collection_id, last.slice_id, last.status, last.error_code);
    _bearingResultReceived(_customPlugin->lastBearingCollectionId());
    if (_listening) {
        qCDebug(CustomStateMachineLog) << "Python: no stored outcome for collection" << _collectionId
                                       << "- waiting up to" << _timeoutTimer.interval() << "ms";
    }
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
    qCDebug(CustomStateMachineLog) << "Python: controller requested a confirmation revisit at" << headingDeg << "deg";
    emit revisitRequested(headingDeg);
}

void PythonWaitForFinishOutcomeState::_bearingResultReceived(uint32_t collectionId)
{
    if (!_listening || collectionId != _collectionId) {
        return;
    }
    qCDebug(CustomStateMachineLog) << "Python: bearing result received for collection" << _collectionId;
    _disconnectAll();
    emit bearingReceived();
}

void PythonWaitForFinishOutcomeState::_disconnectAll()
{
    _listening = false;
    _timeoutTimer.stop();
    disconnect(_customPlugin, &CustomPlugin::collectionStatusReceived,
               this, &PythonWaitForFinishOutcomeState::_collectionStatusReceived);
    disconnect(_customPlugin, &CustomPlugin::bearingResultReceived,
               this, &PythonWaitForFinishOutcomeState::_bearingResultReceived);
}
