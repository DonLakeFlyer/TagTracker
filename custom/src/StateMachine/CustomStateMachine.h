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
    void _runStopHandler();

    QString                 _errorString;
    uint                    _eventMode = 0;
    QString                 _allowedFlightMode;
    bool                    _vehicleLost = false;
    bool                    _stopping = false;  // cancelled or failed; later errors are ignored
    std::function<void()>   _stopHandler;
};
