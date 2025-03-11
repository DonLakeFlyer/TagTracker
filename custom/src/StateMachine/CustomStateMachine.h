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

class CustomStateMachine : public QStateMachine
{
    Q_OBJECT
public:
    CustomStateMachine(const QString& machineName, QObject* parent = nullptr);

    void setError(const QString& errorString);

    void addGuidedModeCancelledTransition();
    void removeGuidedModeCancelledTransition();

public slots:
    void displayError();

private slots:
    void _flightModeChanged(const QString& flightMode);

private:
    void _init();

    Vehicle*        _vehicle = nullptr;
    QString         _errorString;
    FunctionState*  _errorState = nullptr;
    QFinalState*    _finalState = nullptr;
    GuidedModeCancelledTransition* _guidedModeCancelledTransition = nullptr;
    SayState*       _announceCancelState = nullptr;

    friend class CustomState;
};

