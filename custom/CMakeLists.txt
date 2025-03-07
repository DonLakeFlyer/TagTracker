message(STATUS "Adding Custom Plugin")
find_package(Qt6 REQUIRED COMPONENTS Core StateMachine)

# Enable custom build
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
)

# Tag Tracker does not support video
#add_compile_definitions(
#    QGC_HERELINK_AIRUNIT_VIDEO
#)

if(ANDROID)
    add_compile_definitions(TAG_TRACKER_HERELINK_BUILD)
endif()

# Our own, custom resources
list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qgcimages.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/qgcresources.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/qgroundcontrol.qrc
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
set(QGC_RESOURCES ${CUSTOM_RESOURCES} PARENT_SCOPE)

set(QML_IMPORT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/res" CACHE STRING "Extra qml import paths" PARENT_SCOPE)

FILE(GLOB BearingHeaders uavrt_bearing/codegen/exe/bearing/*.h)
FILE(GLOB BearingSources uavrt_bearing/codegen/exe/bearing/*.cpp)
FILE(GLOB BearingSourcesC uavrt_bearing/codegen/exe/bearing/*.c)

qt_add_library(CustomPlugin STATIC
    src/HerelinkCorePlugin.cc
    src/HerelinkCorePlugin.h
    src/HerelinkOptions.cc
    src/HerelinkOptions.h
    src/CustomOptions.cc
    src/CustomOptions.h
    src/CustomPlugin.cc
    src/CustomPlugin.h
    src/CustomSettings.cc
    src/CustomSettings.h
    src/CustomStateMachine.cc
    src/CustomStateMachine.h
    src/DetectorInfo.cc
    src/DetectorInfo.h
    src/DetectorList.cc
    src/DetectorList.h
    src/TagDatabase.cc
    src/TagDatabase.h
    src/PulseInfoList.cc
    src/PulseInfoList.h
    src/PulseRoseMapItem.h
    src/PulseMapItem.h
    src/SendTagsState.cc
    src/SendTagsState.h
    src/CustomLoggingCategory.cc
    src/CustomLoggingCategory.h
    src/SendTunnelCommandState.cc
    src/SendTunnelCommandState.h
    ${BearingHeaders}
    ${BearingSources}
    ${BearingSourcesC}
)

# $$files(uavrt_bearing/codegen/exe/bearing/*.cpp)
# $$files(uavrt_bearing/codegen/exe/bearing/*.c)

target_link_libraries(CustomPlugin
    PRIVATE
        FirmwarePlugin
        PX4AutoPilotPlugin
        PX4FirmwarePlugin
        QmlControls
        Vehicle
        Audio
    PUBLIC
        Qt6::Core
        Qt6::StateMachine
        MAVLink
        QGC
        Settings
        FactSystem
)

target_include_directories(CustomPlugin
    PUBLIC
        src
        matlab-coder-utils/coder-headers
        uavrt_bearing/codegen/exe/bearing
        uavrt_interfaces/include/uavrt_interfaces
)
