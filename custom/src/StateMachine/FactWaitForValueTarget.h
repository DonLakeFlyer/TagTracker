#pragma once

#include "CustomState.h"

#include <QTimer>

class Fact;

class FactWaitForValueTarget : public CustomState
{
    Q_OBJECT

public:
    FactWaitForValueTarget(QState* parentState, Fact* fact, double targetValue, double targetVariance, int waitMsecs);

signals:
    void success();

private slots:
    void _waitTimeout();
    void _rawValueChanged(QVariant rawValue);

private:
    Fact*   _fact;
    double  _targetValue;
    double  _targetVariance;
    QTimer  _targetWaitTimer;
};
