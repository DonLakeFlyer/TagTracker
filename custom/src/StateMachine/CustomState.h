#pragma once

#include "QGCState.h"

class CustomStateMachine;

/// QGCState carrying TagTracker's error semantics (audio annunciation, optional RTL, stop handler).
class CustomState : public QGCState
{
    Q_OBJECT

public:
    CustomState(const QString& stateName, QState* parentState);

    void setError(const QString& errorString);

    CustomStateMachine* customMachine() const;
};
