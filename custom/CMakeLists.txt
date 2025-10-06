message(STATUS "QGC: Adding Custom Plugin")

# Enable custom build
set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
    APPEND PROPERTY COMPILE_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
)

if(ANDROID)
    set(CUSTOM_ANDROID_DIR "${CMAKE_SOURCE_DIR}/custom/android")
    if(EXISTS "${CUSTOM_ANDROID_DIR}")
        file(GLOB CUSTOM_ANDROID_FILES "${CUSTOM_ANDROID_DIR}/*")
        if(CUSTOM_ANDROID_FILES)
            message(STATUS "QGC: Custom Android package template found. Overlaying custom files...")
            set(DEFAULT_ANDROID_DIR "${CMAKE_SOURCE_DIR}/android")
            set(FINAL_ANDROID_DIR "${CMAKE_BINARY_DIR}/custom/android")
            file(COPY "${DEFAULT_ANDROID_DIR}/." DESTINATION "${FINAL_ANDROID_DIR}")
            file(COPY "${CUSTOM_ANDROID_DIR}/." DESTINATION "${FINAL_ANDROID_DIR}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                            "${DEFAULT_ANDROID_DIR}/"
                            "${CUSTOM_ANDROID_DIR}/"
                        )
            set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${FINAL_ANDROID_DIR}" CACHE PATH "Path to a custom Android package template" FORCE)
            message(STATUS "QGC: Android package template path will be set to: ${QGC_ANDROID_PACKAGE_SOURCE_DIR}")
        else()
            message(STATUS "QGC: Custom Android package template empty. Using default.")
        endif()
    else()
        message(STATUS "QGC: No custom Android package template found. Using default.")
    endif()
endif()

if (ANDROID)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR}
        APPEND PROPERTY COMPILE_DEFINITIONS
        TAG_TRACKER_HERELINK_BUILD
    )
endif()

add_subdirectory(src)

# Our own, custom resources
list(APPEND CUSTOM_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/custom.qrc
)
set(QGC_RESOURCES ${QGC_RESOURCES} ${CUSTOM_RESOURCES} CACHE STRING "Paths to .qrc Resources" FORCE)

set(QML_IMPORT_PATH ${QML_IMPORT_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/src" CACHE STRING "Extra qml import paths" FORCE)

# Build up list of UAVRT sources

FILE(GLOB BearingHeaders uavrt_bearing/codegen/exe/bearing/*.h)
FILE(GLOB BearingSources uavrt_bearing/codegen/exe/bearing/*.cpp)
FILE(GLOB BearingSourcesC uavrt_bearing/codegen/exe/bearing/*.c)

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVLogManager.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVLogManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomLoggingCategory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomLoggingCategory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomOptions.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomSettings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorList.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkCorePlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkCorePlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkOptions.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HerelinkOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseInfoList.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseInfoList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseRoseMapItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseMapItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RotationInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RotationInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SliceInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SliceInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagDatabase.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CaptureAtSliceState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CaptureAtSliceState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomStateMachine.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomStateMachine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/DelayState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/DelayState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/ErrorTransition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FactWaitForValueTarget.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FactWaitForValueTarget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SliceSequenceCaptureState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SliceSequenceCaptureState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FunctionState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FunctionState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/GuidedModeCancelledTransition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FullRotateAndCaptureState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FullRotateAndCaptureState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateAndCaptureStateBase.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateAndCaptureStateBase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateAndRateHeartbeatWaitState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateAndRateHeartbeatWaitState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateMavlinkCommandState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/RotateMavlinkCommandState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SetFlightModeState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SetFlightModeState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SayState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendMavlinkCommandState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendMavlinkCommandState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTagsState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTagsState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTunnelCommandState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTunnelCommandState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SmartRotateAndCaptureState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SmartRotateAndCaptureState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StartDetectionState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StartDetectionState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StopDetectionState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StopDetectionState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/TakeoffState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/TakeoffState.h
    ${BearingHeaders}
    ${BearingSources}
    ${BearingSourcesC}
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_QT_COMPONENTS 
    StateMachine
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_LIBRARIES
    CustomModule # Explicitly link QML module, needed by Qt 6.6.3
    Qt6::StateMachine
    CACHE INTERNAL "" FORCE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine
    ${CMAKE_CURRENT_SOURCE_DIR}/matlab-coder-utils/coder-headers
    ${CMAKE_CURRENT_SOURCE_DIR}/uavrt_bearing/codegen/exe/bearing
    ${CMAKE_CURRENT_SOURCE_DIR}/uavrt_interfaces/include/uavrt_interfaces
    CACHE INTERNAL "" FORCE
)

