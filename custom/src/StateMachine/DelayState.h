#pragma once

#include "CustomState.h"

#include <QTimer>

class DelayState : public CustomState
{
    Q_OBJECT

public:
    DelayState(QState* parentState, int delayMsecs);

signals:
    void delayComplete();

private:
    QTimer _delayTimer;
};
