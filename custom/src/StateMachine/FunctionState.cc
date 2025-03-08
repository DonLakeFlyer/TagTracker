#include "FunctionState.h"

FunctionState::FunctionState(std::function<void()> function, QState* parent)
    : GuidedModeState("FunctionState", parent)
    , _function             (function)
{
    connect(this, &QState::entered, this, [this] () { _function(); });
}
