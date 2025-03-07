#include "FunctionState.h"

FunctionState::FunctionState(std::function<void()> function, QState* parent)
    : CustomState   (parent)
    , _function     (function)
{
    connect(this, &QState::entered, this, [this] () { _function(); });
}
