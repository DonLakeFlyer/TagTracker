set(QGC_APP_NAME "TagTracker" CACHE STRING "Application name" FORCE)
set(QGC_APP_DESCRIPTION "Tag Tracker" CACHE STRING "Application description" FORCE)
set(QGC_APP_COPYRIGHT "Copyright (C) 2025 Don Gagne. All rights reserved." CACHE STRING "Copyright notice" FORCE)
set(QGC_ORG_DOMAIN "org.thegagnes" CACHE STRING "Organization domain" FORCE)
set(QGC_ORG_NAME "TheGagnes" CACHE STRING "Organization name" FORCE)
set(QGC_PACKAGE_NAME "com.thegagnes.tagtracker" CACHE STRING "Package identifier" FORCE)
set(QGC_ANDROID_PACKAGE_NAME "com.thegagnes.tagtracker" CACHE STRING "Android package identifier" FORCE)
# Derived from QGC_PACKAGE_NAME upstream, but without FORCE — set it explicitly so a stale cache can't win.
set(QGC_MACOS_BUNDLE_ID "com.thegagnes.tagtracker" CACHE STRING "macOS bundle identifier" FORCE)
