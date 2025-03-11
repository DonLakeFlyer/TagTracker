#pragma once

#include "CustomState.h"

#include <QTimer>

class Vehicle;

class TakeoffState : public CustomState
{
    Q_OBJECT

public:
    TakeoffState(QState* parentState, double takeoffAltRel);

signals:
    void takeoffComplete();

private slots:
    void _onEntry();
    void _flightModeChanged(const QString& flightMode);
    void _takeoffTimeout();
    void _settleTimeout();
    void _disconnectAll();

private:
    Vehicle*    _vehicle;
    double      _takeoffAltRel;
    QTimer      _timeoutTimer;
    QTimer      _settleTimer;
    bool        _takeoffDetected = false;
    QString     _guidedFlightMode;
    QString     _takeoffFlightMode;
};
