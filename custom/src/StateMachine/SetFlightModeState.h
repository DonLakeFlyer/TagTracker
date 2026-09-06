#pragma once

#include "CustomState.h"

#include <QTimer>

class Vehicle;

class SetFlightModeState : public CustomState
{
    Q_OBJECT

public:
    SetFlightModeState(QState* parentState, const QString& flightMode);

    // Mode acks arrive well after 2 s over a lossy link or a slow SITL clock.
    static constexpr int kTimeoutMsecs = 10000;

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
