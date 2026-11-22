#include "GridGeneratorTest.h"

#include <cmath>
#include <functional>
#include <limits>

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>
#include <QtCore/QtMath>
#include <QtTest/QTest>

#include "GridGenerator.h"
#include "MissionItem.h"

using namespace GridGenerator;

Q_DECLARE_METATYPE(GridGenerator::Params)
Q_DECLARE_METATYPE(GridGenerator::StartSide)

namespace {

// Tolerances from uavrt_auto_grid tools/grid_ref/README.md
constexpr double CELL_TOL_M = 1e-9;
constexpr double LATLON_TOL_DEG = 1e-11;
constexpr int WIRE_TOL = 1;

const QGeoCoordinate REF_ORIGIN(47.397742, 8.545594);

Params defaultParams()
{
    Params params;
    params.origin = REF_ORIGIN;
    params.altitudeM = 30.0;
    return params;
}

QList<CellPoint> pointsOfKind(const Result& result, PointKind kind)
{
    QList<CellPoint> points;
    for (const CellPoint& point : result.points) {
        if (point.kind == kind) {
            points.append(point);
        }
    }
    return points;
}

PointKind pointKindFromJson(const QString& kind)
{
    if (kind == QStringLiteral("origin")) {
        return PointKind::Origin;
    }
    if (kind == QStringLiteral("lane_start")) {
        return PointKind::LaneStart;
    }
    return (kind == QStringLiteral("lane_end")) ? PointKind::LaneEnd : PointKind::Arc;
}

/// The item sequence in short form, e.g. "S6 W W S3 W … L R" (speed with its value, waypoint, loiter, RTL).
QString itemPattern(const Result& result)
{
    QStringList tokens;
    for (const Item& item : result.items) {
        switch (item.command) {
            case MAV_CMD_DO_CHANGE_SPEED:
                tokens.append(QStringLiteral("S%1").arg(item.param2));
                break;
            case MAV_CMD_NAV_WAYPOINT:
                tokens.append(QStringLiteral("W"));
                break;
            case MAV_CMD_NAV_LOITER_TIME:
                tokens.append(QStringLiteral("L"));
                break;
            case MAV_CMD_NAV_RETURN_TO_LAUNCH:
                tokens.append(QStringLiteral("R"));
                break;
            default:
                tokens.append(QStringLiteral("?"));
                break;
        }
    }
    return tokens.join(QLatin1Char(' '));
}

/// MAV_FRAME_GLOBAL_RELATIVE_ALT (3) and its _INT twin (6), which grid_ref uses, are the same to PX4.
int normalizedFrame(int frame)
{
    return (frame == MAV_FRAME_GLOBAL_RELATIVE_ALT_INT) ? MAV_FRAME_GLOBAL_RELATIVE_ALT : frame;
}

bool near(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

QString where(const char* what, qsizetype index, double actual, double expected)
{
    return QStringLiteral("%1[%2]: got %3, expected %4")
        .arg(QLatin1String(what))
        .arg(index)
        .arg(actual, 0, 'g', 17)
        .arg(expected, 0, 'g', 17);
}

}  // namespace

void GridGeneratorTest::_referenceMatch_data()
{
    QTest::addColumn<QString>("fileName");

    for (const char* name : {"psi000_left", "psi090_left", "psi180_left", "psi270_left", "psi037.5_left",
                             "psi000_right", "example_w10_d10_s2.5", "clamped_d90_s20", "turn0_psi000_left"}) {
        QTest::newRow(name) << QStringLiteral(":/unittest/grid_ref/%1.json").arg(QLatin1String(name));
    }
}

void GridGeneratorTest::_referenceMatch()
{
    QFETCH(QString, fileName);

    QFile file(fileName);
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(fileName));
    QJsonParseError parseError;
    const QJsonObject ref = QJsonDocument::fromJson(file.readAll(), &parseError).object();
    QCOMPARE(parseError.error, QJsonParseError::NoError);

