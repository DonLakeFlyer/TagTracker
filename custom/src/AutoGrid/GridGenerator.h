#pragma once

#include <cstdint>
#include <limits>

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtPositioning/QGeoCoordinate>

#include "QGCMAVLink.h"

class MissionItem;
class QObject;

/// Auto Grid search geometry and mission (docs/AUTO_GRID_SPEC.md §4, §5). Pure: no vehicle, settings or UI access.
/// Mirrors the T1 reference generator (uavrt_auto_grid tools/grid_proto.py) so its outputs can be compared directly.
namespace GridGenerator {

/// A lane closer than this to the previous one counts as a duplicate and is dropped.
constexpr double LANE_EPS_M = 1e-6;
/// Arc angles j * step are kept while j * step < 180 - ARC_EPS_DEG, so 180 itself is never included.
constexpr double ARC_EPS_DEG = 1e-9;
constexpr double MIN_ARC_STEP_DEG = 10.0;
constexpr double MAX_ARC_STEP_DEG = 90.0;

enum class StartSide
{
    Left,
    Right,
};

/// Defaults are spec §3.
struct Params
{
    QGeoCoordinate origin;     ///< Vehicle position at confirm
    double headingDeg = 0.0;   ///< ψ, degrees true, [0, 360)
    double altitudeM = 0.0;    ///< Relative altitude at confirm
    double widthM = 100.0;     ///< W, centred on the origin
    double depthM = 100.0;     ///< D, forward of the origin
    double spacingM = 20.0;    ///< s
    double cruiseSpeedMS = 6.0;
    double turnSpeedMS = 3.0;  ///< 0 = no turn-speed items
    StartSide startSide = StartSide::Left;
    double arcStepDeg = 45.0;
    double endLoiterS = 10.0;
    double acceptanceRadiusM = 0.0;                                ///< NAV_WAYPOINT param2, 0 = PX4 default
    double maxSpeedMS = std::numeric_limits<double>::quiet_NaN();  ///< MPC_XY_VEL_MAX, NaN = unknown (no check)
};

enum class PointKind
{
    Origin,
    LaneEnd,
    Arc,
    LaneStart,  ///< Start of lane k >= 1, the U-turn's end. A waypoint so PX4 doesn't fly a diagonal into the lane.
};

/// A path point in the cell frame (+x right, +y forward) and its lat/lon.
struct CellPoint
{
    PointKind kind = PointKind::Origin;
    double x = 0.0;
    double y = 0.0;
    QGeoCoordinate coordinate;
};

/// One mission item in MissionItem terms: param5/param6 are lat/lon for global frames.
struct Item
{
    int seq = 0;
    MAV_CMD command = MAV_CMD_NAV_WAYPOINT;
    MAV_FRAME frame = MAV_FRAME_MISSION;
    double param1 = 0.0;
    double param2 = 0.0;
    double param3 = 0.0;
    double param4 = 0.0;
    double param5 = 0.0;
    double param6 = 0.0;
    double param7 = 0.0;
    bool autoContinue = true;
    bool current = false;

    /// MISSION_ITEM_INT x/y as PlanManager sends them: degrees * 1e7 truncated toward zero for global frames.
    [[nodiscard]] int32_t wireX() const;
    [[nodiscard]] int32_t wireY() const;
};

struct Estimates
{
    int laneCount = 0;
    int turnCount = 0;
    double laneLengthM = 0.0;
    double arcLengthM = 0.0;
    double laneTimeS = 0.0;
    double arcTimeS = 0.0;
    double flightTimeS = 0.0;  ///< Lanes + arcs
    double loiterS = 0.0;
    double totalTimeS = 0.0;   ///< Flight + loiter
};

struct Result
{
    QString error;  ///< Empty when the grid was generated
    QList<double> laneYs;
    QList<CellPoint> points;
    QList<Item> items;  ///< Wire order: seq 0…N-1, no home item
    Estimates estimates;
    QString summary;

    [[nodiscard]] bool isValid() const { return error.isEmpty(); }
};

/// Returns an empty string when the parameters make a valid grid, otherwise a user-facing reason.
[[nodiscard]] QString validate(const Params& params);

/// Validates, then builds everything. On invalid input only Result::error is set.
[[nodiscard]] Result generate(const Params& params);

/// Cell frame (+x right, +y forward of headingDeg) to lat/lon: spec §4 NED rotation, then
/// origin.atDistanceAndAzimuth(hypot(n, e), atan2(e, n)).
[[nodiscard]] QGeoCoordinate cellToCoordinate(const QGeoCoordinate& origin, double headingDeg, double x, double y);

/// Confirm slider text, e.g. "100×100 m, 20 m lanes, ~2.4 min, alt 60 m". The time excludes the end loiter.
[[nodiscard]] QString summaryLine(const Params& params, const Estimates& estimates);

/// Items shaped for PlanManager::writeMissionItems on PX4, which drops index 0 as the home item and shifts every
/// other seq down by one: a placeholder home item at the origin (seq 0), then result.items at seq 1…N.
/// The caller owns the returned items (writeMissionItems takes ownership of them).
[[nodiscard]] QList<MissionItem*> planManagerItems(const Params& params, const Result& result,
                                                   QObject* parent = nullptr);

}  // namespace GridGenerator
