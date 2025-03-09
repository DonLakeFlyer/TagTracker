#include "RotateAndCaptureState.h"
#include "SendMavlinkCommandState.h"
#include "DelayState.h"
#include "CustomPlugin.h"
#include "FactWaitForValueTarget.h"
#include "CustomLoggingCategory.h"
#include "SetFlightModeState.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "CustomSettings.h"
#include "TagDatabase.h"
#include "FirmwarePlugin.h"

RotateAndCaptureState::RotateAndCaptureState(QState* parentState)
    : CustomState       ("RotateAndCaptureState", parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
{
    int rotationDivisions = _customSettings->divisions()->rawValue().toInt();

    // Setup new rotation data  
    _customPlugin->rgAngleStrengths().append(QList<double>());
    _customPlugin->rgAngleRatios().append(QList<double>());
    _customPlugin->rgCalcedBearings().append(qQNaN());

    QList<double>& angleStrengths = _customPlugin->rgAngleStrengths().last();
    QList<double>& angleRatios = _customPlugin->rgAngleRatios().last();

    // Prime angle strengths with no values
    for (int i=0; i<rotationDivisions; i++) {
        angleStrengths.append(qQNaN());
        angleRatios.append(qQNaN());
    }

    // We always start our rotation pulse captures with the antenna at 0 degrees heading
    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    double sliceDegrees = 360.0 / rotationDivisions;
    double nextHeading = -antennaOffset;

    // Add rotation state machine entries
    QList<CustomState*> rotationStates;
    for (int i=0; i<rotationDivisions; i++) {
        if (nextHeading >= 360) {
            nextHeading -= 360;
        } else if (nextHeading < 0) {
            nextHeading += 360;
        }

        auto state = _rotateAndCaptureAtHeadingState(this, nextHeading);
        rotationStates.append(state);

        nextHeading += sliceDegrees;
    }

    auto errorState = new QState(this);
    auto finalState = new QFinalState(this);

    // Setup rotation transitions
    for (int i=0; i<rotationStates.count(); i++) {
        if (i == rotationStates.count() - 1) {
            rotationStates[i]->addTransition(rotationStates[i], &QState::finished, finalState);
        } else {
            rotationStates[i]->addTransition(rotationStates[i], &QState::finished, rotationStates[i+1]);
        }
    }

    // Error handling
    this->addTransition(this, &CustomState::error, errorState);

    this->setInitialState(rotationStates.first());
}

CustomState* RotateAndCaptureState::_rotateAndCaptureAtHeadingState(QState* parentState, double headingDegrees)
{
    // We wait at each rotation for enough time to go by to capture a user specified set of k groups
    uint32_t maxK = _customSettings->k()->rawValue().toUInt();
    auto kGroups = _customSettings->rotationKWaitCount()->rawValue().toInt();
    auto maxIntraPulseMsecs = TagDatabase::instance()->maxIntraPulseMsecs();
    int rotationCaptureWaitMsecs = maxIntraPulseMsecs * ((kGroups * maxK) + 1);
    
    auto groupingState = new CustomState("RotateAndCaptureAtHeadingState", parentState);
    connect(groupingState, &QState::entered, this, [this, headingDegrees] () {
            qCDebug(CustomPluginLog) << QStringLiteral("Rotating to heading %1").arg(headingDegrees) << " - " << Q_FUNC_INFO;
    }); 

    auto rotateCommandState = _rotateMavlinkCommandState(groupingState, headingDegrees);
    auto waitForHeadingChangeState = new FactWaitForValueTarget(groupingState, _vehicle->heading(), headingDegrees, 1.0 /* _targetVariance */, 10 * 1000 /* _waitMsecs */);
    auto delayForKGroupsState = new DelayState(groupingState, rotationCaptureWaitMsecs);

    auto errorState = new QState(groupingState);
    auto finalState = new QFinalState(groupingState);

    // Transitions
    rotateCommandState->addTransition(rotateCommandState, &SendMavlinkCommandState::success, waitForHeadingChangeState);
    waitForHeadingChangeState->addTransition(waitForHeadingChangeState, &FactWaitForValueTarget::success, delayForKGroupsState);
    delayForKGroupsState->addTransition(delayForKGroupsState, &DelayState::delayComplete, finalState);

    // Error handling
    rotateCommandState->addTransition(rotateCommandState, &SendMavlinkCommandState::error, errorState);
    groupingState->addTransition(groupingState, &CustomState::error, errorState);

    groupingState->setInitialState(rotateCommandState);

    return groupingState;
}

SendMavlinkCommandState* RotateAndCaptureState::_rotateMavlinkCommandState(QState* parentState, double headingDegrees)
{
    SendMavlinkCommandState* state = nullptr;

    if (_vehicle->px4Firmware()) {
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_DO_REPOSITION,
                        -1,                                     // no change in ground speed
                        MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,    // switch to guided mode
                        0,                                      // reserved
                        qDegreesToRadians(headingDegrees),      // change heading
                        qQNaN(), qQNaN(), qQNaN());             // no change lat, lon, alt
    } else if (_vehicle->apmFirmware()){
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_CONDITION_YAW,
                        headingDegrees,
                        0,                                      // Use default angular speed
                        1,                                      // Rotate clockwise
                        0,                                      // heading specified as absolute angle
                        0, 0, 0);                               // Unused
    }

    return state;
}
