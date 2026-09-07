#pragma once

#include "QmlObjectListModel.h"
#include "TunnelProtocol.h"

class DetectorList : public QmlObjectListModel
{
    Q_OBJECT

public:
    DetectorList(QObject* parent = nullptr);
    ~DetectorList();

    static DetectorList* instance();

    /// Creates UavrtDetectorInfo (Survey Detection) or PythonDetectorInfo (rotation) entries for the selected tags.
    void setupFromSelectedTags();

    void clearDetectors() { clearAndDeleteContents(); }

    /// Python detectors only start heartbeating once START_COLLECTION is sent, so their watchdogs are armed then.
    void startHeartbeatWatchdogs();

    void handleUavrtPulse(const TunnelProtocol::PulseInfo_t& pulseInfo);
    void handlePythonPulse(const TunnelProtocol::PythonPulseInfo_t& pulseInfo);
};
