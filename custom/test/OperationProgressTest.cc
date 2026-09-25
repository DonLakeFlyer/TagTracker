#include "OperationProgressTest.h"

#include <cstring>

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "CustomLoggingCategory.h"
#include "OperationProgress.h"
#include "TunnelProtocol.h"

using namespace TunnelProtocol;

namespace {

const QRegularExpression kStaleWarning(QStringLiteral("^OperationProgress stale"));
const QRegularExpression kStalledWarning(QStringLiteral("^OperationProgress stalled"));

OperationProgress_t makeFrame(uint32_t command, uint32_t requestId, uint32_t state, uint32_t step, uint32_t stepCount,
                              const char* message)
{
    OperationProgress_t frame{};
    frame.header.command = COMMAND_ID_OPERATION_PROGRESS;
    frame.command = command;
    frame.request_id = requestId;
    frame.state = state;
    frame.step = step;
    frame.step_count = stepCount;
    strncpy(frame.message, message, sizeof(frame.message) - 1);
    return frame;
}

}  // namespace

void OperationProgressTest::_fractionIndeterminateWhenNoStepCount()
{
    OperationProgress progress;
    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 0, 0, ""));

    QVERIFY(progress.active());
    QVERIFY(progress.running());
    QCOMPARE(progress.fraction(), -1.0);
}

void OperationProgressTest::_fractionClampsToOne()
{
    OperationProgress progress;
    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 1, 4, "a"));
    QCOMPARE(progress.fraction(), 0.25);

    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 9, 4, "b"));
    QCOMPARE(progress.fraction(), 1.0);
}

void OperationProgressTest::_titleFromCommand()
{
    OperationProgress progress;
    progress.handleFrame(makeFrame(COMMAND_ID_CLEAN_LOGS, 3, OPERATION_STATE_RUNNING, 0, 0, "Deleting logs"));

    QCOMPARE(progress.title(), OperationProgress::titleForCommand(COMMAND_ID_CLEAN_LOGS));
    QCOMPARE(progress.message(), QStringLiteral("Deleting logs"));
    QCOMPARE(OperationProgress::titleForCommand(9999), QStringLiteral("Controller busy"));
}

void OperationProgressTest::_titleFromMessageForControllerInitiated()
{
    OperationProgress progress;
    progress.handleFrame(
        makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, "Post-flight analysis"));
    QCOMPARE(progress.title(), QStringLiteral("Post-flight analysis"));

    // Empty message falls back to the command title
    OperationProgress fallback;
    fallback.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, ""));
    QCOMPARE(fallback.title(), OperationProgress::titleForCommand(COMMAND_ID_STOP_DETECTION));
}

void OperationProgressTest::_titleKeptAcrossRunningUpdates()
{
    OperationProgress progress;
    QSignalSpy spy(&progress, &OperationProgress::changed);

    progress.handleFrame(
        makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, "Post-flight analysis"));
    progress.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 1, 3, "Analyzing tag 2"));

    QCOMPARE(progress.title(), QStringLiteral("Post-flight analysis"));
    QCOMPARE(progress.message(), QStringLiteral("Analyzing tag 2"));
    QCOMPARE(progress.fraction(), 1.0 / 3.0);
    QCOMPARE(spy.count(), 2);
}

void OperationProgressTest::_completeAndFailedStates()
{
    OperationProgress progress;
    progress.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 5, OPERATION_STATE_RUNNING, 0, 10, "Capturing"));
    progress.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 5, OPERATION_STATE_COMPLETE, 10, 10, "Capture complete"));

    QVERIFY(progress.active());
    QVERIFY(!progress.running());
    QVERIFY(!progress.failed());
    QCOMPARE(progress.title(), OperationProgress::titleForCommand(COMMAND_ID_RAW_CAPTURE));
    QCOMPARE(progress.message(), QStringLiteral("Capture complete"));

    OperationProgress failed;
    failed.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 6, OPERATION_STATE_RUNNING, 0, 10, "Capturing"));
    failed.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 6, OPERATION_STATE_FAILED, 3, 10, "Capture failed"));

    QVERIFY(failed.active());
    QVERIFY(!failed.running());
    QVERIFY(failed.failed());
}

