#pragma once

#include "GuidedModeState.h"

#include <functional>

class FunctionState : public GuidedModeState
{
    Q_OBJECT

public:
    FunctionState(std::function<void()>, QState* parent = nullptr);

private:
    std::function<void()> _function;
};
