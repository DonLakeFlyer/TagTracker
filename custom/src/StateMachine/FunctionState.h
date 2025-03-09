#pragma once

#include "CustomState.h"

#include <functional>

class FunctionState : public CustomState
{
    Q_OBJECT

public:
    FunctionState(std::function<void()>, QState* parent = nullptr);

private:
    std::function<void()> _function;
};
