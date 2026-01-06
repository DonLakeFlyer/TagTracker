#pragma once

#include "CustomState.h"

class StartDetectionState : public CustomState
{
    Q_OBJECT

public:
    StartDetectionState(QState* parentState);

signals:
    void tunerSucceeded();
    void checkForSelectedTagsSucceeded();

private:
    void _checkTuner();
    void _checkForSelectedTags();

    bool _channelizerTunerFailed = false;
};
