#include "SendMavlinkCommandState.h"
#include "CustomLoggingCategory.h"
#include "TunnelProtocol.h"

#include "MultiVehicleManager.h"
#include "MissionCommandTree.h"
#include "Vehicle.h"

SendMavlinkCommandState::SendMavlinkCommandState(QState* parent, MAV_CMD command, double param1, double param2, double param3, double param4, double param5, double param6, double param7)
    : FunctionState (std::bind(&SendMavlinkCommandState::_sendMavlinkCommand, this), parent)
    , _vehicle      (MultiVehicleManager::instance()->activeVehicle())
    , _command      (command)
    , _param1       (param1)
    , _param2       (param2)
    , _param3       (param3)
    , _param4       (param4)
    , _param5       (param5)
    , _param6       (param6)
    , _param7       (param7)
{
    connect(this, &QState::entered, this, [this] () 
        { 
            qCDebug(CustomPluginLog) << QStringLiteral("Sending %1 command").arg(MissionCommandTree::instance()->friendlyName(_command)) << " - " << Q_FUNC_INFO;   
        });
}

void SendMavlinkCommandState::_sendMavlinkCommand()
{
    connect(_vehicle, &Vehicle::mavCommandResult, this, &SendMavlinkCommandState::_mavCommandResult);
    _vehicle->sendMavCommand(MAV_COMP_ID_AUTOPILOT1,
                                _command,
                                false /* showError */,
                                static_cast<float>(_param1),
                                static_cast<float>(_param2),
                                static_cast<float>(_param3),
                                static_cast<float>(_param4),
                                static_cast<float>(_param5),
                                static_cast<float>(_param6),
                                static_cast<float>(_param7));
}

void SendMavlinkCommandState::_mavCommandResult(int vehicleId, int component, int command, int result, bool noResponseFromVehicle)
{
    Q_UNUSED(vehicleId);
    Q_UNUSED(component);

    Vehicle* vehicle = dynamic_cast<Vehicle*>(sender());
    if (!vehicle) {
        qWarning() << "Vehicle dynamic cast on sender() failed!" << " - " << Q_FUNC_INFO;
        return;
    }
    if (vehicle != _vehicle) {
        qCWarning(CustomPluginLog) << "Received mavCommandResult from unexpected vehicle" << " - " << Q_FUNC_INFO;
        return;
    }
    if (command != _command) {
        qCWarning(CustomPluginLog) << "Received mavCommandResult for unexpected command - expected:actual" << _command << command << " - " << Q_FUNC_INFO;
        return;
    }

    disconnect(_vehicle, &Vehicle::mavCommandResult, this, &SendMavlinkCommandState::_mavCommandResult);

    QString commandName = MissionCommandTree::instance()->friendlyName(_command);

    if (noResponseFromVehicle) {
        qCDebug(CustomPluginLog) << QStringLiteral("%1 Command - No response from vehicle").arg(commandName) << " - " << Q_FUNC_INFO;
        QString message = QStringLiteral("%1 command failed").arg(commandName);
        setError(message);
    } else if (result != MAV_RESULT_ACCEPTED) {
        qCWarning(CustomPluginLog) << QStringLiteral("%1 Command failed = ack.result: %2").arg(commandName).arg(result) << " - " << Q_FUNC_INFO;
        QString message = QStringLiteral("%1 command failed").arg(commandName);
        setError(message);
    } else {
        // MAV_RESULT_ACCEPTED
        qCDebug(CustomPluginLog) << QStringLiteral("%1 Command succeeded").arg(commandName) << " - " << Q_FUNC_INFO;
        emit success();
    }
}