void OperationProgressTest::_sameIdsAfterCompleteStartNewOperation()
{
    OperationProgress progress;
    progress.handleFrame(
        makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, "Post-flight analysis"));
    progress.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_COMPLETE, 0, 0, "Analysis complete"));

    // Controller-initiated work always uses request_id 0, so a RUNNING frame
    // after COMPLETE is a new operation and must take its title afresh
    progress.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, "Second analysis"));

    QVERIFY(progress.running());
    QCOMPARE(progress.title(), QStringLiteral("Second analysis"));
}

void OperationProgressTest::_duplicateTerminalFrameIsIgnored()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(50, 300);
    QSignalSpy spy(&progress, &OperationProgress::changed);

    progress.handleFrame(
        makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_RUNNING, 0, 0, "Post-flight analysis"));
    progress.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_COMPLETE, 0, 0, "Analysis complete"));
    QCOMPARE(spy.count(), 2);

    // The controller re-sends the terminal frame for a few heartbeat ticks
    QVERIFY(!spy.wait(200));
    progress.handleFrame(makeFrame(COMMAND_ID_STOP_DETECTION, 0, OPERATION_STATE_COMPLETE, 0, 0, "Analysis complete"));

    QCOMPARE(spy.count(), 2);
    QCOMPARE(progress.title(), QStringLiteral("Post-flight analysis"));
    QVERIFY(progress.active());

    // Linger was not restarted by the duplicate: hides ~100 ms later, not ~300 ms
    QTRY_VERIFY_WITH_TIMEOUT(!progress.active(), 200);
}

void OperationProgressTest::_resetHidesOnce()
{
    OperationProgress progress;
    QSignalSpy spy(&progress, &OperationProgress::changed);

    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 0, 0, ""));
    QCOMPARE(spy.count(), 1);

    // Hiding a RUNNING operation is reported as stale
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStaleWarning);
    progress.reset();
    verifyExpectedLogMessage();
    QVERIFY(!progress.active());
    QVERIFY(!progress.running());
    QCOMPARE(spy.count(), 2);

    progress.reset();
    QCOMPARE(spy.count(), 2);
}

void OperationProgressTest::_staleRunningHides()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(50, 50);

    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 0, 0, ""));
    QVERIFY(progress.active());

    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStaleWarning);
    QTRY_VERIFY_WITH_TIMEOUT(!progress.active(), 2000);
    verifyExpectedLogMessage();
}

void OperationProgressTest::_completeLingersThenHides()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(50, 300);

    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_RUNNING, 0, 0, ""));
    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 7, OPERATION_STATE_COMPLETE, 0, 0, "Logs saved"));

    // Linger outlives the stale timeout: still visible after the stale interval
    QSignalSpy spy(&progress, &OperationProgress::changed);
    QVERIFY(!spy.wait(100));
    QVERIFY(progress.active());

    QTRY_VERIFY_WITH_TIMEOUT(!progress.active(), 2000);
}

void OperationProgressTest::_rotationTitle()
{
    OperationProgress progress;
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 0, 40, "Rotation"));
    QCOMPARE(progress.title(), QStringLiteral("Rotation"));
    QCOMPARE(progress.command(), static_cast<uint32_t>(COMMAND_ID_START_COLLECTION));
}

void OperationProgressTest::_stalledWhenStepFrozen()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(5000, 5000, 500);
    QSignalSpy stalled(&progress, &OperationProgress::stalled);

    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 5, 40, "collecting 1/10 s"));
    // Same step re-sent at 1 Hz keeps the card alive but does not reset the stall clock
    QVERIFY(!stalled.wait(50));
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 5, 40, "collecting 1/10 s"));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStalledWarning);
    QTRY_COMPARE_WITH_TIMEOUT(stalled.count(), 1, 2000);
    verifyExpectedLogMessage();
    QCOMPARE(stalled.first().at(0).toUInt(), static_cast<uint32_t>(COMMAND_ID_START_COLLECTION));
    QCOMPARE(stalled.first().at(1).toString(), QStringLiteral("collecting 1/10 s"));
    QVERIFY(progress.active());   // stall is advisory; the card stays until stale/hidden

    // An advancing step never stalls
    OperationProgress moving;
    moving.setTimeoutsForTest(5000, 5000, 500);
    QSignalSpy movingStalled(&moving, &OperationProgress::stalled);
    for (uint32_t step = 0; step < 5; ++step) {
        moving.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, step, 40, "x"));
        QVERIFY(!movingStalled.wait(50));
    }
    QCOMPARE(movingStalled.count(), 0);
}