    // grid_proto/2 is T1 Revision 1, with lane_start points (spec §4 Change 1); older files must be re-copied.
    QCOMPARE(ref[QStringLiteral("format")].toString(), QStringLiteral("grid_proto/2"));
    const QJsonObject constants = ref[QStringLiteral("constants")].toObject();
    QVERIFY(constants[QStringLiteral("lane_eps_m")].toDouble() == LANE_EPS_M);
    QVERIFY(constants[QStringLiteral("arc_eps_deg")].toDouble() == ARC_EPS_DEG);

    const QJsonObject in = ref[QStringLiteral("inputs")].toObject();
    Params params;
    params.origin = QGeoCoordinate(in[QStringLiteral("lat")].toDouble(), in[QStringLiteral("lon")].toDouble());
    params.headingDeg = in[QStringLiteral("heading")].toDouble();
    params.altitudeM = in[QStringLiteral("alt")].toDouble();
    params.widthM = in[QStringLiteral("width")].toDouble();
    params.depthM = in[QStringLiteral("depth")].toDouble();
    params.spacingM = in[QStringLiteral("spacing")].toDouble();
    params.cruiseSpeedMS = in[QStringLiteral("cruise")].toDouble();
    params.turnSpeedMS = in[QStringLiteral("turn_speed")].toDouble();
    params.startSide =
        (in[QStringLiteral("side")].toString() == QStringLiteral("left")) ? StartSide::Left : StartSide::Right;
    params.arcStepDeg = in[QStringLiteral("arc_step")].toDouble();
    params.endLoiterS = in[QStringLiteral("loiter")].toDouble();
    params.acceptanceRadiusM = in[QStringLiteral("accept_radius")].toDouble();

    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    const QJsonArray refYs = ref[QStringLiteral("lane_ys")].toArray();
    QCOMPARE(result.laneYs.size(), refYs.size());
    for (qsizetype i = 0; i < refYs.size(); i++) {
        QVERIFY2(result.laneYs[i] == refYs[i].toDouble(),
                 qPrintable(where("lane_ys", i, result.laneYs[i], refYs[i].toDouble())));
    }

    const QJsonArray refPoints = ref[QStringLiteral("cell_points")].toArray();
    QCOMPARE(result.points.size(), refPoints.size());
    for (qsizetype i = 0; i < refPoints.size(); i++) {
        const QJsonObject expected = refPoints[i].toObject();
        const CellPoint& point = result.points[i];
        QVERIFY2(point.kind == pointKindFromJson(expected[QStringLiteral("kind")].toString()),
                 qPrintable(QStringLiteral("cell_points[%1] kind").arg(i)));
        const double x = expected[QStringLiteral("x")].toDouble();
        const double y = expected[QStringLiteral("y")].toDouble();
        const double lat = expected[QStringLiteral("lat")].toDouble();
        const double lon = expected[QStringLiteral("lon")].toDouble();
        QVERIFY2(near(point.x, x, CELL_TOL_M), qPrintable(where("x", i, point.x, x)));
        QVERIFY2(near(point.y, y, CELL_TOL_M), qPrintable(where("y", i, point.y, y)));
        QVERIFY2(near(point.coordinate.latitude(), lat, LATLON_TOL_DEG),
                 qPrintable(where("lat", i, point.coordinate.latitude(), lat)));
        QVERIFY2(near(point.coordinate.longitude(), lon, LATLON_TOL_DEG),
                 qPrintable(where("lon", i, point.coordinate.longitude(), lon)));
    }

