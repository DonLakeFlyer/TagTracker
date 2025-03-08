#pragma once

#include <QState>
#include <QString>

class CustomStateMachine;

class CustomState : public QState
{
    Q_OBJECT

public:
    CustomState(const QString& stateName, QState* parentState, QState* errorState = nullptr);

    void setError(const QString& errorString);

    CustomStateMachine* machine();
    QString errorString() const { return _errorString; }

signals:
    void error();
    void cancelled();

private:
    QString _errorString;
};
