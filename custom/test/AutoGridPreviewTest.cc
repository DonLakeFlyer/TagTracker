#include "AutoGridPreviewTest.h"

#include <cmath>

#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "AutoGridPreview.h"
#include "GridGenerator.h"

namespace {

// The settings' min/max (Custom.SettingsGroup.json)
const AutoGridPreview::Limits previewTestLimits{
    .widthMin = 10.0,
    .widthMax = 500.0,
    .depthMin = 10.0,
    .depthMax = 500.0,
    .spacingMin = 2.0,
    .spacingMax = 100.0,
};

GridGenerator::Params previewTestParams()
{
    GridGenerator::Params params;  // spec §3 defaults: 100 × 100 m, 20 m lanes
    params.origin = QGeoCoordinate(47.3977, 8.5456);
    params.headingDeg = 37.5;
    params.altitudeM = 30.0;
    params.acceptanceRadiusM = 1.0;  // within the U-turn radius down to the 2 m minimum lane spacing
    params.maxSpeedMS = 12.0;
    return params;
}

void comparePreviewToGenerator(const AutoGridPreview& preview, const GridGenerator::Params& expectedParams)
{
    const GridGenerator::Result expected = GridGenerator::generate(expectedParams);
    QVERIFY(expected.isValid());
    QVERIFY(preview.valid());
    QCOMPARE(preview.summary(), expected.summary);
    QCOMPARE(preview.result().items.count(), expected.items.count());
    QCOMPARE(preview.path().count(), expected.points.count());
    for (qsizetype i = 0; i < expected.points.count(); i++) {
        QCOMPARE(preview.path()[i].value<QGeoCoordinate>(), expected.points[i].coordinate);
    }
}

}  // namespace

void AutoGridPreviewTest::_startLatches()
{
    AutoGridPreview preview;
    QSignalSpy changedSpy(&preview, &AutoGridPreview::changed);
    const GridGenerator::Params params = previewTestParams();

    preview.start(params, previewTestLimits);

    QVERIFY(changedSpy.count() >= 1);
    QVERIFY(preview.active());
    QVERIFY(!preview.confirmed());
    QCOMPARE(preview.origin(), params.origin);
    QCOMPARE(preview.params().headingDeg, params.headingDeg);
    QCOMPARE(preview.params().altitudeM, params.altitudeM);
    QCOMPARE(preview.width(), 100.0);
    QCOMPARE(preview.depth(), 100.0);
    QCOMPARE(preview.spacing(), 20.0);
    QCOMPARE(preview.laneCount(), 6);
    QVERIFY(preview.error().isEmpty());
    comparePreviewToGenerator(preview, params);
    // The path starts at the origin, in the order flown
    QCOMPARE(preview.path().first().value<QGeoCoordinate>(), params.origin);

    // Closed W × D rectangle: ±W/2 at y = 0 and y = D
    const QVariantList outline = preview.outline();
    QCOMPARE(outline.count(), 5);
    QCOMPARE(outline.first().value<QGeoCoordinate>(), outline.last().value<QGeoCoordinate>());
    const QGeoCoordinate expectedCorners[] = {
        GridGenerator::cellToCoordinate(params.origin, params.headingDeg, -50.0, 0.0),
        GridGenerator::cellToCoordinate(params.origin, params.headingDeg, 50.0, 0.0),
        GridGenerator::cellToCoordinate(params.origin, params.headingDeg, 50.0, 100.0),
        GridGenerator::cellToCoordinate(params.origin, params.headingDeg, -50.0, 100.0),
    };
    for (int i = 0; i < 4; i++) {
        QCOMPARE(outline[i].value<QGeoCoordinate>(), expectedCorners[i]);
    }
}

void AutoGridPreviewTest::_confirmMessage()
{
    AutoGridPreview preview;
    preview.start(previewTestParams(), previewTestLimits);

    QCOMPARE(preview.confirmMessage(),
             QStringLiteral("100×100 m, 20 m lanes, ~2.6 min, alt 30 m. The cell and the return path must be clear.\n"
                            "Warning: detection is not running."));

    QSignalSpy changedSpy(&preview, &AutoGridPreview::changed);
    preview.setDetectionRunning(true);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(preview.confirmMessage(),
             QStringLiteral("100×100 m, 20 m lanes, ~2.6 min, alt 30 m. The cell and the return path must be clear."));
}

