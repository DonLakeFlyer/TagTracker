#include "DetectorList.h"

#include <QtCore/QApplicationStatic>
#include <algorithm>

#include "CustomPlugin.h"
#include "CustomLoggingCategory.h"
#include "CustomSettings.h"
#include "DetectorInfo.h"
#include "PythonDetectorInfo.h"
#include "TagDatabase.h"
#include "UavrtDetectorInfo.h"

Q_APPLICATION_STATIC(DetectorList, _detectorListInstance);

DetectorList::DetectorList(QObject* parent) : QmlObjectListModel(parent) {}

DetectorList::~DetectorList() {}

DetectorList* DetectorList::instance()
{
    return _detectorListInstance();
}

void DetectorList::setupFromSelectedTags()
{
    clearDetectors();

    TagDatabase* tagDB = TagDatabase::instance();
    QmlObjectListModel* tagInfoList = tagDB->tagInfoListModel();
    CustomPlugin* customPlugin = qobject_cast<CustomPlugin*>(CustomPlugin::instance());
    CustomSettings* customSettings = customPlugin->customSettings();
    const bool isPythonMode = customPlugin->isPythonMode();

    // Python heartbeats are a 1 Hz timer independent of K; K only scales the timeout to the tag cadence
    const uint32_t pythonK = customSettings->pythonK()->rawValue().toUInt();
    const uint32_t uavrtK = customSettings->k()->rawValue().toUInt();

    for (int i = 0; i < tagInfoList->count(); i++) {
        TagInfo* tagInfo = tagInfoList->value<TagInfo*>(i);
        if (!tagInfo->selected()->rawValue().toBool()) {
            continue;
        }

        TagManufacturer* tagManufacturer = tagDB->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());
        const uint32_t tagId = tagInfo->id()->rawValue().toUInt();
        if (!tagManufacturer) {
            qCWarning(CustomPluginLog) << "Skipping tag with unknown manufacturer tag_id:" << tagId
                                       << "manufacturerId:" << tagInfo->manufacturerId()->rawValue().toUInt();
            continue;
        }

        if (isPythonMode) {
            // One Python detector handles both rates of a tag
            append(new PythonDetectorInfo(tagId, tagManufacturer->ip_msecs_1_id()->rawValue().toString(),
                                          tagManufacturer->ip_msecs_1()->rawValue().toUInt(), pythonK, this));
        } else {
            append(new UavrtDetectorInfo(tagId, tagManufacturer->ip_msecs_1_id()->rawValue().toString(),
                                         tagManufacturer->ip_msecs_1()->rawValue().toUInt(), uavrtK, this));
            if (tagManufacturer->ip_msecs_2()->rawValue().toUInt() != 0) {
                append(new UavrtDetectorInfo(tagId + 1, tagManufacturer->ip_msecs_2_id()->rawValue().toString(),
                                             tagManufacturer->ip_msecs_2()->rawValue().toUInt(), uavrtK, this));
            }
        }
    }

    // uavrt detectors start heartbeating as soon as START_DETECTION is acked, which follows immediately
    if (!isPythonMode) {
        startHeartbeatWatchdogs();
    }

    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<DetectorInfo*>(get(i))) {
            connect(detectorInfo, &DetectorInfo::heartbeatLostChanged, this, &DetectorList::_updateAnyHeartbeatLost);
        }
    }
    _updateAnyHeartbeatLost();
}

void DetectorList::clearDetectors()
{
    clearAndDeleteContents();
    _updateAnyHeartbeatLost();
}

void DetectorList::_updateAnyHeartbeatLost()
{
    bool anyLost = false;
    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<DetectorInfo*>(get(i)); detectorInfo && detectorInfo->heartbeatLost()) {
            anyLost = true;
            break;
        }
    }
    if (anyLost != _anyHeartbeatLost) {
        _anyHeartbeatLost = anyLost;
        emit anyHeartbeatLostChanged();
    }
}

void DetectorList::startHeartbeatWatchdogs()
{
    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<DetectorInfo*>(get(i))) {
            detectorInfo->startHeartbeatWatchdog();
        }
    }
}

void DetectorList::stopHeartbeatWatchdogs()
{
    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<DetectorInfo*>(get(i))) {
            detectorInfo->stopHeartbeatWatchdog();
        }
    }
}

void DetectorList::handleUavrtPulse(const TunnelProtocol::PulseInfo_t& pulseInfo)
{
    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<UavrtDetectorInfo*>(get(i))) {
            detectorInfo->handlePulse(pulseInfo);
        }
    }
}

void DetectorList::handlePythonPulse(const TunnelProtocol::PythonPulseInfo_t& pulseInfo)
{
    for (int i = 0; i < count(); i++) {
        if (auto* detectorInfo = qobject_cast<PythonDetectorInfo*>(get(i))) {
            detectorInfo->handlePulse(pulseInfo);
        }
    }
}

void DetectorList::handleDetectorHeartbeat(const TunnelProtocol::DetectorHeartbeat_t& heartbeat)
{
    if (heartbeat.detection_mode != DETECTION_MODE_UAVRT && heartbeat.detection_mode != DETECTION_MODE_PYTHON) {
        qCWarning(CustomPluginLog) << "Ignoring DETECTOR_HEARTBEAT unknown detection_mode:" << heartbeat.detection_mode
                                   << "tag_id:" << heartbeat.tag_id;
        return;
    }
    // A detector left running from the other mode can share a tag_id with the current list
    const bool isPython = heartbeat.detection_mode == DETECTION_MODE_PYTHON;
    for (int i = 0; i < count(); i++) {
        auto* detectorInfo = qobject_cast<DetectorInfo*>(get(i));
        if (!detectorInfo || detectorInfo->tagId() != heartbeat.tag_id) {
            continue;
        }
        if (isPython == (qobject_cast<PythonDetectorInfo*>(detectorInfo) != nullptr)) {
            detectorInfo->heartbeatReceived();
        }
    }
}
