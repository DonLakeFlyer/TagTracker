#include "RotateAndCaptureStateBase.h"
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

RotateAndCaptureStateBase::RotateAndCaptureStateBase(const QString& stateName, QState* parentState)
    : CustomState       (stateName, parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
    , _detectorList     (DetectorList::instance())
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    // Setup new rotation data  
    _customPlugin->rgAngleStrengths().append(QList<double>());
    _customPlugin->rgAngleRatios().append(QList<double>());
    _customPlugin->rgCalcedBearings().append(qQNaN());

    QList<double>& refAngleStrengths = _customPlugin->rgAngleStrengths().last();
    QList<double>& refAngleRatios = _customPlugin->rgAngleRatios().last();

    // Prime angle strengths with no values
    for (int i=0; i<_rotationDivisions; i++) {
        refAngleStrengths.append(qQNaN());
        refAngleRatios.append(qQNaN());
    }

    // We always start our rotation pulse captures with the antenna at 0 degrees heading
    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    _degreesPerSlice = 360.0 / _rotationDivisions;
    _firstHeading = -antennaOffset;

    _initRotationState              = new FunctionState("InitNewRotationDuringFlight", this, std::bind(&RotateAndCaptureStateBase::_initNewRotationDuringFlight, this));
    _announceRotateCompleteState    = new SayState("AnnounceRotateComplete", this, "Rotation detection complete");
    _finalState                     = new QFinalState(this);

    // Add rotation state machine entries
    for (int i=0; i<_rotationDivisions; i++) {
        auto state = _rotateAndCaptureAtHeadingState(this, i);
        _rotationStates.append(state);
    }

    _announceRotateCompleteState->addTransition(_announceRotateCompleteState, &SayState::functionCompleted, _finalState);

    this->setInitialState(_initRotationState);
}

void RotateAndCaptureStateBase::_initNewRotationDuringFlight()
{
    CSVLogManager& logManager = _customPlugin->csvLogManager();

    logManager.csvStartRotationPulseLog();
    logManager.csvLogRotationStart();
    logManager.csvLogRotationStart();

    _customPlugin->signalAngleRatiosChanged();
    _customPlugin->signalCalcedBearingsChanged();

    // Create compass rose ui on map
    QUrl url = QUrl::fromUserInput("qrc:/qml/CustomPulseRoseMapItem.qml");
    PulseRoseMapItem* mapItem = new PulseRoseMapItem(url, _customPlugin->rgAngleStrengths().count() - 1, _vehicle->coordinate(), this);
    _customPlugin->customMapItems()->append(mapItem);
}

SendMavlinkCommandState* RotateAndCaptureStateBase::_rotateMavlinkCommandState(QState* parentState, double headingDegrees)
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

void RotateAndCaptureStateBase::_sliceBegin(void)
{
    _detectorList->resetMaxStrength();
    _detectorList->resetPulseGroupCount();
}

void RotateAndCaptureStateBase::_sliceEnd(int sliceIndex)
{
    double maxStrength = _detectorList->maxStrength();

    qCDebug(CustomPluginLog) << QStringLiteral("Slice %1 max snr %2").arg(sliceIndex).arg(maxStrength) << " - " << Q_FUNC_INFO;

    auto& rgAngleStrengths = _customPlugin->rgAngleStrengths();
    auto& rgAngleRatios = _customPlugin->rgAngleRatios();

    rgAngleStrengths.last()[sliceIndex] = maxStrength;

    // Adjust the angle ratios to this new information
    maxStrength = 0;
    for (int i=0; i<_rotationDivisions; i++) {
        if (rgAngleStrengths.last()[i] > maxStrength) {
            maxStrength = rgAngleStrengths.last()[i];
        }
    }
    for (int i=0; i<_rotationDivisions; i++) {
        double angleStrength = rgAngleStrengths.last()[i];
        if (!qIsNaN(angleStrength)) {
            rgAngleRatios.last()[i] = rgAngleStrengths.last()[i] / maxStrength;
        }
    }

    _customPlugin->signalAngleRatiosChanged();
}


CustomState* RotateAndCaptureStateBase::_rotateAndCaptureAtHeadingState(QState* parentState, int sliceIndex)
{
    double headingDegrees = _sliceIndexToHeading(sliceIndex);

    // We wait at each rotation for enough time to go by to capture a user specified set of k groups
    uint32_t maxK = _customSettings->k()->rawValue().toUInt();
    auto kGroups = _customSettings->rotationKWaitCount()->rawValue().toInt();
    auto maxIntraPulseMsecs = TagDatabase::instance()->maxIntraPulseMsecs();
    int rotationCaptureWaitMsecs = maxIntraPulseMsecs * ((kGroups * maxK) + 1);
    
    auto groupingState = new CustomState("RotateAndCaptureAtHeadingState", parentState);
    connect(groupingState, &QState::entered, this, [this, headingDegrees] () {
            qCDebug(CustomPluginLog) << QStringLiteral("Rotating to heading %1").arg(headingDegrees) << " - " << Q_FUNC_INFO;
    }); 

    auto announceRotateState        = new SayState("AnnounceRotate", groupingState, QStringLiteral("Detection at %1 degrees").arg(headingDegrees));
    auto rotateCommandState         = _rotateMavlinkCommandState(groupingState, headingDegrees);
    //auto rotateCommandState         = new FunctionState("ErrorTesting", groupingState, [sliceBeginState] () { sliceBeginState->machine()->setError("Error Testing"); });
    auto waitForHeadingChangeState  = new FactWaitForValueTarget(groupingState, _vehicle->heading(), headingDegrees, 1.0 /* _targetVariance */, 10 * 1000 /* _waitMsecs */);
    auto sliceBeginState            = new FunctionState("SliceBegin", groupingState, std::bind(&RotateAndCaptureStateBase::_sliceBegin, this));
    auto delayForKGroupsState       = new DelayState(groupingState, rotationCaptureWaitMsecs);
    auto sliceEndState              = new FunctionState("SliceEnd", groupingState, [this, sliceIndex] () { _sliceEnd(sliceIndex); });
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

double RotateAndCaptureStateBase::_sliceIndexToHeading(int sliceIndex)
{
    double heading = _firstHeading + (sliceIndex * _degreesPerSlice);
    if (heading > 360.0) {
        heading -= 360.0;
    } else if (heading < 0.0) {
        heading += 360.0;
    }
    return heading;
}