void AutoGridPreviewTest::_adjustRegenerates()
{
    AutoGridPreview preview;
    GridGenerator::Params expected = previewTestParams();
    preview.start(expected, previewTestLimits);
    const QString startSummary = preview.summary();
    QSignalSpy changedSpy(&preview, &AutoGridPreview::changed);

    preview.adjustWidth(1);
    expected.widthM = 120.0;
    QCOMPARE(preview.width(), 120.0);
    comparePreviewToGenerator(preview, expected);
    QVERIFY(preview.summary().startsWith(QStringLiteral("120×100 m, 20 m lanes")));
    QVERIFY(preview.summary() != startSummary);
    QVERIFY(preview.confirmMessage().startsWith(preview.summary()));

    // Depth steps by one lane spacing
    preview.adjustDepth(-1);
    expected.depthM = 80.0;
    QCOMPARE(preview.depth(), 80.0);
    QCOMPARE(preview.laneCount(), 5);
    comparePreviewToGenerator(preview, expected);

    // A spacing change re-snaps the depth to whole lanes: 80 / 25 = 3.2 lanes, so 75 m
    preview.adjustSpacing(1);
    expected.spacingM = 25.0;
    expected.depthM = 75.0;
    QCOMPARE(preview.spacing(), 25.0);
    QCOMPARE(preview.depth(), 75.0);
    QCOMPARE(preview.laneCount(), 4);
    comparePreviewToGenerator(preview, expected);
    QVERIFY(preview.summary().startsWith(QStringLiteral("120×75 m, 25 m lanes")));

    QCOMPARE(changedSpy.count(), 3);
    // The latched pose never changes
    QCOMPARE(preview.params().origin, previewTestParams().origin);
    QCOMPARE(preview.params().headingDeg, previewTestParams().headingDeg);
    QCOMPARE(preview.params().altitudeM, previewTestParams().altitudeM);

    // A step that the limits swallow regenerates nothing
    preview.start(expected, previewTestLimits);
    changedSpy.clear();
    preview.adjustWidth(100);
    QCOMPARE(preview.width(), 500.0);
    preview.adjustWidth(1);
    QCOMPARE(changedSpy.count(), 1);
}

void AutoGridPreviewTest::_clamping_data()
{
    QTest::addColumn<char>("field");  // 'W', 'D' or 's'
    QTest::addColumn<double>("width");
    QTest::addColumn<double>("depth");
    QTest::addColumn<double>("spacing");
    QTest::addColumn<int>("steps");
    QTest::addColumn<double>("expected");

    QTest::newRow("width +1") << 'W' << 100.0 << 100.0 << 20.0 << 1 << 120.0;
    QTest::newRow("width -2") << 'W' << 100.0 << 100.0 << 20.0 << -2 << 60.0;
    QTest::newRow("width at max") << 'W' << 500.0 << 100.0 << 20.0 << 1 << 500.0;
    QTest::newRow("width near max") << 'W' << 490.0 << 100.0 << 20.0 << 1 << 500.0;
    QTest::newRow("width at min") << 'W' << 10.0 << 100.0 << 20.0 << -1 << 10.0;
    QTest::newRow("width near min") << 'W' << 25.0 << 100.0 << 20.0 << -1 << 10.0;
    QTest::newRow("depth +1 lane") << 'D' << 100.0 << 100.0 << 20.0 << 1 << 120.0;
    QTest::newRow("depth -2 lanes") << 'D' << 100.0 << 100.0 << 20.0 << -2 << 60.0;
    QTest::newRow("depth at max") << 'D' << 100.0 << 500.0 << 20.0 << 1 << 500.0;
    QTest::newRow("depth at whole-lane max") << 'D' << 100.0 << 480.0 << 30.0 << 1 << 480.0;
    QTest::newRow("depth at one spacing") << 'D' << 100.0 << 20.0 << 20.0 << -1 << 20.0;
    QTest::newRow("depth at whole-lane min") << 'D' << 100.0 << 10.0 << 2.0 << -1 << 10.0;
    QTest::newRow("spacing +1") << 's' << 100.0 << 100.0 << 20.0 << 1 << 25.0;
    QTest::newRow("spacing at max") << 's' << 100.0 << 500.0 << 100.0 << 1 << 100.0;
    QTest::newRow("spacing near min") << 's' << 100.0 << 100.0 << 5.0 << -1 << 2.0;
    QTest::newRow("spacing at min") << 's' << 100.0 << 100.0 << 2.0 << -1 << 2.0;
}

