#include "GuidedModeState.h"
#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"

#include <QSignalTransition>

GuidedModeState::GuidedModeState(const QString& stateName, QState* parentState)
    : CustomState           (stateName, parentState)
    , _vehicle              (MultiVehicleManager::instance()->activeVehicle())
    , _allowedFlightModes   ({ qobject_cast<CustomPlugin*>(CustomPlugin::instance())->holdFlightMode() })
{
#if 0
    connect(this, &QState::entered, this, [this] () {
        connect(_vehicle, &Vehicle::flightModeChanged, this, &GuidedModeState::_flightModeChanged);
    });
    connect(this, &QState::exited, this, [this] () {
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &GuidedModeState::_flightModeChanged);
    });

    auto cancelledTransition = new QSignalTransition(this, &GuidedModeState::cancelled);
    addTransition(cancelledTransition);
#endif    
}

void GuidedModeState::_flightModeChanged(const QString& flightMode)
{
    if (!_allowedFlightModes.contains(flightMode)) {
        qCDebug(CustomPluginLog) << "Flight mode changed to" << flightMode << ". Cancelling state" << " - " << Q_FUNC_INFO;
        disconnect(_vehicle, &Vehicle::flightModeChanged, this, &GuidedModeState::_flightModeChanged);
        emit cancelled();
    }
}

