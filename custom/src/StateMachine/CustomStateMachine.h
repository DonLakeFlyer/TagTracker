#pragma once

#include "QGCStateMachine.h"

#include <QtCore/QString>

#include <functional>

class Vehicle;

class CustomStateMachine : public QGCStateMachine
{
    Q_OBJECT

public:
    CustomStateMachine(const QString& machineName, QObject* parent = nullptr);

    void setError(const QString& errorString);
    void registerStopHandler(std::function<void()> handler) { _stopHandler = handler; }
    /// A mode the machine itself puts the vehicle in, so entering it is not a user cancel.
    void setAllowedFlightMode(const QString& flightMode) { _allowedFlightMode = flightMode; }
    /// Replaces the Hold/allowed-mode rule while CancelOnFlightModeChange is set: the handler sees every mode change
    /// and decides itself whether to cancel().
    void setFlightModeChangeHandler(std::function<void(const QString&)> handler)
    {
        _flightModeChangeHandler = std::move(handler);
    }
    /// Stops a running machine as a user cancel: no RTL, the vehicle is left as it is.
    void cancel();
    /// Cancelled or failed. stop() is asynchronous, so a state timer can still fire after this; it must send nothing.
    bool stopping() const { return _stopping; }

    enum {
        CancelOnFlightModeChange = 0x01,
        RTLOnError              = 0x02
    };

public slots:
    void displayError();
    void setEventMode(uint eventMode);

private slots:
    void _flightModeChanged(const QString& flightMode);
    void _vehicleRemoved(Vehicle* vehicle);

private:
    void _cancelForUser();
    void _runStopHandler();

    QString                 _errorString;
    uint                    _eventMode = 0;
    QString                 _allowedFlightMode;
    bool                    _vehicleLost = false;
    bool                    _stopping = false;  // cancelled or failed; later errors are ignored
    std::function<void()>   _stopHandler;
    std::function<void(const QString&)> _flightModeChangeHandler;
};