void AutoGridPreviewTest::_clamping()
{
    QFETCH(char, field);
    QFETCH(double, width);
    QFETCH(double, depth);
    QFETCH(double, spacing);
    QFETCH(int, steps);
    QFETCH(double, expected);

    GridGenerator::Params params = previewTestParams();
    params.widthM = width;
    params.depthM = depth;
    params.spacingM = spacing;
    AutoGridPreview preview;
    preview.start(params, previewTestLimits);
    QVERIFY(preview.valid());

    switch (field) {
        case 'W':
            preview.adjustWidth(steps);
            QCOMPARE(preview.width(), expected);
            break;
        case 'D':
            preview.adjustDepth(steps);
            QCOMPARE(preview.depth(), expected);
            break;
        default:
            preview.adjustSpacing(steps);
            QCOMPARE(preview.spacing(), expected);
            break;
    }
    // Depth stays a whole number of lane spacings
    QCOMPARE(std::fmod(preview.depth(), preview.spacing()), 0.0);
    QCOMPARE(preview.laneCount(), qRound(preview.depth() / preview.spacing()) + 1);
    QVERIFY(preview.valid());
    // The values that weren't adjusted are unchanged
    QCOMPARE(preview.width(), (field == 'W') ? expected : width);
    QCOMPARE(preview.depth(), (field == 'D') ? expected : depth);
    QCOMPARE(preview.spacing(), (field == 's') ? expected : spacing);
}

void AutoGridPreviewTest::_depthSnapsToWholeLanes()
{
    // The brief's example: settings D = 90, s = 20 opens at 100 m (6 lanes)
    GridGenerator::Params params = previewTestParams();
    params.depthM = 90.0;
    AutoGridPreview preview;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.depth(), 100.0);
    QCOMPARE(preview.laneCount(), 6);
    QVERIFY(preview.valid());
    // Every lane is a full spacing apart: no clamped short last lane
    const QList<double>& laneYs = preview.result().laneYs;
    QCOMPARE(laneYs.count(), 6);
    for (qsizetype i = 1; i < laneYs.count(); i++) {
        QCOMPARE(laneYs[i] - laneYs[i - 1], 20.0);
    }

    // s 20 → 25 keeps 100 m (5 lanes); D + then adds one spacing
    preview.adjustSpacing(1);
    QCOMPARE(preview.spacing(), 25.0);
    QCOMPARE(preview.depth(), 100.0);
    QCOMPARE(preview.laneCount(), 5);
    preview.adjustDepth(1);
    QCOMPARE(preview.depth(), 125.0);
    QCOMPARE(preview.laneCount(), 6);

    // Nearest multiple: 85 / 20 = 4.25 lanes, so 80 m
    params.depthM = 85.0;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.depth(), 80.0);

    // Spacing above the settings' depth snaps to one spacing (two lanes) instead of an error
    params.depthM = 50.0;
    params.spacingM = 80.0;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.depth(), 80.0);
    QCOMPARE(preview.laneCount(), 2);
    QVERIFY(preview.valid());

    // Within the settings' limits: the largest whole number of 30 m lanes in 500 m is 480 m
    params.depthM = 500.0;
    params.spacingM = 30.0;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.depth(), 480.0);
    QCOMPARE(preview.depthMax(), 480.0);
    // and the smallest at or above the 10 m minimum with 4 m lanes is 12 m
    params.depthM = 10.0;
    params.spacingM = 4.0;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.depth(), 12.0);
    QCOMPARE(preview.depthMin(), 12.0);
}

