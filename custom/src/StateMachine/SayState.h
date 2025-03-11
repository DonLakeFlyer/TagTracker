#pragma once

#include "FunctionState.h"

#include "AudioOutput.h"

class SayState : public FunctionState
{
    Q_OBJECT

public:
    SayState(const QString& stateName, QState* parentState, const QString& text) :
        FunctionState(stateName, parentState, std::bind(&SayState::_say, this, text))
    { }

private:
    void _say(const QString text) { AudioOutput::instance()->say(text); }
};