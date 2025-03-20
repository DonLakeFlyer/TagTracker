#pragma once

#include "RotateAndCaptureStateBase.h"

class FullRotateAndCaptureState : public RotateAndCaptureStateBase
{
    Q_OBJECT

public:
    FullRotateAndCaptureState(QState* parentState);
};
