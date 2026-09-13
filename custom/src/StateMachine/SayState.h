#pragma once

#include "FunctionState.h"

#include "AudioOutput.h"

#include <functional>

class SayState : public FunctionState
{
    Q_OBJECT

public:
    SayState(const QString& stateName, QState* parentState, const QString& text) :
        SayState(stateName, parentState, [text] { return text; })
    { }

    // Text is resolved when the state is entered, for announcements not known at construction
    SayState(const QString& stateName, QState* parentState, std::function<QString()> textProvider) :
        FunctionState(stateName, parentState, [textProvider] { AudioOutput::instance()->say(textProvider()); })
    { }
};
