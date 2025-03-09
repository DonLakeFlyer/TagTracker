#pragma once

#include "CustomState.h"

#include <functional>

class FunctionState : public CustomState
{
    Q_OBJECT

public:
    FunctionState(const QString& stateName, QState* parentState, std::function<void()>);

signals:
    void functionCompleted();

private:
    std::function<void()> _function;
};
