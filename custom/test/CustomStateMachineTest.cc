#include "CustomStateMachineTest.h"

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtStateMachine/QFinalState>
#include <QtStateMachine/QState>
#include <QtTest/QSignalSpy>

#include "CustomStateMachine.h"
#include "Fact.h"
#include "FactWaitForValueTarget.h"
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

UT_REGISTER_TEST(CustomStateMachineTest, TestLabel::Unit)
