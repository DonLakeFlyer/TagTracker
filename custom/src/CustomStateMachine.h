#pragma once

#include <QStateMachine>
#include <QFinalState>
#include <QString>

#include <functional>

class CustomState : public QState
{
    Q_OBJECT

public:
    CustomState(QState* parent = nullptr) :
        QState(parent)
    {
    }

    void setError(const QString& errorString);

    QString errorString() const { return _errorString; }

signals:
    void error();

private:
    QString _errorString;
};

class FunctionState : public CustomState
{
    Q_OBJECT

public:
    FunctionState(std::function<void()>, QState* parent = nullptr);

private:
    std::function<void()> _function;
};

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

