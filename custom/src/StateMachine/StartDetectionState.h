#pragma once

#include "CustomState.h"

class StartDetectionState : public CustomState
{
    Q_OBJECT

public:
    StartDetectionState(QState* parentState);

signals:
    void tunerSucceeded();

private:
    void _checkTuner();
    
    bool _channelizerTunerFailed = false;
};
