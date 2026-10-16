#include "YawSweepState.h"

#include <cmath>

#include <QtCore/QtMath>

#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "CustomStateMachine.h"
#include "QGCMAVLink.h"
#include "QGCStateMachine.h"
#include "Vehicle.h"

using namespace TunnelProtocol;

CustomPlugin* YawSweepState::_customPlugin()
{
    return qobject_cast<CustomPlugin*>(CustomPlugin::instance());
}

YawSweepState::YawSweepState(QState* parentState, bool clockwise, double rateDegPerSec, uint32_t progressOffsetDeg)
    : CustomState("YawSweepState", parentState)
    , _clockwise(clockwise)
    , _rateDegPerSec(rateDegPerSec)
    , _progressOffsetDeg(progressOffsetDeg)
{
    _tickTimer.setInterval(_tickMSecs);
    connect(&_tickTimer, &QTimer::timeout, this, &YawSweepState::_tick);
    connect(this, &QState::entered, this, &YawSweepState::_start);
    connect(this, &QState::exited, this, &YawSweepState::_stop);
}

void YawSweepState::_start()
{
    _vehicle = customMachine()->vehicle();
    if (!_vehicle) {
        setError(QStringLiteral("No vehicle for calibration sweep"));
        return;
    }
    if (!_vehicle->px4Firmware() && !_vehicle->apmFirmware()) {
        setError(QStringLiteral("Calibration sweep requires PX4 or ArduPilot"));
        return;
    }

    connect(_vehicle, &Vehicle::mavCommandResult, this, &YawSweepState::_mavCommandResult, Qt::UniqueConnection);
    // A cancelled machine stops without exiting this state, so the setpoint stream must stop here too
    connect(machine(), &QStateMachine::stopped, this, &YawSweepState::_stop, Qt::UniqueConnection);

    _startHeadingDeg = _vehicle->heading()->rawValue().toDouble();
    _lastHeadingDeg = _startHeadingDeg;
    _turnedDeg = 0;
    _lastProgressDeg = 0;
    _lastProgressMSecs = 0;
    qCDebug(CustomPluginLog) << "Calibration sweep clockwise:" << _clockwise << "rateDegPerSec:" << _rateDegPerSec
                             << "startHeadingDeg:" << _startHeadingDeg;

    _customPlugin()->reportCalibrationSweepProgress(OPERATION_STATE_RUNNING, _progressOffsetDeg, _progressMessage());
    _elapsed.start();
    _tickTimer.start();

    if (_vehicle->px4Firmware()) {
        _orbitSeen = _vehicle->flightMode() == CustomPlugin::orbitFlightMode();
        _sendOrbit();
    }
}

QString YawSweepState::_progressMessage() const
{
    return _clockwise ? tr("Sweeping clockwise") : tr("Sweeping counter-clockwise");
}

void YawSweepState::_stop()
{
    _tickTimer.stop();
    if (_vehicle) {
        disconnect(_vehicle, &Vehicle::mavCommandResult, this, &YawSweepState::_mavCommandResult);
    }
}

void YawSweepState::_tick()
{
    const double headingDeg = _vehicle->heading()->rawValue().toDouble();
    _turnedDeg += std::remainder(headingDeg - _lastHeadingDeg, 360.0);
    _lastHeadingDeg = headingDeg;

    const qint64 elapsedMSecs = _elapsed.elapsed();

    if (_vehicle->px4Firmware()) {
        if (!_checkOrbitMode()) {
            return;
        }
    } else {
        const double sweptDeg = qMin(360.0, elapsedMSecs / 1000.0 * _rateDegPerSec);
        const double targetHeadingDeg =
            CustomPlugin::normalizeHeading(_startHeadingDeg + (_clockwise ? sweptDeg : -sweptDeg));
        // Skip a tick rather than queue a duplicate; the next setpoint supersedes it anyway
        if (!_vehicle->isMavCommandPending(MAV_COMP_ID_AUTOPILOT1, MAV_CMD_CONDITION_YAW)) {
            _sendHeading(targetHeadingDeg);
        }
    }

    const double turnedInSweepDirectionDeg = _clockwise ? _turnedDeg : -_turnedDeg;
    const uint32_t progressDeg = static_cast<uint32_t>(qRound(qBound(0.0, turnedInSweepDirectionDeg, 360.0)));
    // Degrees ride in the step (the card's bar); a changing message would be logged every tick
    _customPlugin()->reportCalibrationSweepProgress(OPERATION_STATE_RUNNING, _progressOffsetDeg + progressDeg,
                                                    _progressMessage());

    // Measured turn, so a vehicle that never turned cannot complete
    if (turnedInSweepDirectionDeg >= 360.0 - _settledToleranceDeg) {
        qCDebug(CustomPluginLog) << "Calibration sweep complete turnedDeg:" << _turnedDeg
                                 << "elapsedSecs:" << elapsedMSecs / 1000.0;
        _stop();
        emit sweepComplete();
        return;
    }

    if (turnedInSweepDirectionDeg >= _lastProgressDeg + _stallProgressDeg) {
        _lastProgressDeg = turnedInSweepDirectionDeg;
        _lastProgressMSecs = elapsedMSecs;
    } else if (elapsedMSecs - _lastProgressMSecs > _stallTimeoutMSecs) {
        qCWarning(CustomPluginLog) << "Calibration sweep stalled turnedDeg:" << _turnedDeg
                                   << "headingDeg:" << headingDeg;
        _stop();
        setError(QStringLiteral("Calibration sweep stopped turning"));
    }
}