    const QJsonArray refItems = ref[QStringLiteral("mission_items")].toArray();
    QCOMPARE(result.items.size(), refItems.size());
    for (qsizetype i = 0; i < refItems.size(); i++) {
        const QJsonObject expected = refItems[i].toObject();
        const Item& item = result.items[i];
        const QByteArray tag = QStringLiteral("mission_items[%1]").arg(i).toUtf8();
        QVERIFY2(item.seq == expected[QStringLiteral("seq")].toInt(), tag.constData());
        QVERIFY2(normalizedFrame(item.frame) == normalizedFrame(expected[QStringLiteral("frame")].toInt()),
                 tag.constData());
        QVERIFY2(item.command == expected[QStringLiteral("command")].toInt(), tag.constData());
        QVERIFY2(item.current == (expected[QStringLiteral("current")].toInt() == 1), tag.constData());
        QVERIFY2(item.autoContinue == (expected[QStringLiteral("autocontinue")].toInt() == 1), tag.constData());
        QVERIFY2(item.param1 == expected[QStringLiteral("param1")].toDouble(), tag.constData());
        QVERIFY2(item.param2 == expected[QStringLiteral("param2")].toDouble(), tag.constData());
        QVERIFY2(item.param3 == expected[QStringLiteral("param3")].toDouble(), tag.constData());
        QVERIFY2(item.param4 == expected[QStringLiteral("param4")].toDouble(), tag.constData());
        QVERIFY2(std::abs(item.wireX() - expected[QStringLiteral("x")].toInteger()) <= WIRE_TOL,
                 qPrintable(where("x", i, item.wireX(), expected[QStringLiteral("x")].toDouble())));
        QVERIFY2(std::abs(item.wireY() - expected[QStringLiteral("y")].toInteger()) <= WIRE_TOL,
                 qPrintable(where("y", i, item.wireY(), expected[QStringLiteral("y")].toDouble())));
        QVERIFY2(item.param7 == expected[QStringLiteral("z")].toDouble(), tag.constData());
    }

    const QJsonObject refEst = ref[QStringLiteral("estimates")].toObject();
    const Estimates& est = result.estimates;
    QCOMPARE(est.laneCount, refEst[QStringLiteral("lane_count")].toInt());
    QCOMPARE(est.turnCount, refEst[QStringLiteral("turn_count")].toInt());
    const QList<std::pair<const char*, double>> estimates = {
        {"lane_length_m", est.laneLengthM}, {"arc_length_m", est.arcLengthM},   {"lane_time_s", est.laneTimeS},
        {"arc_time_s", est.arcTimeS},       {"flight_time_s", est.flightTimeS}, {"loiter_s", est.loiterS},
        {"total_time_s", est.totalTimeS},
    };
    for (const auto& [key, actual] : estimates) {
        const double expected = refEst[QLatin1String(key)].toDouble();
        QVERIFY2(actual == expected, qPrintable(where(key, 0, actual, expected)));
    }

    QCOMPARE(result.summary, ref[QStringLiteral("summary")].toString());
}

void GridGeneratorTest::_workedExamplePath()
{
    Params params = defaultParams();
    params.widthM = 10.0;
    params.depthM = 10.0;
    params.spacingM = 2.5;

    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    // Spec §4: (0,0) → (−5,0) → U-turn → (−5,2.5) → (5,2.5) → U-turn → (5,5) → (−5,5) → U-turn → (−5,7.5) → (5,7.5)
    // → U-turn → (5,10) → (−5,10)
    const QList<std::pair<double, double>> expected = {{0.0, 0.0},  {-5.0, 0.0}, {-5.0, 2.5}, {5.0, 2.5},
                                                       {5.0, 5.0},  {-5.0, 5.0}, {-5.0, 7.5}, {5.0, 7.5},
                                                       {5.0, 10.0}, {-5.0, 10.0}};
    QList<std::pair<double, double>> path;
    for (const CellPoint& point : result.points) {
        if (point.kind != PointKind::Arc) {
            path.append({point.x, point.y});
        }
    }
    QCOMPARE(path, expected);
    QCOMPARE(result.points.last().kind, PointKind::LaneEnd);

    // The loiter and RTL follow the last lane end.
    QCOMPARE(result.items[result.items.size() - 2].command, MAV_CMD_NAV_LOITER_TIME);
    QCOMPARE(result.items[result.items.size() - 2].param1, 10.0);
    QCOMPARE(result.items.last().command, MAV_CMD_NAV_RETURN_TO_LAUNCH);
}

void GridGeneratorTest::_defaultShape_data()
{
    QTest::addColumn<StartSide>("side");
    QTest::addColumn<double>("firstLaneEndX");

    QTest::newRow("left") << StartSide::Left << -50.0;
    QTest::newRow("right") << StartSide::Right << 50.0;
}

