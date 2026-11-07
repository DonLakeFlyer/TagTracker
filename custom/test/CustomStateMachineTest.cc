#include "CustomStateMachineTest.h"

#include <cstring>

#include <QtCore/QPointer>
#include <QtCore/QScopeGuard>
#include <QtCore/QScopedPointer>
#include <QtStateMachine/QFinalState>
#include <QtStateMachine/QState>
#include <QtTest/QSignalSpy>

#include "CustomLoggingCategory.h"
#include "CustomPlugin.h"
#include "CustomStateMachine.h"
#include "Fact.h"
#include "FactWaitForValueTarget.h"
#include "HeadingSettleState.h"
#include "OperationProgress.h"
#include "PythonWaitForDetectionResultState.h"
#include "TunnelProtocol.h"
#include "Vehicle.h"

void CustomStateMachineTest::_flightModeChangeAfterFinishDoesNotCancel()
{
    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange);

    bool stopHandlerRan = false;
    machine->registerStopHandler([&stopHandlerRan]() { stopHandlerRan = true; });

    auto initialState = new QState(machine.data());
    auto finalState = new QFinalState(machine.data());
    initialState->addTransition(finalState);
    machine->setInitialState(initialState);

    QSignalSpy finishedSpy(machine.data(), &QStateMachine::finished);
    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty(), 5000);

    emit _vehicle->flightModeChanged(QStringLiteral("Return"));

    QVERIFY(!stopHandlerRan);
}

void CustomStateMachineTest::_flightModeChangeWhileRunningCancels()
{
    // QPointer, not QScopedPointer: a cancelled machine deletes itself
    QPointer<CustomStateMachine> machine = new CustomStateMachine(QStringLiteral("Test"), nullptr);
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange);

    bool stopHandlerRan = false;
    machine->registerStopHandler([&stopHandlerRan]() { stopHandlerRan = true; });

    // Never leaves the initial state, so the machine stays running
    auto initialState = new QState(machine.data());
    machine->setInitialState(initialState);

    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(machine->inProgress(), 5000);

    emit _vehicle->flightModeChanged(QStringLiteral("Return"));

    QVERIFY(stopHandlerRan);
    QTRY_VERIFY_WITH_TIMEOUT(machine.isNull(), 5000);
}

void CustomStateMachineTest::_allowedFlightModeDoesNotCancel()
{
    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange);
    machine->setAllowedFlightMode(QStringLiteral("Orbit"));

    bool stopHandlerRan = false;
    machine->registerStopHandler([&stopHandlerRan]() { stopHandlerRan = true; });

    auto initialState = new QState(machine.data());
    machine->setInitialState(initialState);

    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(machine->inProgress(), 5000);

    emit _vehicle->flightModeChanged(QStringLiteral("Orbit"));
    QVERIFY(!stopHandlerRan);

    emit _vehicle->flightModeChanged(QStringLiteral("Return"));
    QVERIFY(stopHandlerRan);
}

void CustomStateMachineTest::_errorAfterCancelIsIgnored()
{
    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange | CustomStateMachine::RTLOnError);

    auto initialState = new QState(machine.data());
    machine->setInitialState(initialState);

    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(machine->inProgress(), 5000);

    emit _vehicle->flightModeChanged(QStringLiteral("Return"));

    // A state timer firing before the async stop lands: no error dialog, no RTL
    const QString flightMode = _vehicle->flightMode();
    machine->setError(QStringLiteral("late error"));
    QCOMPARE(_vehicle->flightMode(), flightMode);
}

void CustomStateMachineTest::_cancelWhileRunningStopsWithoutRTL()
{
    // QPointer, not QScopedPointer: a cancelled machine deletes itself
    QPointer<CustomStateMachine> machine = new CustomStateMachine(QStringLiteral("Test"), nullptr);
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange | CustomStateMachine::RTLOnError);

    int stopHandlerCount = 0;
    machine->registerStopHandler([&stopHandlerCount]() { stopHandlerCount++; });

    auto initialState = new QState(machine.data());
    machine->setInitialState(initialState);

    // inProgress() is set before Qt processes the start, so wait for the state itself
    QSignalSpy enteredSpy(initialState, &QState::entered);
    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(!enteredSpy.isEmpty(), 5000);

    const QString flightMode = _vehicle->flightMode();
    machine->cancel();
    machine->cancel();
    QCOMPARE(stopHandlerCount, 1);

    // A state timer firing before the async stop lands: no RTL
    machine->setError(QStringLiteral("late error"));
    QCOMPARE(_vehicle->flightMode(), flightMode);

    // The vehicle's mode change from the pause must not cancel a second time
    emit _vehicle->flightModeChanged(QStringLiteral("Hold"));
    QCOMPARE(stopHandlerCount, 1);
    QTRY_VERIFY_WITH_TIMEOUT(machine.isNull(), 5000);
}

