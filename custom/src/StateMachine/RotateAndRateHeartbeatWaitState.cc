#include "RotateAndRateHeartbeatWaitState.h"
#include "RotateMavlinkCommandState.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "FunctionState.h"

#include <QFinalState>

RotateAndRateHeartbeatWaitState::RotateAndRateHeartbeatWaitState(QState* parentState, double heading)
    : CustomState   ("RotateAndRateHeartbeatWaitState", parentState)
    , _customPlugin (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
{
    _heartbeatTimeoutTimer.setSingleShot(true);
    _heartbeatTimeoutTimer.setInterval(_customPlugin->maxWaitMSecsForKGroup());
    connect(&_heartbeatTimeoutTimer, &QTimer::timeout, this, [this] () {
        _disconnectAll();
        setError("Timeout waiting for heartbeats from rate detectors");
    }); 

    // States
    auto rotateCommandState = new RotateMavlinkCommandState(this, heading);
    auto heartbeatWaitState = new FunctionState("HeartbeatWaitState", this, [this] () { _heartbeatWaitStateEntered(); });
    auto finalState         = new QFinalState(this);

    // Transitions
    rotateCommandState->addTransition(rotateCommandState, &RotateMavlinkCommandState::success, heartbeatWaitState);
    heartbeatWaitState->addTransition(this, &RotateAndRateHeartbeatWaitState::heartbeatsReceived, finalState);

    setInitialState(rotateCommandState);
}

void RotateAndRateHeartbeatWaitState::_heartbeatWaitStateEntered()
{
    qCDebug(CustomPluginLog) << "Waiting for heartbeat from both rate detectors" << " - " << Q_FUNC_INFO;
    _heartbeatTimeoutTimer.start();
    connect(_customPlugin, &CustomPlugin::detectorHeartbeatReceived, this, &RotateAndRateHeartbeatWaitState::_detectorHeartbeatReceived);
}

void RotateAndRateHeartbeatWaitState::_detectorHeartbeatReceived(int oneBasedRateIndex)
{
    if (oneBasedRateIndex == 1) {
        qCDebug(CustomPluginLog) << "Rate detector 1 heartbeat received";
        _rateDetector1Heartbeat = true;
    } else if (oneBasedRateIndex == 2) {
        qCDebug(CustomPluginLog) << "Rate detector 2 heartbeat received";
        _rateDetector2Heartbeat = true;
    }

    if (_rateDetector1Heartbeat && _rateDetector2Heartbeat) {
        qCDebug(CustomPluginLog) << "Heartbeats from both rate detectors received";
        _disconnectAll();
        emit heartbeatsReceived();
    }
}

void RotateAndRateHeartbeatWaitState::_disconnectAll()
{
    _heartbeatTimeoutTimer.stop();
    disconnect(_customPlugin, &CustomPlugin::detectorHeartbeatReceived, this, &RotateAndRateHeartbeatWaitState::_detectorHeartbeatReceived);
}