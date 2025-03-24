#pragma once

#include "SendMavlinkCommandState.h"

class RotateMavlinkCommandState : public SendMavlinkCommandState
{
    Q_OBJECT

public:
    RotateMavlinkCommandState(QState* parentState, double heading);
};