void GridGeneratorTest::_defaultShape()
{
    QFETCH(StartSide, side);
    QFETCH(double, firstLaneEndX);

    Params params = defaultParams();
    params.startSide = side;
    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    QCOMPARE(result.laneYs, (QList<double>{0.0, 20.0, 40.0, 60.0, 80.0, 100.0}));
    QCOMPARE(result.estimates.laneCount, 6);
    QCOMPARE(result.estimates.turnCount, 5);

    // 45° arc step: 3 interior points per U-turn
    const QList<CellPoint> arcs = pointsOfKind(result, PointKind::Arc);
    QCOMPARE(arcs.size(), 5 * 3);
    for (const CellPoint& arc : arcs) {
        QVERIFY2(std::abs(arc.x) > params.widthM / 2.0, qPrintable(QString::number(arc.x)));
    }

    // Lane 0 is a half leg to the start side; lane ends then alternate edges.
    const QList<CellPoint> laneEnds = pointsOfKind(result, PointKind::LaneEnd);
    QCOMPARE(laneEnds.size(), 6);
    for (qsizetype k = 0; k < laneEnds.size(); k++) {
        QCOMPARE(laneEnds[k].x, (k % 2 == 0) ? firstLaneEndX : -firstLaneEndX);
        QCOMPARE(laneEnds[k].y, result.laneYs[k]);
    }

    // Every U-turn bulges out on the side of the lane end it starts from, and ends exactly at the next lane's start.
    const QList<CellPoint> laneStarts = pointsOfKind(result, PointKind::LaneStart);
    QCOMPARE(laneStarts.size(), 5);
    qsizetype arcIndex = 0;
    for (qsizetype k = 0; k + 1 < laneEnds.size(); k++) {
        for (int j = 0; j < 3; j++, arcIndex++) {
            QVERIFY(std::signbit(arcs[arcIndex].x) == std::signbit(laneEnds[k].x));
            QVERIFY((arcs[arcIndex].y > laneEnds[k].y) && (arcs[arcIndex].y < laneEnds[k + 1].y));
        }
        QVERIFY(laneStarts[k].x == laneEnds[k].x);
        QVERIFY(laneStarts[k].y == result.laneYs[k + 1]);
    }
    QCOMPARE(result.points.size(), 27);

    // Per turn: turn speed → arcs → lane start → cruise → lane end. Every point has one waypoint, in order.
    QCOMPARE(itemPattern(result),
             QStringLiteral("S6 W W") + QStringLiteral(" S3 W W W W S6 W").repeated(5) + QStringLiteral(" L R"));
    qsizetype pointIndex = 0;
    for (const Item& item : result.items) {
        if (item.command == MAV_CMD_NAV_WAYPOINT) {
            const QGeoCoordinate& coordinate = result.points[pointIndex++].coordinate;
            QVERIFY((item.param5 == coordinate.latitude()) && (item.param6 == coordinate.longitude()));
            QVERIFY((item.param4 == params.headingDeg) && (item.param7 == params.altitudeM));
        }
    }
    QCOMPARE(pointIndex, result.points.size());
    QCOMPARE(result.summary, QStringLiteral("100×100 m, 20 m lanes, ~2.6 min, alt 30 m"));
}

void GridGeneratorTest::_cellToCoordinate_data()
{
    QTest::addColumn<double>("heading");
    QTest::addColumn<double>("x");
    QTest::addColumn<double>("y");
    QTest::addColumn<double>("expectedAzimuth");

    QTest::newRow("psi 0 forward is north") << 0.0 << 0.0 << 10.0 << 0.0;
    QTest::newRow("psi 90 forward is east") << 90.0 << 0.0 << 10.0 << 90.0;
    QTest::newRow("psi 180 forward is south") << 180.0 << 0.0 << 10.0 << 180.0;
    QTest::newRow("psi 270 forward is west") << 270.0 << 0.0 << 10.0 << 270.0;
    QTest::newRow("psi 0 right is east") << 0.0 << 10.0 << 0.0 << 90.0;
    QTest::newRow("psi 90 left is north") << 90.0 << -10.0 << 0.0 << 0.0;
    QTest::newRow("psi 37.5 forward") << 37.5 << 0.0 << 10.0 << 37.5;
    QTest::newRow("psi 37.5 forward-right") << 37.5 << 10.0 << 10.0 << 82.5;
}

