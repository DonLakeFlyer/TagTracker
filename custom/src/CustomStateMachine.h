#pragma once

#include <QStateMachine>
#include <QFinalState>
#include <QString>

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

class CustomStateMachine : public QStateMachine
{
    Q_OBJECT

public:
    CustomStateMachine(QObject* parent = nullptr);

    void setErrorState(CustomState* errorState);
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

