#include "DetectorList.h"
#include "DetectorInfo.h"
#include "TagDatabase.h"
#include "QGCApplication.h"
#include "CustomPlugin.h"
#include "CustomSettings.h"
#include "TunnelProtocol.h"

#include <QDebug>
#include <QPointF>
#include <QLineF>
#include <QQmlEngine>

using namespace TunnelProtocol;

Q_APPLICATION_STATIC(DetectorList, _detectorListInstance);

DetectorList::DetectorList(QObject* parent)
    : QmlObjectListModel(parent)
{

}

DetectorList::~DetectorList()
{

}

DetectorList* DetectorList::instance()
{
    return _detectorListInstance();
}

void DetectorList::setupFromSelectedTags()
{
    clear();

    TagDatabase*        tagDB               = TagDatabase::instance();
    QmlObjectListModel* tagInfoList         = tagDB->tagInfoListModel();
    CustomSettings*     customSettings      = qobject_cast<CustomPlugin*>(CustomPlugin::instance())->customSettings();
    const bool          isPythonMode        = customSettings->detectionMode()->rawValue().toUInt() == DETECTION_MODE_PYTHON;
    const uint32_t      kValue              = isPythonMode ? customSettings->pythonK()->rawValue().toUInt()
                                                           : customSettings->k()->rawValue().toUInt();

    for (int i=0; i<tagInfoList->count(); i++) {
        TagInfo* tagInfo = tagInfoList->value<TagInfo*>(i);
        if (!tagInfo->selected()->rawValue().toBool()) {
            continue;
        }

        TagManufacturer* tagManufacturer = tagDB->findTagManufacturer(tagInfo->manufacturerId()->rawValue().toUInt());

        DetectorInfo* detectorInfo = new DetectorInfo(
                                            tagInfo->id()->rawValue().toUInt(),
                                            tagManufacturer->ip_msecs_1_id()->rawValue().toString(),
                                            tagManufacturer->ip_msecs_1()->rawValue().toUInt(),
                                            kValue,
                                            this);
        append(detectorInfo);

        if (tagManufacturer->ip_msecs_2()->rawValue().toUInt() != 0) {
            DetectorInfo* detectorInfo = new DetectorInfo(
                                                tagInfo->id()->rawValue().toUInt() + 1,
                                                tagManufacturer->ip_msecs_2_id()->rawValue().toString(),
                                                tagManufacturer->ip_msecs_2()->rawValue().toUInt(),
                                                kValue,
                                                this);
            append(detectorInfo);
        }
    }
}

void DetectorList::handleTunnelPulse(const mavlink_tunnel_t& tunnel)
{
    for (int i=0; i<count(); i++) {
        DetectorInfo* detectorInfo = qobject_cast<DetectorInfo*>(get(i));
        detectorInfo->handleTunnelPulse(tunnel);
    }
}

void DetectorList::resetMaxStrength()
{
    for (int i=0; i<count(); i++) {
        DetectorInfo* detectorInfo = qobject_cast<DetectorInfo*>(get(i));
        detectorInfo->resetMaxStrength();
    }
}

void DetectorList::resetPulseGroupCount()
{
    for (int i=0; i<count(); i++) {
        DetectorInfo* detectorInfo = qobject_cast<DetectorInfo*>((*this)[i]);
        detectorInfo->resetPulseGroupCount();
    }
}

double DetectorList::maxStrength() const
{
    double maxStrength = 0.0;
    for (int i=0; i<count(); i++) {
        const DetectorInfo* detectorInfo = qobject_cast<const DetectorInfo*>((*this)[i]);
        maxStrength = std::max(maxStrength, detectorInfo->maxStrength());
    }

    return maxStrength;
}

bool DetectorList::allHeartbeatCountsReached(uint32_t targetHeartbeatCount) const
{
    for (int i=0; i<count(); i++) {
        const DetectorInfo* detectorInfo = qobject_cast<const DetectorInfo*>((*this)[i]);
        if (detectorInfo->heartbeatCount() < targetHeartbeatCount) {
            return false;
        }
    }

    return true;
}

bool DetectorList::allPulseGroupCountsReached(uint32_t targetPulseGroupCount) const
{
    for (int i=0; i<count(); i++) {
        const DetectorInfo* detectorInfo = qobject_cast<const DetectorInfo*>((*this)[i]);
        if (detectorInfo->pulseGroupCount() < targetPulseGroupCount) {
            return false;
        }
    }

    return true;
}
