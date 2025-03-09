#pragma once

#include <QState>
#include <QString>

class CustomStateMachine;

class CustomState : public QState
{
    Q_OBJECT

public:
    //CustomState(const QString& stateName, QState* parentState, QState* errorState = nullptr);
    CustomState(const QString& stateName, QState* parentState);

    void setError(const QString& errorString);

    CustomStateMachine* machine();
};
