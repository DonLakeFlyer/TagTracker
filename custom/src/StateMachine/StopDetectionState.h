#pragma once

#include "CustomState.h"

class StopDetectionState : public CustomState
{
    Q_OBJECT

public:
    StopDetectionState(QState* parentState);

private:
    void _clearDetectorList();
};
