#include "PythonWaitForDetectionResultState.h"
#include "FunctionState.h"
#include "CustomPlugin.h"
#include "DetectorList.h"
#include "DetectorInfo.h"
#include "CustomLoggingCategory.h"

#include <QFinalState>

PythonWaitForDetectionResultState::PythonWaitForDetectionResultState(QState* parentState, int timeoutMsecs)
    : CustomState   ("PythonWaitForDetectionResultState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
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
    _pendingTagIds.clear();

    DetectorList* detectorList = DetectorList::instance();
    for (int i = 0; i < detectorList->count(); i++) {
        const DetectorInfo* detectorInfo = qobject_cast<const DetectorInfo*>(detectorList->get(i));
        if (detectorInfo) {
            _pendingTagIds.insert(detectorInfo->tagId());
        }
    }

    qCDebug(CustomStateMachineLog) << "Python: waiting for detection results from tag IDs:" << _pendingTagIds << " - " << Q_FUNC_INFO;

    if (_pendingTagIds.isEmpty()) {
        qCWarning(CustomStateMachineLog) << "PythonWaitForDetectionResultState: no detectors in list" << Q_FUNC_INFO;
        emit resultsReceived();
        return;
    }

    connect(_customPlugin, &CustomPlugin::pythonDetectorResultReceived,
            this, &PythonWaitForDetectionResultState::_pythonDetectorResultReceived);

    if (_timeoutTimer.interval() > 0) {
        _timeoutTimer.start();
    }
}

void PythonWaitForDetectionResultState::_pythonDetectorResultReceived(uint32_t tagId)
{
    if (_pendingTagIds.remove(tagId)) {
        qCDebug(CustomStateMachineLog) << "Python detection result for tag_id" << tagId
                                 << "- still pending:" << _pendingTagIds << " - " << Q_FUNC_INFO;
    }

    if (_pendingTagIds.isEmpty()) {
        qCDebug(CustomStateMachineLog) << "Python: all detection results received" << " - " << Q_FUNC_INFO;
        _disconnectAll();
        emit resultsReceived();
    }
}

void PythonWaitForDetectionResultState::_disconnectAll()
{
    _timeoutTimer.stop();
    disconnect(_customPlugin, &CustomPlugin::pythonDetectorResultReceived,
               this, &PythonWaitForDetectionResultState::_pythonDetectorResultReceived);
}
