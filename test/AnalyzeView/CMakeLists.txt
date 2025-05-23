find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt_add_library(AnalyzeViewTest
    STATIC
        ExifParserTest.cc
        ExifParserTest.h
        GeoTagControllerTest.cc
        GeoTagControllerTest.h
        LogDownloadTest.cc
        LogDownloadTest.h
        MavlinkLogTest.cc
        MavlinkLogTest.h
        PX4LogParserTest.cc
        PX4LogParserTest.h
        ULogParserTest.cc
        ULogParserTest.h
)

target_link_libraries(AnalyzeViewTest
    PRIVATE
        Qt6::Core
        Qt6::Test
        AnalyzeView
        MAVLink
        MockLink
        Utilities
        Vehicle
    PUBLIC
        qgcunittest
)

target_include_directories(AnalyzeViewTest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# https://github.com/ianare/exif-samples
qt_add_resources(AnalyzeViewTest "AnalyzeViewTest_res"
    PREFIX "/"
    FILES
        DSCN0010.jpg
        SampleULog.ulg
)
