#pragma once

#include <QtCore/QList>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include "CustomState.h"

class MissionItem;
class Vehicle;

/// Writes a mission with the vehicle's MissionManager and waits for the vehicle's final ack.
/// The items go to PlanManager::writeMissionItems unchanged, so on PX4 index 0 must be the home placeholder.
class UploadMissionState : public CustomState
{
    Q_OBJECT

public:
    /// Takes ownership of missionItems.
    UploadMissionState(QState* parentState, Vehicle* vehicle, const QList<MissionItem*>& missionItems);
    ~UploadMissionState() override;

    static constexpr int kTimeoutMsecs = 15000;

signals:
    void uploadComplete();

private slots:
    void _upload();
    void _sendComplete(bool error);
    void _timeout();
    void _disconnectAll();

private:
    QPointer<Vehicle> _vehicle;
    QList<MissionItem*> _missionItems;
    QTimer _timeoutTimer;
};