void OperationProgressTest::_stallOnlyForRotation()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(5000, 5000, 200);
    QSignalSpy stalled(&progress, &OperationProgress::stalled);

    // A frozen capture step is normal and must not stall
    progress.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 4, OPERATION_STATE_RUNNING, 1, 3, "Capturing"));
    QVERIFY(!stalled.wait(400));
    progress.restartStallWatch();
    QVERIFY(!stalled.wait(400));
    QCOMPARE(stalled.count(), 0);

    // A rotation's armed stall clock is dropped when another operation takes over
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 5, OPERATION_STATE_RUNNING, 0, 40, "Rotation"));
    progress.handleFrame(makeFrame(COMMAND_ID_SAVE_LOGS, 6, OPERATION_STATE_RUNNING, 0, 1, "Saving"));
    QVERIFY(!stalled.wait(400));
    QCOMPARE(stalled.count(), 0);

    // A non-rotation whose frames stop is hidden as stale but is not a stall
    OperationProgress silent;
    silent.setTimeoutsForTest(50, 300, 5000);
    QSignalSpy silentStalled(&silent, &OperationProgress::stalled);
    silent.handleFrame(makeFrame(COMMAND_ID_RAW_CAPTURE, 7, OPERATION_STATE_RUNNING, 1, 3, "Capturing"));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStaleWarning);
    QTRY_VERIFY_WITH_TIMEOUT(!silent.active(), 2000);
    verifyExpectedLogMessage();
    QCOMPARE(silentStalled.count(), 0);
}

void OperationProgressTest::_stepCountGrowthIsNotProgress()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(5000, 5000, 500);
    QSignalSpy stalled(&progress, &OperationProgress::stalled);

    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 20, 40, "Slice 8/8 complete"));
    QVERIFY(!stalled.wait(50));
    // Revisit: step_count grows, step parked
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 20, 52, "Revisit requested"));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStalledWarning);
    QTRY_COMPARE_WITH_TIMEOUT(stalled.count(), 1, 2000);
    verifyExpectedLogMessage();
}

void OperationProgressTest::_restartStallWatchDefersStall()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(5000, 5000, 500);
    QSignalSpy stalled(&progress, &OperationProgress::stalled);

    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 20, 40, "Slice 3/8 complete"));
    QVERIFY(!stalled.wait(300));
    progress.restartStallWatch();   // GCS finished yawing, now waiting on the controller again
    // Past the original 500 ms deadline, short of the restarted one
    QVERIFY(!stalled.wait(300));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStalledWarning);
    QTRY_COMPARE_WITH_TIMEOUT(stalled.count(), 1, 2000);
    verifyExpectedLogMessage();

    // Not running: no effect
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_COMPLETE, 40, 40, "done"));
    progress.restartStallWatch();
    QVERIFY(!QSignalSpy(&progress, &OperationProgress::stalled).wait(200));
}

void OperationProgressTest::_finishedSignalAndStaleStalls()
{
    OperationProgress progress;
    progress.setTimeoutsForTest(50, 300, 5000);
    QSignalSpy finished(&progress, &OperationProgress::finished);
    QSignalSpy stalled(&progress, &OperationProgress::stalled);

    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 0, 40, "Rotation"));
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_FAILED, 7, 40, "Detector 2 exited"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first().at(1).toBool(), false);
    QCOMPARE(finished.first().at(2).toString(), QStringLiteral("Detector 2 exited"));
    // Terminal re-send does not re-emit
    progress.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_FAILED, 7, 40, "Detector 2 exited"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(stalled.count(), 0);

    // Frames stopping altogether while RUNNING counts as a stall
    OperationProgress silent;
    silent.setTimeoutsForTest(50, 300, 5000);
    QSignalSpy silentStalled(&silent, &OperationProgress::stalled);
    silent.handleFrame(makeFrame(COMMAND_ID_START_COLLECTION, 3, OPERATION_STATE_RUNNING, 4, 40, "armed"));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg, kStaleWarning);
    QTRY_COMPARE_WITH_TIMEOUT(silentStalled.count(), 1, 2000);
    verifyExpectedLogMessage();
    QVERIFY(!silent.active());
}

UT_REGISTER_TEST(OperationProgressTest, TestLabel::Unit)
