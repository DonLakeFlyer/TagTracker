#pragma once

#include "FunctionState.h"
#include "CustomState.h"

#include <QStateMachine>
#include <QFinalState>
#include <QString>

#include <functional>

class Vehicle;
class GuidedModeCancelledTransition;
class FunctioState;
class SayState;
class SetFlightModeState;

class CustomStateMachine : public QStateMachine
{
    Q_OBJECT
public:
    CustomStateMachine(const QString& machineName, QObject* parent = nullptr);

    void setError(const QString& errorString);

    enum {
        CancelOnFlightModeChange = 0x01,
        RTLOnError              = 0x02
    };

signals:
    void error();

public slots:
    void displayError();
    void setEventMode(uint eventMode);

private slots:
    void _flightModeChanged(const QString& flightMode);

private:
    void _init();

    Vehicle*    _vehicle = nullptr;
    QString     _errorString;
    uint        _eventMode = 0;

    friend class CustomState;
};

