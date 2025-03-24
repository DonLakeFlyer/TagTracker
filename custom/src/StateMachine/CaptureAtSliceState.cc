#include "CaptureAtSliceState.h"
#include "SendMavlinkCommandState.h"
#include "DelayState.h"
#include "CustomPlugin.h"
#include "FactWaitForValueTarget.h"
#include "CustomLoggingCategory.h"
#include "SetFlightModeState.h"
#include "DetectorList.h"
#include "PulseRoseMapItem.h"
#include "SayState.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "CustomSettings.h"
#include "TagDatabase.h"
#include "FirmwarePlugin.h"

CaptureAtSliceState::CaptureAtSliceState(QState* parentState, int sliceIndex)
    : CustomState       ("CaptureAtSliceState", parentState)
    , _sliceIndex       (sliceIndex)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
    , _detectorList     (DetectorList::instance())
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    double degreesPerSlice = 360.0 / _rotationDivisions;

    // Convert slice index to heading
    _sliceHeadingDegrees = _sliceIndex * degreesPerSlice;
    if (_sliceHeadingDegrees > 360.0) {
        _sliceHeadingDegrees -= 360.0;
    } else if (_sliceHeadingDegrees < 0.0) {
        _sliceHeadingDegrees += 360.0;
    }

    // Adjust heading for antenna offset
    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    _sliceHeadingDegrees += -antennaOffset;

    // States
    auto rotateState = _rotateAndCaptureAtHeadingState();
    auto finalState = new QFinalState(this);

    // Transitions
    rotateState->addTransition(rotateState, &QState::finished, finalState);

    this->setInitialState(rotateState);
}

SendMavlinkCommandState* CaptureAtSliceState::_rotateMavlinkCommandState(QState* parentState)
{
    SendMavlinkCommandState* state = nullptr;

    if (_vehicle->px4Firmware()) {
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_DO_REPOSITION,
                        -1,                                     // no change in ground speed
                        MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,    // switch to guided mode
                        0,                                      // reserved
                        qDegreesToRadians(_sliceHeadingDegrees),      // change heading
                        qQNaN(), qQNaN(), qQNaN());             // no change lat, lon, alt
    } else if (_vehicle->apmFirmware()){
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_CONDITION_YAW,
                        _sliceHeadingDegrees,
                        0,                                      // Use default angular speed
                        1,                                      // Rotate clockwise
                        0,                                      // heading specified as absolute angle
                        0, 0, 0);                               // Unused
    }

    return state;
}

void CaptureAtSliceState::_sliceBegin(void)
{
    auto& rgAngleStrengths = _customPlugin->rgAngleStrengths().last();
    auto& rgAngleRatios = _customPlugin->rgAngleRatios().last();

    if (qIsNaN(rgAngleStrengths[_sliceIndex])) {
        rgAngleStrengths[_sliceIndex] = 0;
        rgAngleRatios[_sliceIndex] = 0;
        _customPlugin->signalAngleRatiosChanged();
    }
}

void CaptureAtSliceState::_sliceEnd()
{

}

CustomState* CaptureAtSliceState::_rotateAndCaptureAtHeadingState()
{
    // We wait at each rotation for enough time to go by to capture a user specified set of k groups
    uint32_t maxK                   = _customSettings->k()->rawValue().toUInt();
    auto kGroups                    = _customSettings->rotationKWaitCount()->rawValue().toInt();
    auto maxIntraPulseMsecs         = TagDatabase::instance()->maxIntraPulseMsecs();
    int rotationCaptureWaitMsecs    = maxIntraPulseMsecs * ((kGroups * maxK) + 1);
    
    auto groupingState = new CustomState("RotateAndCaptureAtHeadingState", this);
    connect(groupingState, &QState::entered, this, [this] () {
            qCDebug(CustomPluginLog) << QStringLiteral("Rotating to heading %1").arg(this->_sliceHeadingDegrees) << " - " << Q_FUNC_INFO;
    }); 

    auto announceRotateState        = new SayState("Announce Rotate", groupingState, QStringLiteral("Detection at %1 degrees").arg(_sliceHeadingDegrees));
    auto rotateCommandState         = _rotateMavlinkCommandState(groupingState);
    //auto rotateCommandState         = new FunctionState("ErrorTesting", groupingState, [sliceBeginState] () { sliceBeginState->machine()->setError("Error Testing"); });
    auto waitForHeadingChangeState  = new FactWaitForValueTarget(groupingState, _vehicle->heading(), _sliceHeadingDegrees, 1.0 /* _targetVariance */, 10 * 1000 /* _waitMsecs */);
    auto sliceBeginState            = new FunctionState("Slice Begin", groupingState, std::bind(&CaptureAtSliceState::_sliceBegin, this));
    auto delayForKGroupsState       = new DelayState(groupingState, rotationCaptureWaitMsecs);
    auto sliceEndState              = new FunctionState("Slice End", groupingState, [this] () { _sliceEnd(); });
    auto finalState                 = new QFinalState(groupingState);

    // Transitions
    announceRotateState->addTransition(announceRotateState, &SayState::functionCompleted, rotateCommandState);
    rotateCommandState->addTransition(rotateCommandState, &SendMavlinkCommandState::success, waitForHeadingChangeState);
    //rotateCommandState->addTransition(rotateCommandState, &FunctionState::functionCompleted, waitForHeadingChangeState);
    waitForHeadingChangeState->addTransition(waitForHeadingChangeState, &FactWaitForValueTarget::success, sliceBeginState);
    sliceBeginState->addTransition(sliceBeginState, &FunctionState::functionCompleted, delayForKGroupsState);
    delayForKGroupsState->addTransition(delayForKGroupsState, &DelayState::delayComplete, sliceEndState);
    sliceEndState->addTransition(sliceEndState, &FunctionState::functionCompleted, finalState);

    groupingState->setInitialState(announceRotateState);

    return groupingState;
}
