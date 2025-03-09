#include "FunctionState.h"

#include <QTimer>

FunctionState::FunctionState(const QString& stateName, QState* parentState, std::function<void()> function)
    : CustomState   (stateName, parentState)
    , _function     (function)
{
    connect(this, &QState::entered, this, [this] () {
        _function();
        emit functionCompleted();
    });
}
