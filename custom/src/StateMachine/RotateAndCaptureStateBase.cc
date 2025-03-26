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
#include "RotateAndRateHeartbeatWaitState.h"

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
    // States
    auto initialRotateCommandState      = new RotateAndRateHeartbeatWaitState(this, 0);
    _rotationBeginState                 = new FunctionState("Rotation Begin", this, std::bind(&RotateAndCaptureStateBase::_rotationBegin, this));
    _rotationEndState                   = new FunctionState("Rotation End", this, std::bind(&RotateAndCaptureStateBase::_rotationEnd, this));
    auto announceRotateCompleteState    = new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                     = new QFinalState(this);

    // Transitions
    initialRotateCommandState->addTransition(initialRotateCommandState, &QState::finished, _rotationBeginState);
    _rotationEndState->addTransition(_rotationEndState, &FunctionState::functionCompleted, announceRotateCompleteState);
    announceRotateCompleteState->addTransition(announceRotateCompleteState, &SayState::functionCompleted, finalState);

    this->setInitialState(initialRotateCommandState);
}

void RotateAndCaptureStateBase::_rotationBegin()
{
    _customPlugin->rotationIsStarting();
    // We always start our rotation pulse captures with the antenna at 0 degrees heading
    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    _degreesPerSlice = 360.0 / _rotationDivisions;
    _firstHeading = -antennaOffset;
}

void RotateAndCaptureStateBase::_rotationEnd()
{
    _customPlugin->rotationIsEnding();
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
