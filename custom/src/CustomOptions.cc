#include "CustomOptions.h"
#include "CustomPlugin.h"

CustomOptions::CustomOptions(CustomPlugin* plugin, QObject* parent)
#ifdef TAG_TRACKER_HERELINK_BUILD
    : HerelinkOptions(parent)
#else
    : QGCOptions(parent)
#endif
{

}
