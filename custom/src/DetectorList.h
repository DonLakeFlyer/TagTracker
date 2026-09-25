#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

class DetectorList : public QmlObjectListModel
{
    Q_OBJECT
    Q_PROPERTY(bool anyHeartbeatLost READ anyHeartbeatLost NOTIFY anyHeartbeatLostChanged)

public:
    DetectorList(QObject* parent = nullptr);
    ~DetectorList();

    static DetectorList* instance();

    /// Creates UavrtDetectorInfo (Survey Detection) or PythonDetectorInfo (rotation) entries for the selected tags.
    void setupFromSelectedTags();

    void clearDetectors();

    bool anyHeartbeatLost() const { return _anyHeartbeatLost; }

    /// Python detectors only start heartbeating once START_COLLECTION is sent, so their watchdogs are armed then.
    void startHeartbeatWatchdogs();
    /// FINISH_COLLECTION tears the Python detectors down, so silence after a rotation is not a failure.
    void stopHeartbeatWatchdogs();

    void handleUavrtPulse(const TunnelProtocol::PulseInfo_t& pulseInfo);
    void handlePythonPulse(const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
    void handleDetectorHeartbeat(const TunnelProtocol::DetectorHeartbeat_t& heartbeat);

signals:
    void anyHeartbeatLostChanged();

private:
    void _updateAnyHeartbeatLost();

    bool _anyHeartbeatLost = false;
};
