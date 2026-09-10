#include "PythonCaptureAtSliceState.h"
#include "SendMavlinkCommandState.h"
#include "SendTunnelCommandState.h"
#include "FactWaitForValueTarget.h"
#include "PythonWaitForDetectionResultState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"

#include <QFinalState>
#include <QtMath>

namespace {
static constexpr double kPythonResultTimeoutFudgeFactor = 0.5;
}

using namespace TunnelProtocol;

PythonCaptureAtSliceState::PythonCaptureAtSliceState(
    QState* parentState, int headingIndex, int sequenceIndex, uint32_t collectionId)
    : CustomState       ("PythonCaptureAtSliceState", parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    double degreesPerSlice = 360.0 / _rotationDivisions;

    _sliceHeadingDegrees = headingIndex * degreesPerSlice;
    if (_sliceHeadingDegrees > 360.0) {
        _sliceHeadingDegrees -= 360.0;
    } else if (_sliceHeadingDegrees < 0.0) {
        _sliceHeadingDegrees += 360.0;
    }

    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    _sliceHeadingDegrees += -antennaOffset;
    _sliceHeadingDegrees = fmod(_sliceHeadingDegrees, 360.0);
    if (_sliceHeadingDegrees < 0.0) {
        _sliceHeadingDegrees += 360.0;
    }

    _buildStates(collectionId, static_cast<uint32_t>(sequenceIndex + 1));
}

PythonCaptureAtSliceState::PythonCaptureAtSliceState(
    QState* parentState, double yawDeg, uint32_t sliceId, uint32_t collectionId, ExplicitYaw)
    : CustomState       ("PythonCaptureAtSliceState", parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    _sliceHeadingDegrees = fmod(yawDeg, 360.0);
    if (_sliceHeadingDegrees < 0.0) {
        _sliceHeadingDegrees += 360.0;
    }
    _buildStates(collectionId, sliceId);
}

void PythonCaptureAtSliceState::_buildStates(uint32_t collectionId, uint32_t sliceId)
{
    connect(this, &QState::entered, this, [this] () {
        qCDebug(CustomStateMachineLog) << QStringLiteral("Python: rotating to heading %1").arg(_sliceHeadingDegrees) << " - " << Q_FUNC_INFO;
    });

    StartCollectionSlice_t startSlice {};
    startSlice.header.command = COMMAND_ID_START_COLLECTION_SLICE;
    startSlice.collection_id = collectionId;
    startSlice.slice_id = sliceId;
    startSlice.heading_deg = static_cast<float>(_sliceHeadingDegrees);

    // States
    auto announceRotateState        = new SayState("Announce Rotate", this, QStringLiteral("Searching at %1 degrees").arg(_sliceHeadingDegrees));
    auto rotateCommandState         = _rotateMavlinkCommandState(this);
    auto waitForHeadingState        = new FactWaitForValueTarget(this, _vehicle->heading(), _sliceHeadingDegrees, 1.0, 20 * 1000);
    auto startSliceState            = new SendTunnelCommandState("Python StartCollectionSlice", this, reinterpret_cast<uint8_t*>(&startSlice), sizeof(startSlice));
    const int maxWaitMsecs = _customPlugin->maxWaitMSecsForKGroup();
    const int waitForDetectionTimeoutMsecs = maxWaitMsecs + static_cast<int>(maxWaitMsecs * kPythonResultTimeoutFudgeFactor);
    auto waitForDetectionResultState= new PythonWaitForDetectionResultState(
        this, collectionId, startSlice.slice_id, waitForDetectionTimeoutMsecs);
    auto finalState                 = new QFinalState(this);

    // Transitions
    announceRotateState->addTransition      (announceRotateState,           &SayState::advance,               rotateCommandState);
    rotateCommandState->addTransition       (rotateCommandState,            &SendMavlinkCommandState::success,          waitForHeadingState);
    waitForHeadingState->addTransition      (waitForHeadingState,           &FactWaitForValueTarget::success,           startSliceState);
    startSliceState->addTransition          (startSliceState,               &SendTunnelCommandState::commandSucceeded,  waitForDetectionResultState);
    waitForDetectionResultState->addTransition(waitForDetectionResultState, &QState::finished,                          finalState);

    setInitialState(announceRotateState);
}

SendMavlinkCommandState* PythonCaptureAtSliceState::_rotateMavlinkCommandState(QState* parentState)
{
    SendMavlinkCommandState* state = nullptr;

    if (_vehicle->px4Firmware()) {
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_DO_REPOSITION,
                        -1,
                        MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
                        0,
                        qDegreesToRadians(_sliceHeadingDegrees),
                        qQNaN(), qQNaN(), qQNaN());
    } else if (_vehicle->apmFirmware()) {
        state = new SendMavlinkCommandState(
                        parentState,
                        MAV_CMD_CONDITION_YAW,
                        _sliceHeadingDegrees,
                        0,
                        1,
                        0,
                        0, 0, 0);
    }

    return state;
}