void YawSweepState::_sendHeading(double headingDeg)
{
    _vehicle->sendMavCommand(MAV_COMP_ID_AUTOPILOT1, MAV_CMD_CONDITION_YAW, false /* showError */,
                             static_cast<float>(headingDeg), static_cast<float>(_rateDegPerSec), _clockwise ? 1 : -1,
                             0);
}

void YawSweepState::_sendOrbit()
{
    const QGeoCoordinate position = _vehicle->coordinate();
    if (!position.isValid()) {
        _stop();
        setError(QStringLiteral("No vehicle position for calibration sweep"));
        return;
    }

    // Centre ahead on the current heading: the vehicle starts on the circle already facing it
    const QGeoCoordinate center = position.atDistanceAndAzimuth(_orbitRadiusM, _startHeadingDeg);
    const double speedMetersPerSec = _orbitRadiusM * qDegreesToRadians(_rateDegPerSec);
    qCDebug(CustomPluginLog) << "Calibration sweep orbit radiusM:" << _orbitRadiusM
                             << "speedMetersPerSec:" << speedMetersPerSec << "center:" << center;

    customMachine()->setAllowedFlightMode(CustomPlugin::orbitFlightMode());
    // PX4 takes the direction from the radius sign
    _vehicle->sendMavCommandInt(
        MAV_COMP_ID_AUTOPILOT1, MAV_CMD_DO_ORBIT, MAV_FRAME_GLOBAL, false /* showError */,
        static_cast<float>(_clockwise ? _orbitRadiusM : -_orbitRadiusM), static_cast<float>(speedMetersPerSec),
        static_cast<float>(ORBIT_YAW_BEHAVIOUR_HOLD_FRONT_TO_CIRCLE_CENTER), static_cast<float>(qQNaN()),
        center.latitude(), center.longitude(), static_cast<float>(qQNaN()));
    emit orbitSent();
}

bool YawSweepState::_checkOrbitMode()
{
    if (_vehicle->flightMode() == CustomPlugin::orbitFlightMode()) {
        _orbitSeen = true;
        return true;
    }
    if (_orbitSeen) {
        // Hold is not a machine cancel, so a pilot switching to it lands here
        qCWarning(CustomPluginLog) << "Calibration sweep orbit ended by mode change to:" << _vehicle->flightMode();
        _stop();
        setError(QStringLiteral("Calibration sweep orbit ended by flight mode change"));
        return false;
    }
    if (_elapsed.elapsed() > _orbitEntryTimeoutMSecs) {
        _stop();
        setError(QStringLiteral("Vehicle did not enter Orbit for the calibration sweep"));
        return false;
    }
    return true;
}

void YawSweepState::_mavCommandResult(int vehicleId, int targetComponent, int command, int ackResult, int failureCode)
{
    Q_UNUSED(vehicleId);
    Q_UNUSED(targetComponent);

    if (command != MAV_CMD_DO_ORBIT && command != MAV_CMD_CONDITION_YAW) {
        return;
    }
    if (ackResult != MAV_RESULT_ACCEPTED) {
        qCWarning(CustomPluginLog) << "Calibration sweep heading command failed ackResult:" << ackResult
                                   << "failureCode:" << failureCode;
        _stop();
        setError(QStringLiteral("Vehicle rejected calibration sweep heading"));
    }
}
