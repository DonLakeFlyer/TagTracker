#pragma once

#include "CustomState.h"

#include "QGCMAVLink.h"
#include "CustomStateMachine.h"

class Vehicle;

class SendMavlinkCommandState : public CustomState
{
    Q_OBJECT

public:
    SendMavlinkCommandState(QState* parent, MAV_CMD command, double param1 = 0.0, double param2 = 0.0, double param3 = 0.0, double param4 = 0.0, double param5 = 0.0, double param6 = 0.0, double param7 = 0.0);
    SendMavlinkCommandState(QState* parent);

    void setup(MAV_CMD command, double param1 = 0.0, double param2 = 0.0, double param3 = 0.0, double param4 = 0.0, double param5 = 0.0, double param6 = 0.0, double param7 = 0.0);
    
signals:
    void success();

private slots:
    void _mavCommandResult(int vehicleId, int component, int command, int result, int failureCode);
    void _disconnectAll();

private:
    void _sendMavlinkCommand();

    Vehicle*    _vehicle = nullptr;
    MAV_CMD     _command;
    double      _param1 = 0.0;
    double      _param2 = 0.0;
    double      _param3 = 0.0;
    double      _param4 = 0.0;
    double      _param5 = 0.0;
    double      _param6 = 0.0;
    double      _param7 = 0.0;
};
