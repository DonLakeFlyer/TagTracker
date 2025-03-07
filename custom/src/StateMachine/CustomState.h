#pragma once

#include <QState>
#include <QString>

class CustomStateMachine;

class CustomState : public QState
{
    Q_OBJECT

public:
    CustomState(QState* parent) :
        QState(QState::ExclusiveStates, parent)
    {
    }

    void setError(const QString& errorString);

    CustomStateMachine* machine();
    QString errorString() const { return _errorString; }

signals:
    void error();

private:
    QString _errorString;
};
