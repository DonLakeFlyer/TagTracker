#pragma once

#include "CustomState.h"

#include <QTimer>

class Vehicle;

class SetFlightModeState : public CustomState
{
    Q_OBJECT

public:
    SetFlightModeState(QState* parentState, const QString& flightMode);

signals:
    void flightModeChanged();

private slots:
    void _setFlightMode();
    void _timeout();
    void _validateFlightModeChange(const QString& flightMode);
    void _disconnectAll();

private:
    QString     _flightMode;
    QTimer      _timeoutTimer;
    Vehicle*    _vehicle;
};