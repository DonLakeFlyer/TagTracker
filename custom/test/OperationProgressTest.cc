#include "OperationProgressTest.h"

#include <cstring>

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>

#include "CustomLoggingCategory.h"
#include "OperationProgress.h"
#include "TunnelProtocol.h"

using namespace TunnelProtocol;

namespace {

const QRegularExpression kStaleWarning(QStringLiteral("^OperationProgress stale"));

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

UT_REGISTER_TEST(OperationProgressTest, TestLabel::Unit)
