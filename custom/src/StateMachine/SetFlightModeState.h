#pragma once

#include "CustomState.h"

#include <QTimer>

#include <functional>

class Vehicle;

class SetFlightModeState : public CustomState
{
    Q_OBJECT

public:
    SetFlightModeState(QState* parentState, const QString& flightMode);

    // Mode acks arrive well after 2 s over a lossy link or a slow SITL clock.
    static constexpr int kTimeoutMsecs = 10000;

    /// Re-sends the request every retryMsecs until the mode is reached, up to `attempts` requests in total. The
    /// kTimeoutMsecs limit still applies from the first request. Off by default.
    void setRetry(int retryMsecs, int attempts);
    /// Replaces Vehicle::setFlightMode for sending the request (tests stand in for a vehicle that ignores it).
    void setModeRequest(std::function<void(const QString&)> modeRequest) { _modeRequest = std::move(modeRequest); }

signals:
    void flightModeChanged();

private slots:
    void _setFlightMode();
    void _retry();
    void _timeout();
    void _validateFlightModeChange(const QString& flightMode);
    void _disconnectAll();

private:
    void _requestFlightMode();

    QString     _flightMode;
    QTimer      _timeoutTimer;
    QTimer      _retryTimer;
    int         _maxAttempts    = 1;
    int         _attempts       = 0;
    Vehicle*    _vehicle;
    std::function<void(const QString&)> _modeRequest;
};
