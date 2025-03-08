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

private:
    void _disconnectAll();

    QString     _flightMode;
    QTimer      _timeoutTimer;
    Vehicle*    _vehicle;
};