void AutoGridPreviewTest::_validationLimitsBlockStart()
{
    // Too long: 500 × 500 m with 20 m lanes is ~40 min
    GridGenerator::Params params = previewTestParams();
    params.widthM = 500.0;
    params.depthM = 500.0;
    AutoGridPreview preview;
    preview.start(params, previewTestLimits);
    QVERIFY(!preview.valid());
    QVERIFY2(preview.error().startsWith(QStringLiteral("Grid would take about 40 min (limit 20 min).")),
             qPrintable(preview.error()));
    QVERIFY(preview.confirmMessage().startsWith(QStringLiteral("Can't start a grid: Grid would take about")));
    QVERIFY(!preview.requestConfirm());

    // Wider lanes bring it under 20 min, live
    preview.adjustSpacing(6);
    QCOMPARE(preview.spacing(), 50.0);
    QVERIFY2(preview.valid(), qPrintable(preview.error()));

    // Acceptance radius above the U-turn radius: narrowing the lanes below 2 × 3 m blocks Start grid
    params = previewTestParams();
    params.acceptanceRadiusM = 3.0;
    params.spacingM = 10.0;
    preview.start(params, previewTestLimits);
    QVERIFY(preview.valid());
    preview.adjustSpacing(-1);
    QCOMPARE(preview.spacing(), 5.0);
    QVERIFY(!preview.valid());
    QVERIFY(
        preview.error().startsWith(QStringLiteral("Acceptance radius 3 m must not be more than the U-turn radius")));
    QVERIFY(!preview.requestConfirm());

    // Too low (latched at tap: no panel value fixes it)
    params = previewTestParams();
    params.altitudeM = 8.0;
    preview.start(params, previewTestLimits);
    QCOMPARE(preview.error(), QStringLiteral("Altitude must be at least 10 m to start a grid."));
    QVERIFY(!preview.requestConfirm());
}

void AutoGridPreviewTest::_stages()
{
    AutoGridPreview preview;
    QVERIFY(!preview.active());
    QVERIFY(!preview.requestConfirm());

    // Tap opens the editor
    preview.start(previewTestParams(), previewTestLimits);
    QVERIFY(preview.active());
    QVERIFY(preview.editing());
    QVERIFY(!preview.confirming());
    QVERIFY(!preview.confirmed());

    // Start grid opens the slider
    QVERIFY(preview.requestConfirm());
    QVERIFY(!preview.editing());
    QVERIFY(preview.confirming());
    QVERIFY(!preview.requestConfirm());

    preview.setConfirmed();
    QVERIFY(preview.active());
    QVERIFY(!preview.confirming());
    QVERIFY(preview.confirmed());

    preview.clear();
    QVERIFY(!preview.active());
    QVERIFY(!preview.confirmed());
    // A new tap starts in the editor again
    preview.start(previewTestParams(), previewTestLimits);
    QVERIFY(preview.editing());
}

void AutoGridPreviewTest::_movedTooFar_data()
{
    QTest::addColumn<double>("distanceM");
    QTest::addColumn<bool>("tooFar");

    QTest::newRow("not moved") << 0.0 << false;
    QTest::newRow("9.9 m") << 9.9 << false;
    QTest::newRow("10.1 m") << 10.1 << true;
    QTest::newRow("20 m") << 20.0 << true;
}

void AutoGridPreviewTest::_movedTooFar()
{
    QFETCH(double, distanceM);
    QFETCH(bool, tooFar);

    AutoGridPreview preview;
    const GridGenerator::Params params = previewTestParams();
    preview.start(params, previewTestLimits);
    // Any direction counts, so not just along the heading
    QCOMPARE(preview.movedTooFar(params.origin.atDistanceAndAzimuth(distanceM, 250.0)), tooFar);
    QVERIFY(preview.movedTooFar(QGeoCoordinate()));
}

void AutoGridPreviewTest::_clear()
{
    AutoGridPreview preview;
    preview.start(previewTestParams(), previewTestLimits);
    QSignalSpy changedSpy(&preview, &AutoGridPreview::changed);

    preview.clear();
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(!preview.active());
    QVERIFY(!preview.valid());
    QVERIFY(preview.path().isEmpty());
    QVERIFY(preview.outline().isEmpty());
    QVERIFY(!preview.origin().isValid());
    QVERIFY(preview.confirmMessage().isEmpty());
    // Nothing was latched, so confirm would refuse
    QVERIFY(preview.movedTooFar(previewTestParams().origin));

    preview.adjustWidth(1);
    preview.clear();
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(!preview.active());
}

void AutoGridPreviewTest::_onlyEditorAdjusts()
{
    AutoGridPreview preview;
    preview.adjustWidth(1);
    QVERIFY(!preview.active());

    // The slider and confirm show and fly what was edited, so nothing changes after Start grid
    preview.start(previewTestParams(), previewTestLimits);
    QVERIFY(preview.requestConfirm());
    preview.adjustWidth(1);
    preview.adjustDepth(1);
    preview.adjustSpacing(1);
    comparePreviewToGenerator(preview, previewTestParams());

    preview.setConfirmed();
    preview.adjustWidth(1);
    comparePreviewToGenerator(preview, previewTestParams());
}

UT_REGISTER_TEST(AutoGridPreviewTest, TestLabel::Unit)
