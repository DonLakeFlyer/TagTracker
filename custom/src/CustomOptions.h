#pragma once

#ifdef TAG_TRACKER_HERELINK_BUILD
    #include "HerelinkOptions.h"
#else
    #include "QGCOptions.h"
#endif

class CustomPlugin;

class CustomOptions : 
#ifdef TAG_TRACKER_HERELINK_BUILD
    public HerelinkOptions
#else
    public QGCOptions
#endif
{
public:
    CustomOptions(CustomPlugin* plugin, QObject* parent = NULL);

    // QGCOptions overrides
    virtual bool guidedBarShowOrbit () const final { return false; }
    virtual bool guidedBarShowROI   () const final { return false; }
};
