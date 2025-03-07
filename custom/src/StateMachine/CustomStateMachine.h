#pragma once

#include "FunctionState.h"
#include "CustomState.h"

#include <QStateMachine>
#include <QFinalState>
#include <QString>

#include <functional>

class CustomStateMachine : public QStateMachine
{
    Q_OBJECT
public:
    CustomStateMachine(QObject* parent = nullptr);

    void setError(const QString& errorString);

    CustomState* errorState() { return _errorState; }
    QFinalState* finalState() { return _finalState; }

public slots:
    void displayError();

signals:
    void error();

private:
    void _init();

    QString         _errorString;
    CustomState*    _errorState = nullptr;
    QFinalState*    _finalState = nullptr;

    static CustomStateMachine* _instance;

    friend CustomState;
};

