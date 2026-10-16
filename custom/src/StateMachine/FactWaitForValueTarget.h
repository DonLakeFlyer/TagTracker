#pragma once

#include "CustomState.h"

#include <QTimer>

class Fact;

class FactWaitForValueTarget : public CustomState
{
    Q_OBJECT

public:
    // angleDegrees: value and target are compass degrees, compared across the 0/360 wrap
    FactWaitForValueTarget(QState* parentState, Fact* fact, double targetValue, double targetVariance, int waitMsecs, bool angleDegrees = false);

signals:
    void success();

private slots:
    void _waitTimeout();
    void _rawValueChanged(QVariant rawValue);
    void _disconnectAll();

private:
    Fact*   _fact;
    double  _targetValue;
    double  _targetVariance;
    bool    _angleDegrees;
    QTimer  _targetWaitTimer;
};
