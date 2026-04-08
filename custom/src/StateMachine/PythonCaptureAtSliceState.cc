#include "PythonCaptureAtSliceState.h"
#include "SendMavlinkCommandState.h"
#include "SendTunnelCommandState.h"
#include "FactWaitForValueTarget.h"
#include "PythonWaitForDetectionResultState.h"
#include "SayState.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "CustomLoggingCategory.h"
#include "TagDatabase.h"
#include "TunnelProtocol.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"

#include <QFinalState>
#include <QtMath>

namespace {
static constexpr int kPythonResultTimeoutFudgeMsecs = 1000;
}

using namespace TunnelProtocol;

PythonCaptureAtSliceState::PythonCaptureAtSliceState(QState* parentState, int sliceIndex)
    : CustomState       ("PythonCaptureAtSliceState", parentState)
    , _vehicle          (MultiVehicleManager::instance()->activeVehicle())
    , _customPlugin     (qobject_cast<CustomPlugin*>(CustomPlugin::instance()))
    , _customSettings   (_customPlugin->customSettings())
    , _rotationDivisions(_customSettings->divisions()->rawValue().toInt())
{
    double degreesPerSlice = 360.0 / _rotationDivisions;

    _sliceHeadingDegrees = sliceIndex * degreesPerSlice;
    if (_sliceHeadingDegrees > 360.0) {
        _sliceHeadingDegrees -= 360.0;
    } else if (_sliceHeadingDegrees < 0.0) {
        _sliceHeadingDegrees += 360.0;
    }

    double antennaOffset = _customSettings->antennaOffset()->rawValue().toDouble();
    _sliceHeadingDegrees += -antennaOffset;

    connect(this, &QState::entered, this, [this] () {
        qCDebug(CustomStateMachineLog) << QStringLiteral("Python: rotating to heading %1").arg(_sliceHeadingDegrees) << " - " << Q_FUNC_INFO;
    });

    // Build StartDetectionInfo for this slice
    StartDetectionInfo_t startDetectionInfo;
    memset(&startDetectionInfo, 0, sizeof(startDetectionInfo));
    startDetectionInfo.header.command               = COMMAND_ID_START_DETECTION;
    startDetectionInfo.radio_center_frequency_hz    = TagDatabase::instance()->channelizerTuner();
    startDetectionInfo.gain                         = _customSettings->gain()->rawValue().toUInt();
    startDetectionInfo.detection_mode               = DETECTION_MODE_PYTHON;
    startDetectionInfo.detection_margin             = _customSettings->detectionMargin()->rawValue().toDouble();
    startDetectionInfo.confidence_ratio             = _customSettings->confidenceRatio()->rawValue().toDouble();

    StopDetectionInfo_t stopDetectionInfo;
    stopDetectionInfo.header.command = COMMAND_ID_STOP_DETECTION;

    // States
    auto announceRotateState        = new SayState("Announce Rotate", this, QStringLiteral("Searching at %1 degrees").arg(_sliceHeadingDegrees));
    auto rotateCommandState         = _rotateMavlinkCommandState(this);
    auto waitForHeadingState        = new FactWaitForValueTarget(this, _vehicle->heading(), _sliceHeadingDegrees, 1.0, 10 * 1000);
    auto startDetectionState        = new SendTunnelCommandState("Python StartDetection", this, (uint8_t*)&startDetectionInfo, sizeof(startDetectionInfo));
    const int waitForDetectionTimeoutMsecs = _customPlugin->maxWaitMSecsForKGroup() + kPythonResultTimeoutFudgeMsecs;
    auto waitForDetectionResultState= new PythonWaitForDetectionResultState(this, waitForDetectionTimeoutMsecs);
    auto stopDetectionState         = new SendTunnelCommandState("Python StopDetection", this, (uint8_t*)&stopDetectionInfo, sizeof(stopDetectionInfo));
    auto finalState                 = new QFinalState(this);

    // Transitions
    announceRotateState->addTransition      (announceRotateState,           &SayState::functionCompleted,               rotateCommandState);
    rotateCommandState->addTransition       (rotateCommandState,            &SendMavlinkCommandState::success,          waitForHeadingState);
    waitForHeadingState->addTransition      (waitForHeadingState,           &FactWaitForValueTarget::success,           startDetectionState);
    startDetectionState->addTransition      (startDetectionState,           &SendTunnelCommandState::commandSucceeded,  waitForDetectionResultState);
    waitForDetectionResultState->addTransition(waitForDetectionResultState, &QState::finished,                          stopDetectionState);
    stopDetectionState->addTransition       (stopDetectionState,            &SendTunnelCommandState::commandSucceeded,  finalState);

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
