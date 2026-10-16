set(QGC_APP_NAME "TagTracker" CACHE STRING "Application name" FORCE)
set(QGC_APP_DESCRIPTION "Tag Tracker" CACHE STRING "Application description" FORCE)
set(QGC_APP_COPYRIGHT "Copyright (C) 2025 Don Gagne. All rights reserved." CACHE STRING "Copyright notice" FORCE)
set(QGC_ORG_DOMAIN "org.thegagnes" CACHE STRING "Organization domain" FORCE)
set(QGC_ORG_NAME "TheGagnes" CACHE STRING "Organization name" FORCE)
set(QGC_PACKAGE_NAME "com.thegagnes.tagtracker" CACHE STRING "Package identifier" FORCE)
set(QGC_ANDROID_PACKAGE_NAME "com.thegagnes.tagtracker" CACHE STRING "Android package identifier" FORCE)
# Derived from QGC_PACKAGE_NAME upstream, but without FORCE — set it explicitly so a stale cache can't win.
set(QGC_MACOS_BUNDLE_ID "com.thegagnes.tagtracker" CACHE STRING "macOS bundle identifier" FORCE)

# App icons; the raster files are regenerated from res/icons/tagtracker.svg by tools/generate_icons.py.
# The tagtracker_logo*.svg files and android/res/drawable/ic_launcher_foreground.xml are hand-maintained copies
# of that artwork and must be updated manually when it changes.
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _tagtracker_custom_dir)
set(_tagtracker_icons_dir "${_tagtracker_custom_dir}/res/icons")
set(QGC_MACOS_ICON_PATH
    "${_tagtracker_icons_dir}/tagtracker.icns"
    CACHE FILEPATH "macOS application icon path" FORCE
)
set(QGC_WINDOWS_ICON_PATH
    "${_tagtracker_icons_dir}/tagtracker.ico"
    CACHE FILEPATH "Windows application icon" FORCE
)
set(QGC_APPIMAGE_ICON_256_PATH
    "${_tagtracker_icons_dir}/tagtracker_256.png"
    CACHE FILEPATH "AppImage 256x256 icon path" FORCE
)
set(QGC_APPIMAGE_ICON_SCALABLE_PATH
    "${_tagtracker_icons_dir}/tagtracker.svg"
    CACHE FILEPATH "AppImage SVG icon path" FORCE
)
unset(_tagtracker_custom_dir)
unset(_tagtracker_icons_dir)
