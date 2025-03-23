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
    // States
    _rotationBeginState                 = new FunctionState("Rotation Begin", this, std::bind(&RotateAndCaptureStateBase::_rotationBegin, this));
    _rotationEndState                   = new FunctionState("Rotation End", this, std::bind(&RotateAndCaptureStateBase::_rotationEnd, this));
    auto announceRotateCompleteState    = new SayState("Announce Rotate Complete", this, "Rotation detection complete");
    auto finalState                     = new QFinalState(this);

    // Transitions
    _rotationEndState->addTransition(_rotationEndState, &FunctionState::functionCompleted, announceRotateCompleteState);
    announceRotateCompleteState->addTransition(announceRotateCompleteState, &SayState::functionCompleted, finalState);

    this->setInitialState(_rotationBeginState);
}

void RotateAndCaptureStateBase::_rotationBegin()
{
    // Setup new rotation data  
    _customPlugin->setActiveRotation(true);
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

    CSVLogManager& logManager = _customPlugin->csvLogManager();

    logManager.csvStartRotationPulseLog();
    logManager.csvLogRotationStart();

    _customPlugin->signalAngleRatiosChanged();
    _customPlugin->signalCalcedBearingsChanged();

    // Create compass rose ui on map
    QUrl url = QUrl::fromUserInput("qrc:/qml/CustomPulseRoseMapItem.qml");
    PulseRoseMapItem* mapItem = new PulseRoseMapItem(url, _customPlugin->rgAngleStrengths().count() - 1, _vehicle->coordinate(), this);
    _customPlugin->customMapItems()->append(mapItem);
}

void RotateAndCaptureStateBase::_rotationEnd()
{
    _customPlugin->setActiveRotation(false);
    CSVLogManager& logManager = _customPlugin->csvLogManager();
    logManager.csvLogRotationStop();
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
