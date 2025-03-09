#include "FunctionState.h"

FunctionState::FunctionState(std::function<void()> function, QState* parent)
    : CustomState("FunctionState", parent)
    , _function             (function)
{
    connect(this, &QState::entered, this, [this] () { _function(); });
}
