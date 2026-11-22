#include "UploadMissionState.h"

#include <utility>

#include "CustomLoggingCategory.h"
#include "MissionItem.h"
#include "MissionManager.h"
#include "Vehicle.h"

UploadMissionState::UploadMissionState(QState* parentState, Vehicle* vehicle, const QList<MissionItem*>& missionItems)
    : CustomState("UploadMissionState", parentState)
    , _vehicle(vehicle)
    , _missionItems(missionItems)
{
    _timeoutTimer.setSingleShot(true);
    _timeoutTimer.setInterval(kTimeoutMsecs);

    connect(this, &QState::entered, this, &UploadMissionState::_upload);
    connect(this, &QState::exited, this, &UploadMissionState::_disconnectAll);
}

UploadMissionState::~UploadMissionState()
{
    // Items never handed to the mission manager (state not entered, or the upload refused)
    qDeleteAll(_missionItems);
}

void UploadMissionState::_upload()
{
    if (!_vehicle) {
        setError(tr("No vehicle for the mission upload"));
        return;
    }
    MissionManager* const missionManager = _vehicle->missionManager();
    if (!missionManager || missionManager->inProgress()) {
        setError(tr("The vehicle is busy with another mission transfer"));
        return;
    }

    qCDebug(CustomPluginLog) << "Uploading mission items:" << _missionItems.count();
    connect(missionManager, &MissionManager::sendComplete, this, &UploadMissionState::_sendComplete);
    connect(&_timeoutTimer, &QTimer::timeout, this, &UploadMissionState::_timeout);
    _timeoutTimer.start();
    missionManager->writeMissionItems(std::exchange(_missionItems, {}));
}

void UploadMissionState::_sendComplete(bool error)
{
    _disconnectAll();
    if (error) {
        setError(tr("Mission upload failed"));
        return;
    }
    qCDebug(CustomPluginLog) << "Mission upload complete";
    emit uploadComplete();
}

void UploadMissionState::_timeout()
{
    _disconnectAll();
    setError(tr("Timeout waiting for the mission upload"));
}

void UploadMissionState::_disconnectAll()
{
    _timeoutTimer.stop();
    disconnect(&_timeoutTimer, &QTimer::timeout, this, &UploadMissionState::_timeout);
    if (_vehicle && _vehicle->missionManager()) {
        disconnect(_vehicle->missionManager(), &MissionManager::sendComplete, this, &UploadMissionState::_sendComplete);
    }
}
