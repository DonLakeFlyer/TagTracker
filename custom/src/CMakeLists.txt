qt_add_library(CustomModule STATIC)

set_source_files_properties(ControllerIndicator.qml                     PROPERTIES QT_RESOURCE_ALIAS ControllerIndicator.qml)
set_source_files_properties(CustomGuidedActionAutoTakeoffRotateRTL.qml  PROPERTIES QT_RESOURCE_ALIAS CustomGuidedActionAutoTakeoffRotateRTL.qml)
set_source_files_properties(CustomGuidedActionDownloadLogs.qml          PROPERTIES QT_RESOURCE_ALIAS CustomGuidedActionDownloadLogs.qml)
set_source_files_properties(CustomGuidedActionStartRotation.qml         PROPERTIES QT_RESOURCE_ALIAS CustomGuidedActionStartRotation.qml)
set_source_files_properties(CustomGuidedActionStartStopDetection.qml    PROPERTIES QT_RESOURCE_ALIAS CustomGuidedActionStartStopDetection.qml)
set_source_files_properties(CustomGuidedActionStopDetection.qml         PROPERTIES QT_RESOURCE_ALIAS CustomGuidedActionStopDetection.qml)
set_source_files_properties(PulseMapItem.qml                           PROPERTIES QT_RESOURCE_ALIAS PulseMapItem.qml)
set_source_files_properties(TagInfoDialog.qml                          PROPERTIES QT_RESOURCE_ALIAS TagInfoDialog.qml)
set_source_files_properties(TagManufacturersDialog.qml                 PROPERTIES QT_RESOURCE_ALIAS TagManufacturersDialog.qml)

qt_add_qml_module(CustomModule
    URI TagTracker
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES
        ControllerIndicator.qml
        CustomGuidedActionAutoTakeoffRotateRTL.qml
        CustomGuidedActionDownloadLogs.qml
        CustomGuidedActionStartRotation.qml
        CustomGuidedActionStartStopDetection.qml
        CustomGuidedActionStopDetection.qml
        PulseMapItem.qml
        TagInfoDialog.qml
        TagManufacturersDialog.qml
    NO_PLUGIN
)
