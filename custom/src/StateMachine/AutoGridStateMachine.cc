#include "AutoGridStateMachine.h"

#include <cmath>
#include <limits>

#include <QtStateMachine/QFinalState>

#include "AudioOutput.h"
#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "Fact.h"
#include "FunctionState.h"
#include "MissionManager.h"
#include "ParameterManager.h"
#include "SayState.h"
#include "SetFlightModeState.h"
#include "UploadMissionState.h"
#include "Vehicle.h"

QString AutoGridStateMachine::checkVehicle(Vehicle* vehicle)
{
    if (!vehicle) {
        return tr("No vehicle connected");
    }
    if (!vehicle->airborne()) {
        return tr("Vehicle must be flying to start a grid");
    }
    if (!vehicle->coordinate().isValid()) {
        return tr("Vehicle has no valid position for a grid");
    }
    return QString();
}

GridGenerator::Params AutoGridStateMachine::latchParams(Vehicle* vehicle, CustomSettings* settings)
{
    GridGenerator::Params params;
    if (!vehicle || !settings) {
        return params;
    }

    params.origin = vehicle->coordinate();
    params.headingDeg = CustomPlugin::normalizeHeading(vehicle->heading()->rawValue().toDouble());
    params.altitudeM = vehicle->altitudeRelative()->rawValue().toDouble();

    params.widthM = settings->gridWidth()->rawValue().toDouble();
    params.depthM = settings->gridDepth()->rawValue().toDouble();
    params.spacingM = settings->gridLaneSpacing()->rawValue().toDouble();
    params.cruiseSpeedMS = settings->gridCruiseSpeed()->rawValue().toDouble();
    params.turnSpeedMS = settings->gridTurnSpeed()->rawValue().toDouble();
    params.startSide = (settings->gridStartSide()->rawValue().toUInt() == CustomSettings::GridStartRight)
                           ? GridGenerator::StartSide::Right
                           : GridGenerator::StartSide::Left;
    params.arcStepDeg = settings->gridArcStep()->rawValue().toDouble();
    params.endLoiterS = settings->gridEndLoiter()->rawValue().toDouble();
    params.acceptanceRadiusM = settings->gridAcceptanceRadius()->rawValue().toDouble();

    params.maxSpeedMS = std::numeric_limits<double>::quiet_NaN();
    ParameterManager* const parameterManager = vehicle->parameterManager();
    static const QString maxSpeedParam = QStringLiteral("MPC_XY_VEL_MAX");
    if (parameterManager && parameterManager->parameterExists(ParameterManager::defaultComponentId, maxSpeedParam)) {
        if (Fact* const maxSpeed =
                parameterManager->getParameter(ParameterManager::defaultComponentId, maxSpeedParam)) {
            params.maxSpeedMS = maxSpeed->rawValue().toDouble();
        }
    }
    return params;
}

AutoGridStateMachine::ModeChange AutoGridStateMachine::classifyFlightModeChange(const QString& flightMode,
                                                                                bool missionStarted, bool atMissionEnd,
                                                                                const QString& missionFlightMode,
                                                                                const QString& rtlFlightMode,
                                                                                const QString& holdFlightMode)
{
    if (flightMode == missionFlightMode) {
        return ModeChange::Continue;
    }
    if (missionStarted && atMissionEnd) {
        if (flightMode == rtlFlightMode) {
            return ModeChange::Complete;
        }
        if (flightMode == holdFlightMode) {
            return ModeChange::EndHold;
        }
    }
    return ModeChange::Cancel;
}

bool AutoGridStateMachine::isAtMissionEnd(const GridGenerator::Result& result, int currentWireIndex)
{
    for (const GridGenerator::Item& item : result.items) {
        if ((item.command == MAV_CMD_NAV_LOITER_TIME) || (item.command == MAV_CMD_NAV_RETURN_TO_LAUNCH)) {
            return currentWireIndex >= item.seq;
        }
    }
    return false;
}

