#include "RotateMavlinkCommandState.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"

RotateMavlinkCommandState::RotateMavlinkCommandState(QState* parentState, double heading)
    : SendMavlinkCommandState(parentState)
{
    auto vehicle = MultiVehicleManager::instance()->activeVehicle();
    if (vehicle->px4Firmware()) {
        setup(
            MAV_CMD_DO_REPOSITION,
            -1,                                     // no change in ground speed
            MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,    // switch to guided mode
            0,                                      // reserved
            qDegreesToRadians(heading),             // change heading
            qQNaN(), qQNaN(), qQNaN());             // no change lat, lon, alt
    } else if (vehicle->apmFirmware()){
        setup(
            MAV_CMD_CONDITION_YAW,
            heading,                                // change heading
            0,                                      // Use default angular speed
            1,                                      // Rotate clockwise
            0,                                      // heading specified as absolute angle
            0, 0, 0);                               // Unused
    }
}
