message(STATUS "Adding Custom Plugin")
find_package(Qt6 REQUIRED COMPONENTS Core StateMachine)

# Enable custom build
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
)

find_package(Python3)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
        RESULT_VARIABLE PYTHON_RESULT
        OUTPUT_VARIABLE PYTHON_OUTPUT
        ERROR_VARIABLE PYTHON_ERROR
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        INPUT_FILE updateqrc.py
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    cmake_print_variables(PYTHON_RESULT PYTHON_OUTPUT PYTHON_ERROR)
endif()

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
    src/CSVLogManager.cc
    src/CSVLogManager.h
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
    src/CustomLoggingCategory.cc
    src/CustomLoggingCategory.h
    src/StateMachine/CustomState.cc
    src/StateMachine/CustomState.h
    src/StateMachine/CustomStateMachine.cc
    src/StateMachine/CustomStateMachine.h
    src/StateMachine/DelayState.cc
    src/StateMachine/DelayState.h
    src/StateMachine/ErrorTransition.h
    src/StateMachine/FactWaitForValueTarget.cc
    src/StateMachine/FactWaitForValueTarget.h
    src/StateMachine/FunctionState.cc
    src/StateMachine/FunctionState.h
    src/StateMachine/GuidedModeCancelledTransition.h
    src/StateMachine/RotateAndCaptureState.cc
    src/StateMachine/RotateAndCaptureState.h
    src/StateMachine/SetFlightModeState.cc
    src/StateMachine/SetFlightModeState.h
    src/StateMachine/SayState.h
    src/StateMachine/SendMavlinkCommandState.cc
    src/StateMachine/SendMavlinkCommandState.h
    src/StateMachine/SendTagsState.cc
    src/StateMachine/SendTagsState.h
    src/StateMachine/SendTunnelCommandState.cc
    src/StateMachine/SendTunnelCommandState.h
    src/StateMachine/StartDetectionState.cc
    src/StateMachine/StartDetectionState.h
    src/StateMachine/StopDetectionState.cc
    src/StateMachine/StopDetectionState.h
    src/StateMachine/TakeoffState.cc
    src/StateMachine/TakeoffState.h
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
        Utilities
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
        src/StateMachine
        matlab-coder-utils/coder-headers
        uavrt_bearing/codegen/exe/bearing
        uavrt_interfaces/include/uavrt_interfaces
)