QString AutoGridStateMachine::logLine(const GridGenerator::Params& params, const GridGenerator::Result& result)
{
    return QStringLiteral(
               "Auto Grid: origin %1, %2 heading %3 alt %4 width %5 depth %6 spacing %7 cruise %8 "
               "turn %9 side %10 arcStep %11 loiter %12 acceptance %13 maxSpeed %14 items %15 summary \"%16\"")
        .arg(params.origin.latitude(), 0, 'f', 7)
        .arg(params.origin.longitude(), 0, 'f', 7)
        .arg(params.headingDeg, 0, 'f', 1)
        .arg(params.altitudeM, 0, 'f', 1)
        .arg(params.widthM)
        .arg(params.depthM)
        .arg(params.spacingM)
        .arg(params.cruiseSpeedMS)
        .arg(params.turnSpeedMS)
        .arg((params.startSide == GridGenerator::StartSide::Left) ? QStringLiteral("Left") : QStringLiteral("Right"))
        .arg(params.arcStepDeg)
        .arg(params.endLoiterS)
        .arg(params.acceptanceRadiusM)
        .arg(params.maxSpeedMS)
        .arg(result.items.count())
        .arg(result.summary);
}

AutoGridStateMachine::AutoGridStateMachine(const GridGenerator::Params& params, const GridGenerator::Result& result,
                                           QObject* parent)
    : CustomStateMachine(QStringLiteral("Grid"), parent)
    , _params(params)
    , _result(result)
{
    // Any mode change from confirm on is classified by the grid's own rules, not the Hold/allowed-mode rule
    setEventMode(CancelOnFlightModeChange);
    setFlightModeChangeHandler([this](const QString& flightMode) { _handleFlightModeChange(flightMode); });

    _endHoldTimer.setSingleShot(true);
    _endHoldTimer.setInterval(kEndHoldGraceMsecs);
    connect(&_endHoldTimer, &QTimer::timeout, this, [this]() {
        qCDebug(CustomPluginLog) << "Grid end Hold without Return";
        cancel();
    });

    Vehicle* const gridVehicle = vehicle();
    // A disarm leaves the mode as it was, so it would otherwise never end the grid
    connect(gridVehicle, &Vehicle::armedChanged, this, [this](bool armed) {
        if (!armed && !_completing) {
            cancel();
        }
    });

    auto uploadState = new UploadMissionState(this, gridVehicle, GridGenerator::planManagerItems(_params, _result));
    // 1 is wire item 0 on PX4: Vehicle subtracts the home item
    auto setCurrentState = new FunctionState(QStringLiteral("SetCurrentMissionItem"), this,
                                             [gridVehicle]() { gridVehicle->setCurrentMissionSequence(1); });
    auto missionModeState = new SetFlightModeState(this, gridVehicle->missionFlightMode());
    auto missionStartedState =
        new SayState(QStringLiteral("AnnounceGridStarted"), this, QStringLiteral("Grid started"));
    // From here the machine only watches. Return counts as complete only once gridComplete has a state to leave.
    auto monitorState = new QState(this);
    monitorState->setObjectName(QStringLiteral("MonitorGrid"));
    connect(monitorState, &QState::entered, this, [this]() { _missionStarted = true; });
    auto announceCompleteState =
        new SayState(QStringLiteral("AnnounceGridComplete"), this, QStringLiteral("Grid complete, returning"));
    auto finalState = new QFinalState(this);

    uploadState->addTransition(uploadState, &UploadMissionState::uploadComplete, setCurrentState);
    setCurrentState->addTransition(setCurrentState, &FunctionState::advance, missionModeState);
    missionModeState->addTransition(missionModeState, &SetFlightModeState::flightModeChanged, missionStartedState);
    missionStartedState->addTransition(missionStartedState, &SayState::advance, monitorState);
    monitorState->addTransition(this, &AutoGridStateMachine::gridComplete, announceCompleteState);
    announceCompleteState->addTransition(announceCompleteState, &SayState::advance, finalState);

    setInitialState(uploadState);
}

void AutoGridStateMachine::_handleFlightModeChange(const QString& flightMode)
{
    if (_completing) {
        return;
    }
    _endHoldTimer.stop();

    Vehicle* const gridVehicle = vehicle();
    const int currentIndex = gridVehicle->missionManager()->currentIndex();
    const ModeChange change = classifyFlightModeChange(
        flightMode, _missionStarted, isAtMissionEnd(_result, currentIndex), gridVehicle->missionFlightMode(),
        gridVehicle->rtlFlightMode(), gridVehicle->pauseFlightMode());
    qCDebug(CustomPluginLog) << "Grid flight mode change:" << flightMode << "missionStarted:" << _missionStarted
                             << "currentIndex:" << currentIndex << "change:" << static_cast<int>(change);

    switch (change) {
        case ModeChange::Continue:
            break;
        case ModeChange::EndHold:
            _endHoldTimer.start();
            break;
        case ModeChange::Complete:
            _completing = true;
            emit gridComplete();
            break;
        case ModeChange::Cancel:
            cancel();
            break;
    }
}
