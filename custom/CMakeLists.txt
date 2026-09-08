# ============================================================================
# TagTracker Custom Plugin
# ============================================================================

message(STATUS "QGC: Adding TagTracker Custom Plugin")

# ============================================================================
# Custom Android Package Template
# ============================================================================

if(ANDROID)
    set(CUSTOM_ANDROID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    file(GLOB_RECURSE CUSTOM_ANDROID_FILES
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES FALSE
        "${CUSTOM_ANDROID_DIR}/*"
    )

    # Only overlay when there is something to overlay; otherwise upstream's android/ is used as-is.
    if(CUSTOM_ANDROID_FILES)
        set(DEFAULT_ANDROID_DIR "${CMAKE_SOURCE_DIR}/android")
        set(FINAL_ANDROID_DIR "${CMAKE_BINARY_DIR}/${QGC_CUSTOM_DIR}/android")

        # Recreate the merge so removed source files cannot remain stale.
        file(REMOVE_RECURSE "${FINAL_ANDROID_DIR}")
        file(COPY "${DEFAULT_ANDROID_DIR}/" DESTINATION "${FINAL_ANDROID_DIR}")
        file(COPY "${CUSTOM_ANDROID_DIR}/" DESTINATION "${FINAL_ANDROID_DIR}")

        set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${FINAL_ANDROID_DIR}")
        set(QGC_ANDROID_PACKAGE_SOURCE_DIR "${FINAL_ANDROID_DIR}" PARENT_SCOPE)
    endif()
endif()

# ============================================================================
# QML Resource Overrides
# ============================================================================
# Loaded through CustomOverrideInterceptor: a request for qrc:/qml/<path>
# resolves to :/Custom/qml/<path> when that alias exists.

set(CUSTOM_QML_OVERRIDES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceOverrides/FlyView/CustomGuidedActionsController.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceOverrides/FlyView/FlyViewAdditionalCustomActionsList.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceOverrides/FlyView/FlyViewCustomLayer.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ResourceOverrides/FlyView/FlyViewToolStrip.qml"
)
foreach(_override IN LISTS CUSTOM_QML_OVERRIDES)
    get_filename_component(_override_name "${_override}" NAME)
    set_source_files_properties("${_override}"
        PROPERTIES QT_RESOURCE_ALIAS "QGroundControl/FlyView/${_override_name}"
    )
endforeach()

qt_add_resources(${CMAKE_PROJECT_NAME} custom_qml_overrides
    PREFIX "/Custom/qml"
    FILES ${CUSTOM_QML_OVERRIDES}
)

# ============================================================================
# Standalone QML loaded by URL
# ============================================================================

set(CUSTOM_URL_LOADED_QML
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ControllerIndicator.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPulseRoseMapItem.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EmergencyStopIndicator.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/PulseMapItem.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TagTrackerSettings.qml"
)
qgc_set_qt_resource_alias(${CUSTOM_URL_LOADED_QML})
qt_add_resources(${CMAKE_PROJECT_NAME} custom_url_loaded_qml
    PREFIX "/qml"
    FILES ${CUSTOM_URL_LOADED_QML}
)

# ============================================================================
# JSON / Text Resources
# ============================================================================

set(CUSTOM_JSON_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/Custom.SettingsGroup.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TagInfo.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TagManufacturer.json"
)
qgc_set_qt_resource_alias(${CUSTOM_JSON_RESOURCES})
qt_add_resources(${CMAKE_PROJECT_NAME} custom_json
    PREFIX "/json"
    FILES ${CUSTOM_JSON_RESOURCES}
)

set(CUSTOM_RES_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/TagInfo.txt"
    "${CMAKE_SOURCE_DIR}/resources/action.svg"
)
qgc_set_qt_resource_alias(${CUSTOM_RES_RESOURCES})
qt_add_resources(${CMAKE_PROJECT_NAME} custom_res
    PREFIX "/res"
    FILES ${CUSTOM_RES_RESOURCES}
)

# ============================================================================
# QGroundControl.CustomControls QML Module
# ============================================================================

qt_add_library(CustomControlsModule STATIC)
qgc_configure_target(CustomControlsModule)

qgc_set_qt_resource_alias(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionAutoDetection.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionStartRotation.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionStartStopDetection.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagInfoDialog.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagManufacturersDialog.qml
)

qt_add_qml_module(CustomControlsModule
    URI QGroundControl.CustomControls
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionAutoDetection.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionStartRotation.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomGuidedActionStartStopDetection.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TagInfoDialog.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TagManufacturersDialog.qml
    NO_PLUGIN
)

# ============================================================================
# Custom Plugin Sources
# ============================================================================

set(CUSTOM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLogDownloader.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CompanionLogDownloader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVLogManager.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CSVLogManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomLoggingCategory.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomLoggingCategory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomOptions.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/CustomSettings.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings/CustomSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorList.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/DetectorList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseMapItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PythonDetectorInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PythonDetectorInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PulseRoseMapItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RotationInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RotationInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SliceInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SliceInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagDatabase.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TagDatabase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UavrtDetectorInfo.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UavrtDetectorInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomStateMachine.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/CustomStateMachine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/ErrorTransition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FactWaitForValueTarget.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/FactWaitForValueTarget.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/GuidedModeCancelledTransition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonCaptureAtSliceState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonCaptureAtSliceState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonRotateAndCaptureState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonRotateAndCaptureState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonWaitForDetectionResultState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/PythonWaitForDetectionResultState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SayState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTagsState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTagsState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTunnelCommandState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SendTunnelCommandState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SetFlightModeState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/SetFlightModeState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StartDetectionState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StartDetectionState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StopDetectionState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/StopDetectionState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/TakeoffState.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine/TakeoffState.h
)

if(QGC_BUILD_TESTING)
    list(APPEND CUSTOM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test/TagTrackerPulseDisplayTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/TagTrackerPulseDisplayTest.h
    )

    include(QGCTest)
    add_qgc_test(TagTrackerPulseDisplayTest LABELS Unit)
endif()

set(CUSTOM_SOURCES ${CUSTOM_SOURCES} PARENT_SCOPE)

# ============================================================================
# Tunnel Protocol (shared with the companion computer firmware)
# ============================================================================
# Header-only repo with no CMakeLists, hence DOWNLOAD_ONLY.

CPMAddPackage(
    NAME              TunnelProtocol
    GITHUB_REPOSITORY DonLakeFlyer/TagTrackerTunnelProtocol
    GIT_TAG           f4da01e9fb3d8e517a80b4b923f2efb8292da6e5
    DOWNLOAD_ONLY     TRUE
)

# ============================================================================
# Custom Build Configuration
# ============================================================================

set(CUSTOM_LIBRARIES
    CustomControlsModule
    PARENT_SCOPE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateMachine
    ${TunnelProtocol_SOURCE_DIR}
    PARENT_SCOPE
)

set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD
    CUSTOMHEADER="CustomPlugin.h"
    CUSTOMCLASS=CustomPlugin
    PARENT_SCOPE
)

set(CUSTOM_QT_COMPONENTS
    Core
    StateMachine
    PARENT_SCOPE
)
