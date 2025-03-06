#pragma once

#include "QmlObjectListModel.h"
#include "QGCMAVLink.h"

class DetectorList : public QmlObjectListModel
{
    Q_OBJECT

public:
    DetectorList(QObject* parent = nullptr);
    ~DetectorList();

    static DetectorList* instance();

    void    setupFromSelectedTags       ();
    void    handleTunnelPulse           (const mavlink_tunnel_t& tunnel);
    void    resetMaxStrength            ();
    void    resetPulseGroupCount        ();
    double  maxStrength                 () const;
    bool    allHeartbeatCountsReached   (uint32_t targetHeartbeatCount) const;
    bool    allPulseGroupCountsReached  (uint32_t targetPulseGroupCount) const;
};