void CustomStateMachineTest::_stallAfterTakeoffReturns()
{
    // An Auto Detection rotation that stalls mid-slice after takeoff must return
    // Going airborne samples the GCS pressure sensor, which test hosts do not have
    ignoreLogMessage("Utilities.QGCSensors", QtWarningMsg, QRegularExpression(QStringLiteral("[Pp]ressure")));
    _vehicle->sendMavCommand(_vehicle->defaultComponentId(), MAV_CMD_NAV_TAKEOFF, true, 0, 0, 0, 0, 0, 0,
                             50.0f /* altitude */);
    QTRY_VERIFY_WITH_TIMEOUT(_vehicle->airborne(), 10000);

    auto* plugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    QVERIFY(plugin);
    OperationProgress* progress = plugin->operationProgress();
    progress->setTimeoutsForTest(5000, 5000, 300);
    OperationProgress_t frame{};
    frame.header.command = COMMAND_ID_OPERATION_PROGRESS;
    frame.command = COMMAND_ID_START_COLLECTION;
    frame.request_id = 3;
    frame.state = OPERATION_STATE_RUNNING;
    frame.step = 5;
    frame.step_count = 40;
    strncpy(frame.message, "collecting 6/8", sizeof(frame.message) - 1);
    progress->handleFrame(frame);
    // The plugin outlives this test, so restore it even when a check returns early
    const auto restoreProgress = qScopeGuard([progress, &frame]() {
        frame.state = OPERATION_STATE_FAILED;
        progress->handleFrame(frame);
        progress->reset();
        progress->setTimeoutsForTest(5000, 30000, OperationProgress::kDefaultStallMSecs);
    });

    // QPointer, not QScopedPointer: a failed machine deletes itself
    QPointer<CustomStateMachine> machine = new CustomStateMachine(QStringLiteral("Auto Detection"), nullptr);
    // As set by Auto Detection once takeoff completes
    machine->setEventMode(CustomStateMachine::CancelOnFlightModeChange | CustomStateMachine::RTLOnError);
    int stopHandlerCount = 0;
    machine->registerStopHandler([&stopHandlerCount]() { stopHandlerCount++; });
    machine->setInitialState(new PythonWaitForDetectionResultState(machine.data(), 1, 6));

    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg,
                     QRegularExpression(QStringLiteral("^OperationProgress stalled")));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg,
                     QRegularExpression(QStringLiteral("^machine:.*error:.*Rotation stalled at slice 6")));
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg,
                     QRegularExpression(QStringLiteral("^error:.*Rotation stalled at slice 6")));
    expectAppMessage(QRegularExpression(QStringLiteral("Rotation stalled at slice 6")));
    machine->start();

    QTRY_COMPARE_WITH_TIMEOUT(_vehicle->flightMode(), _vehicle->rtlFlightMode(), 10000);
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();
    QCOMPARE(stopHandlerCount, 1);
    QTRY_VERIFY_WITH_TIMEOUT(machine.isNull(), 5000);
}

void CustomStateMachineTest::_headingTargetWrapsAtNorth()
{
    Fact heading(0, QStringLiteral("heading"), FactMetaData::valueTypeDouble);
    heading.setRawValue(10.0);

    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    auto waitState = new FactWaitForValueTarget(machine.data(), &heading, 0.0, 1.0, 5000, true /* angleDegrees */);
    auto finalState = new QFinalState(machine.data());
    waitState->addTransition(waitState, &FactWaitForValueTarget::success, finalState);
    machine->setInitialState(waitState);

    QSignalSpy finishedSpy(machine.data(), &QStateMachine::finished);
    QSignalSpy enteredSpy(waitState, &QState::entered);
    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(!enteredSpy.isEmpty(), 5000);

    heading.setRawValue(359.6);
    QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty(), 1000);
}

void CustomStateMachineTest::_headingSettleWaitsOutOvershoot()
{
    Fact heading(0, QStringLiteral("heading"), FactMetaData::valueTypeDouble);
    heading.setRawValue(359.5);

    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    auto settleState = new HeadingSettleState(machine.data(), &heading, 0.0, 3.0, 1000 /* hold */, 5000 /* max wait */);
    auto finalState = new QFinalState(machine.data());
    settleState->addTransition(settleState, &HeadingSettleState::settled, finalState);
    machine->setInitialState(settleState);

    QSignalSpy settledSpy(settleState, &HeadingSettleState::settled);
    QSignalSpy enteredSpy(settleState, &QState::entered);
    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(!enteredSpy.isEmpty(), 5000);

    // Overshoot out of the band restarts the hold
    QVERIFY(!settledSpy.wait(400));
    heading.setRawValue(7.0);
    QVERIFY(!settledSpy.wait(400));
    // An invalid heading is not in band, however long it lasts
    heading.setRawValue(qQNaN());
    QVERIFY(!settledSpy.wait(1200));
    heading.setRawValue(2.0);
    QVERIFY(!settledSpy.wait(400));
    QTRY_COMPARE_WITH_TIMEOUT(settledSpy.count(), 1, 1500);
}

void CustomStateMachineTest::_headingSettleGivesUpInGusts()
{
    Fact heading(0, QStringLiteral("heading"), FactMetaData::valueTypeDouble);
    heading.setRawValue(5.0);

    QScopedPointer<CustomStateMachine> machine(new CustomStateMachine(QStringLiteral("Test"), nullptr));
    bool stopHandlerRan = false;
    machine->registerStopHandler([&stopHandlerRan]() { stopHandlerRan = true; });
    auto settleState = new HeadingSettleState(machine.data(), &heading, 0.0, 3.0, 300 /* hold */, 600 /* max wait */);
    auto finalState = new QFinalState(machine.data());
    settleState->addTransition(settleState, &HeadingSettleState::settled, finalState);
    machine->setInitialState(settleState);

    // Never settling still starts the slice; it must not fail the rotation
    QSignalSpy finishedSpy(machine.data(), &QStateMachine::finished);
    expectLogMessage(CustomPluginLog().categoryName(), QtWarningMsg,
                     QRegularExpression(QStringLiteral("^Heading did not settle")));
    machine->start();
    QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty(), 5000);
    verifyExpectedLogMessage();
    QVERIFY(!stopHandlerRan);
}

UT_REGISTER_TEST(CustomStateMachineTest, TestLabel::Unit)
