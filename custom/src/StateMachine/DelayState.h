#pragma once

#include "GuidedModeState.h"

#include <QTimer>

class DelayState : public GuidedModeState
{
    Q_OBJECT

public:
    DelayState(QState* parentState, int delayMsecs);

signals:
    void delayComplete();

private:
    QTimer _delayTimer;
};
