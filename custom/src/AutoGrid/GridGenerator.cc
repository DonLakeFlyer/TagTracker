#include "GridGenerator.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QCoreApplication>
#include <QtCore/QtMath>

#include "MissionItem.h"

namespace GridGenerator {

namespace {

/// y_k = min(k * s, D) for k = 0 … ceil(D / s), dropping a lane that duplicates the previous y.
QList<double> laneYs(double depth, double spacing)
{
    QList<double> ys;
    const int lastK = static_cast<int>(std::ceil(depth / spacing));
    for (int k = 0; k <= lastK; k++) {
        const double y = std::min(k * spacing, depth);
        if (!ys.isEmpty() && y - ys.last() <= LANE_EPS_M) {
            continue;
        }
        ys.append(y);
    }
    return ys;
}

/// Multiples of the arc step strictly between 0 and 180 degrees.
QList<double> arcAnglesDeg(double arcStep)
{
    QList<double> angles;
    for (int j = 1; j * arcStep < 180.0 - ARC_EPS_DEG; j++) {
        angles.append(j * arcStep);
    }
    return angles;
}

/// Origin, the lane 0 half leg, then per U-turn: arc points, the next lane start and that lane's end. Lane k ends at
/// x_e = σ·W/2·(-1)^k. The U-turn after it is a semicircle at that edge, centre (x_e, (y_k + y_k+1) / 2), radius
/// (y_k+1 - y_k) / 2, bulging outward. θ runs from 0 at the end of lane k to 180 at the start of lane k+1:
/// x = x_e + e·r·sin θ, y = yc - r·cos θ, e = sign(x_e). The θ = 180 end is the lane start, at exactly (x_e, y_k+1).
QList<CellPoint> cellPath(const Params& params, const QList<double>& ys)
{
    const double sigma = (params.startSide == StartSide::Left) ? -1.0 : 1.0;
    const QList<double> angles = arcAnglesDeg(params.arcStepDeg);

    QList<CellPoint> points;
    points.append({PointKind::Origin, 0.0, 0.0, {}});
    double xEnd = sigma * params.widthM / 2.0;
    points.append({PointKind::LaneEnd, xEnd, ys.first(), {}});
    for (qsizetype k = 0; k + 1 < ys.size(); k++) {
        const double xEdge = xEnd;
        const double e = (xEdge > 0) ? 1.0 : -1.0;
        const double yc = (ys[k] + ys[k + 1]) / 2.0;
        const double r = (ys[k + 1] - ys[k]) / 2.0;
        for (const double theta : angles) {
            const double t = qDegreesToRadians(theta);
            points.append({PointKind::Arc, xEdge + e * r * std::sin(t), yc - r * std::cos(t), {}});
        }
        points.append({PointKind::LaneStart, xEdge, ys[k + 1], {}});
        xEnd = -xEdge;
        points.append({PointKind::LaneEnd, xEnd, ys[k + 1], {}});
    }
    return points;
}

Item speedItem(double speed)
{
    Item item;
    item.command = MAV_CMD_DO_CHANGE_SPEED;
    item.frame = MAV_FRAME_MISSION;
    item.param1 = 1.0;   // Ground speed
    item.param2 = speed;
    item.param3 = -1.0;  // Throttle unchanged
    return item;
}

Item navItem(const Params& params, MAV_CMD command, const QGeoCoordinate& coordinate, double param1, double param2)
{
    Item item;
    item.command = command;
    item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    item.param1 = param1;
    item.param2 = param2;
    item.param4 = params.headingDeg;  // Hold the confirm heading
    item.param5 = coordinate.latitude();
    item.param6 = coordinate.longitude();
    item.param7 = params.altitudeM;
    return item;
}

/// Spec §5. Per U-turn: turn speed → arc points → lane start → cruise → lane end, so the whole turn, including the
/// leg into the lane start, is flown at turn speed. Turn speed 0 leaves out both per-turn speed items.
QList<Item> missionItems(const Params& params, const QList<CellPoint>& points)
{
    const bool turnSpeedItems = params.turnSpeedMS > 0;

    QList<Item> items;
    items.append(speedItem(params.cruiseSpeedMS));
    PointKind previousKind = PointKind::Origin;
    for (const CellPoint& point : points) {
        if (turnSpeedItems) {
            if ((point.kind == PointKind::Arc) && (previousKind == PointKind::LaneEnd)) {
                items.append(speedItem(params.turnSpeedMS));
            } else if ((point.kind == PointKind::LaneEnd) && (previousKind == PointKind::LaneStart)) {
                items.append(speedItem(params.cruiseSpeedMS));
            }
        }
        items.append(navItem(params, MAV_CMD_NAV_WAYPOINT, point.coordinate, 0.0, params.acceptanceRadiusM));
        previousKind = point.kind;
    }
    items.append(navItem(params, MAV_CMD_NAV_LOITER_TIME, points.last().coordinate, params.endLoiterS, 0.0));

    Item rtl;
    rtl.command = MAV_CMD_NAV_RETURN_TO_LAUNCH;
    rtl.frame = MAV_FRAME_MISSION;
    items.append(rtl);

    for (qsizetype i = 0; i < items.size(); i++) {
        items[i].seq = static_cast<int>(i);
        items[i].current = (i == 0);
    }
    return items;
}

Estimates estimate(const Params& params, const QList<double>& ys)
{
    Estimates est;
    est.laneCount = static_cast<int>(ys.size());
    est.turnCount = est.laneCount - 1;
    est.laneLengthM = params.widthM / 2.0 + (est.laneCount - 1) * params.widthM;
    for (qsizetype k = 0; k + 1 < ys.size(); k++) {
        est.arcLengthM += M_PI * (ys[k + 1] - ys[k]) / 2.0;
    }
    est.laneTimeS = est.laneLengthM / params.cruiseSpeedMS;
    est.arcTimeS = est.arcLengthM / ((params.turnSpeedMS > 0) ? params.turnSpeedMS : params.cruiseSpeedMS);
    est.flightTimeS = est.laneTimeS + est.arcTimeS;
    est.loiterS = params.endLoiterS;
    est.totalTimeS = est.flightTimeS + params.endLoiterS;
    return est;
}

int32_t wireCoordinate(MAV_FRAME frame, double value)
{
    // PlanManager passes param * 1e7 (a double) to an int32_t argument, which truncates toward zero.
    return static_cast<int32_t>((frame == MAV_FRAME_MISSION) ? value : value * 1e7);
}

}  // namespace

int32_t Item::wireX() const
{
    return wireCoordinate(frame, param5);
}

int32_t Item::wireY() const
{
    return wireCoordinate(frame, param6);
}

QString validate(const Params& params)
{
    const double numbers[] = {params.headingDeg, params.altitudeM,        params.widthM,      params.depthM,
                              params.spacingM,   params.cruiseSpeedMS,    params.turnSpeedMS, params.arcStepDeg,
                              params.endLoiterS, params.acceptanceRadiusM};
    for (const double value : numbers) {
        if (!std::isfinite(value)) {
            return QCoreApplication::translate("GridGenerator", "Grid parameters must be finite numbers.");
        }
    }
    if (!params.origin.isValid()) {
        return QCoreApplication::translate("GridGenerator", "The grid origin is not a valid position.");
    }
    if ((params.headingDeg < 0.0) || (params.headingDeg >= 360.0)) {
        return QCoreApplication::translate("GridGenerator", "Heading must be from 0 up to 360 degrees.");
    }
    if (params.altitudeM <= 0.0) {
        return QCoreApplication::translate("GridGenerator", "Altitude must be above the launch point.");
    }
    if ((params.widthM <= 0.0) || (params.depthM <= 0.0) || (params.spacingM <= 0.0)) {
        return QCoreApplication::translate("GridGenerator",
                                           "Grid width, depth and lane spacing must be greater than 0.");
    }
    if (params.spacingM > params.depthM) {
        return QCoreApplication::translate("GridGenerator", "Lane spacing must not be more than the grid depth.");
    }
    if ((params.arcStepDeg < MIN_ARC_STEP_DEG) || (params.arcStepDeg > MAX_ARC_STEP_DEG)) {
        return QCoreApplication::translate("GridGenerator", "Arc step must be from 10 to 90 degrees.");
    }
    if (params.cruiseSpeedMS <= 0.0) {
        return QCoreApplication::translate("GridGenerator", "Cruise speed must be greater than 0.");
    }
    if (params.turnSpeedMS < 0.0) {
        return QCoreApplication::translate("GridGenerator",
                                           "Turn speed must be greater than 0, or 0 for no turn speed.");
    }
    if (params.endLoiterS < 0.0) {
        return QCoreApplication::translate("GridGenerator", "End loiter time must not be negative.");
    }
    if (params.acceptanceRadiusM < 0.0) {
        return QCoreApplication::translate("GridGenerator", "Acceptance radius must not be negative.");
    }
    if (std::isfinite(params.maxSpeedMS)) {
        if (params.cruiseSpeedMS > params.maxSpeedMS) {
            return QCoreApplication::translate("GridGenerator",
                                               "Cruise speed %1 m/s is above the vehicle maximum of %2 m/s.")
                .arg(params.cruiseSpeedMS)
                .arg(params.maxSpeedMS);
        }
        if (params.turnSpeedMS > params.maxSpeedMS) {
            return QCoreApplication::translate("GridGenerator",
                                               "Turn speed %1 m/s is above the vehicle maximum of %2 m/s.")
                .arg(params.turnSpeedMS)
                .arg(params.maxSpeedMS);
        }
    }
    return QString();
}

QGeoCoordinate cellToCoordinate(const QGeoCoordinate& origin, double headingDeg, double x, double y)
{
    const double psi = qDegreesToRadians(headingDeg);
    const double north = y * std::cos(psi) - x * std::sin(psi);
    const double east = y * std::sin(psi) + x * std::cos(psi);
    return origin.atDistanceAndAzimuth(std::hypot(north, east), qRadiansToDegrees(std::atan2(east, north)));
}

Result generate(const Params& params)
{
    Result result;
    result.error = validate(params);
    if (!result.error.isEmpty()) {
        return result;
    }

    result.laneYs = laneYs(params.depthM, params.spacingM);
    result.points = cellPath(params, result.laneYs);
    for (CellPoint& point : result.points) {
        point.coordinate = cellToCoordinate(params.origin, params.headingDeg, point.x, point.y);
    }
    result.items = missionItems(params, result.points);
    result.estimates = estimate(params, result.laneYs);
    result.summary = summaryLine(params, result.estimates);
    return result;
}

QString summaryLine(const Params& params, const Estimates& estimates)
{
    // Flight time only. Whether to add the end loiter is spec §12's open question; change it here.
    return QStringLiteral("%1×%2 m, %3 m lanes, ~%4 min, alt %5 m")
        .arg(QString::number(params.widthM, 'g', 6))
        .arg(QString::number(params.depthM, 'g', 6))
        .arg(QString::number(params.spacingM, 'g', 6))
        .arg(QString::number(estimates.flightTimeS / 60.0, 'f', 1))
        .arg(QString::number(params.altitudeM, 'f', 0));
}

QList<MissionItem*> planManagerItems(const Params& params, const Result& result, QObject* parent)
{
    QList<MissionItem*> planItems;
    if (!result.isValid() || result.items.isEmpty()) {
        return planItems;
    }

    const QGeoCoordinate& origin = params.origin;
    planItems.append(new MissionItem(0, MAV_CMD_NAV_WAYPOINT, MAV_FRAME_GLOBAL_RELATIVE_ALT, 0.0, 0.0, 0.0, 0.0,
                                     origin.latitude(), origin.longitude(), params.altitudeM, true, false, parent));
    for (const Item& item : result.items) {
        planItems.append(new MissionItem(item.seq + 1, item.command, item.frame, item.param1, item.param2, item.param3,
                                         item.param4, item.param5, item.param6, item.param7, item.autoContinue, false,
                                         parent));
    }
    return planItems;
}

}  // namespace GridGenerator