void GridGeneratorTest::_cellToCoordinate()
{
    QFETCH(double, heading);
    QFETCH(double, x);
    QFETCH(double, y);
    QFETCH(double, expectedAzimuth);

    const QGeoCoordinate point = cellToCoordinate(REF_ORIGIN, heading, x, y);
    QVERIFY2(near(REF_ORIGIN.distanceTo(point), std::hypot(x, y), 1e-6),
             qPrintable(QString::number(REF_ORIGIN.distanceTo(point), 'g', 17)));
    const double azimuthError = std::remainder(REF_ORIGIN.azimuthTo(point) - expectedAzimuth, 360.0);
    QVERIFY2(std::abs(azimuthError) < 1e-6, qPrintable(QString::number(REF_ORIGIN.azimuthTo(point), 'g', 17)));

    const QGeoCoordinate origin = cellToCoordinate(REF_ORIGIN, heading, 0.0, 0.0);
    QVERIFY(near(origin.latitude(), REF_ORIGIN.latitude(), LATLON_TOL_DEG));
    QVERIFY(near(origin.longitude(), REF_ORIGIN.longitude(), LATLON_TOL_DEG));
}

void GridGeneratorTest::_clampedLastLane()
{
    Params params = defaultParams();
    params.depthM = 90.0;
    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    QCOMPARE(result.laneYs, (QList<double>{0.0, 20.0, 40.0, 60.0, 80.0, 90.0}));
    QCOMPARE(result.points.last().y, 90.0);

    // The last U-turn has radius 5 m, so its middle point is 5 m outside the edge at y = 85.
    const QList<CellPoint> arcs = pointsOfKind(result, PointKind::Arc);
    QVERIFY(near(std::abs(arcs[arcs.size() - 2].x), 55.0, CELL_TOL_M));
    QVERIFY(near(arcs[arcs.size() - 2].y, 85.0, CELL_TOL_M));
    QVERIFY(near(result.estimates.arcLengthM, M_PI * 90.0 / 2.0, 1e-9));
}

void GridGeneratorTest::_sliverLaneDropped()
{
    // ceil(4.9 / 0.7) = 8 gives 9 candidate lanes, but 7 * 0.7 is within LANE_EPS_M of 4.9, so the last is dropped.
    Params params = defaultParams();
    params.depthM = 4.9;
    params.spacingM = 0.7;
    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    QCOMPARE(result.laneYs.size(), 8);
    QVERIFY(near(result.laneYs.last(), 4.9, 1e-12));
    for (qsizetype k = 1; k < result.laneYs.size(); k++) {
        QVERIFY(result.laneYs[k] - result.laneYs[k - 1] > 0.5);
    }
}

void GridGeneratorTest::_turnSpeedZero()
{
    Params params = defaultParams();
    params.turnSpeedMS = 0.0;
    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    // Only the initial cruise speed item remains; the arcs are estimated at cruise speed.
    QCOMPARE(itemPattern(result),
             QStringLiteral("S6 W W") + QStringLiteral(" W W W W W").repeated(5) + QStringLiteral(" L R"));
    QCOMPARE(result.estimates.arcTimeS, result.estimates.arcLengthM / params.cruiseSpeedMS);
}

void GridGeneratorTest::_planManagerItems()
{
    const Params params = defaultParams();
    const Result result = generate(params);
    QVERIFY2(result.isValid(), qPrintable(result.error));

    const QList<MissionItem*> planItems = planManagerItems(params, result);
    QCOMPARE(planItems.size(), result.items.size() + 1);

    const MissionItem* home = planItems.first();
    QCOMPARE(home->sequenceNumber(), 0);
    QCOMPARE(home->command(), MAV_CMD_NAV_WAYPOINT);
    QCOMPARE(home->param5(), REF_ORIGIN.latitude());
    QCOMPARE(home->param6(), REF_ORIGIN.longitude());

    for (qsizetype i = 0; i < result.items.size(); i++) {
        const Item& item = result.items[i];
        const MissionItem* planItem = planItems[i + 1];
        QCOMPARE(planItem->sequenceNumber(), item.seq + 1);
        QCOMPARE(planItem->command(), item.command);
        QCOMPARE(planItem->frame(), item.frame);
        QCOMPARE(planItem->autoContinue(), item.autoContinue);
        QVERIFY(planItem->param1() == item.param1);
        QVERIFY(planItem->param2() == item.param2);
        QVERIFY(planItem->param3() == item.param3);
        QVERIFY(planItem->param4() == item.param4);
        QVERIFY(planItem->param5() == item.param5);
        QVERIFY(planItem->param6() == item.param6);
        QVERIFY(planItem->param7() == item.param7);
    }
    qDeleteAll(planItems);

    Params invalid = params;
    invalid.spacingM = 0.0;
    QVERIFY(planManagerItems(invalid, generate(invalid)).isEmpty());
}

