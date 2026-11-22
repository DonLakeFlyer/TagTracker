#pragma once

#include <QtCore/QString>
#include <QtCore/QTimer>

#include "CustomStateMachine.h"
#include "GridGenerator.h"

class CustomSettings;
class Vehicle;

/// Auto Grid (docs/AUTO_GRID_SPEC.md §6, §9): uploads a generated grid mission, makes its first item current,
/// switches to Mission, then only watches. It ends as complete when the vehicle returns from the grid's end, and as
/// cancelled on any other mode change. After the switch to Mission it never sends anything to the vehicle.
class AutoGridStateMachine : public CustomStateMachine
{
    Q_OBJECT

public:
    /// What a flight mode change means for a running grid
    enum class ModeChange
    {
        Continue,  ///< Mission: the grid is (still) flying
        Complete,  ///< Return from the grid's loiter or RTL item: normal finish
        EndHold,   ///< Hold at the grid's end ("Mission finished, loitering" before Return): wait for Return
        Cancel,    ///< Anything else: the pilot, GCS or a failsafe has taken over
    };

    /// Preconditions shared by the toolstrip action and the start: an empty string, or the reason the grid can't
    /// start. Doesn't check for other running machines.
    [[nodiscard]] static QString checkVehicle(Vehicle* vehicle);

    /// Grid parameters from the settings and the vehicle pose now: origin, heading normalised to [0, 360),
    /// relative altitude and MPC_XY_VEL_MAX (NaN when the vehicle doesn't have it).
    [[nodiscard]] static GridGenerator::Params latchParams(Vehicle* vehicle, CustomSettings* settings);

    /// Pure classification of a mode change. missionStarted: the machine has switched the vehicle to Mission.
    /// atMissionEnd: the mission's current item is the grid's end loiter or RTL item (or past it).
    [[nodiscard]] static ModeChange classifyFlightModeChange(const QString& flightMode, bool missionStarted,
                                                             bool atMissionEnd, const QString& missionFlightMode,
                                                             const QString& rtlFlightMode,
                                                             const QString& holdFlightMode);

    /// True when the vehicle's current mission item (wire seq, as MissionManager::currentIndex reports it) is the
    /// grid's end loiter or later.
    [[nodiscard]] static bool isAtMissionEnd(const GridGenerator::Result& result, int currentWireIndex);

    /// The single log line written for each grid.
    [[nodiscard]] static QString logLine(const GridGenerator::Params& params, const GridGenerator::Result& result);

    /// params and result must be valid (GridGenerator::generate succeeded).
    AutoGridStateMachine(const GridGenerator::Params& params, const GridGenerator::Result& result,
                         QObject* parent = nullptr);

    const GridGenerator::Params& params() const { return _params; }

    const GridGenerator::Result& result() const { return _result; }

    /// A Hold at the grid's end that lasts this long without Return ends the grid as cancelled.
    static constexpr int kEndHoldGraceMsecs = 15000;

signals:
    void gridComplete();

private:
    void _handleFlightModeChange(const QString& flightMode);

    GridGenerator::Params _params;
    GridGenerator::Result _result;
    bool _missionStarted = false;
    bool _completing = false;
    QTimer _endHoldTimer;
};