void GridGeneratorTest::_validate_data()
{
    using Change = std::function<void(Params&)>;
    QTest::addColumn<Params>("params");
    QTest::addColumn<bool>("valid");

    const auto row = [](const char* name, const Change& change, bool valid) {
        Params params = defaultParams();
        change(params);
        QTest::newRow(name) << params << valid;
    };
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

    row("defaults", [](Params&) {}, true);
    row("invalid origin", [](Params& p) { p.origin = QGeoCoordinate(); }, false);
    row("heading 360", [](Params& p) { p.headingDeg = 360.0; }, false);
    row("heading negative", [](Params& p) { p.headingDeg = -1.0; }, false);
    row("heading NaN", [](Params& p) { p.headingDeg = NaN; }, false);
    row("altitude 0", [](Params& p) { p.altitudeM = 0.0; }, false);
    row("width 0", [](Params& p) { p.widthM = 0.0; }, false);
    row("depth negative", [](Params& p) { p.depthM = -10.0; }, false);
    row("spacing 0", [](Params& p) { p.spacingM = 0.0; }, false);
    row("spacing above depth", [](Params& p) { p.spacingM = 101.0; }, false);
    row("spacing equals depth", [](Params& p) { p.spacingM = 100.0; }, true);
    row("arc step below 10", [](Params& p) { p.arcStepDeg = 9.9; }, false);
    row("arc step above 90", [](Params& p) { p.arcStepDeg = 90.1; }, false);
    row("arc step 10", [](Params& p) { p.arcStepDeg = 10.0; }, true);
    row("arc step 90", [](Params& p) { p.arcStepDeg = 90.0; }, true);
    row("cruise 0", [](Params& p) { p.cruiseSpeedMS = 0.0; }, false);
    row("cruise infinite", [](Params& p) { p.cruiseSpeedMS = std::numeric_limits<double>::infinity(); }, false);
    row("turn speed negative", [](Params& p) { p.turnSpeedMS = -1.0; }, false);
    row("turn speed 0", [](Params& p) { p.turnSpeedMS = 0.0; }, true);
    row("loiter negative", [](Params& p) { p.endLoiterS = -1.0; }, false);
    row("acceptance radius negative", [](Params& p) { p.acceptanceRadiusM = -1.0; }, false);
    row(
        "cruise above max speed",
        [](Params& p) {
            p.cruiseSpeedMS = 20.0;
            p.maxSpeedMS = 12.0;
        },
        false);
    row(
        "turn speed above max speed",
        [](Params& p) {
            p.cruiseSpeedMS = 10.0;
            p.turnSpeedMS = 13.0;
            p.maxSpeedMS = 12.0;
        },
        false);
    row(
        "speeds at max speed",
        [](Params& p) {
            p.cruiseSpeedMS = 12.0;
            p.turnSpeedMS = 12.0;
            p.maxSpeedMS = 12.0;
        },
        true);
    row(
        "max speed unknown",
        [](Params& p) {
            p.cruiseSpeedMS = 20.0;
            p.maxSpeedMS = NaN;
        },
        true);
}

void GridGeneratorTest::_validate()
{
    QFETCH(Params, params);
    QFETCH(bool, valid);

    const QString error = validate(params);
    QVERIFY2(error.isEmpty() == valid, qPrintable(error));

    const Result result = generate(params);
    QCOMPARE(result.error, error);
    QCOMPARE(result.items.isEmpty(), !valid);
    QCOMPARE(result.points.isEmpty(), !valid);
}

UT_REGISTER_TEST(GridGeneratorTest, TestLabel::Unit)
