find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(tst_ShellSingleImage tst_main.cpp)
target_link_libraries(tst_ShellSingleImage PRIVATE
    Qt6::QuickTest
    Desktop_app_v2plugin
    Desktop_app_v2Contentplugin
)

get_target_property(_opendss_qmake Qt6::qmake IMPORTED_LOCATION)
execute_process(
    COMMAND "${_opendss_qmake}" -query QT_INSTALL_PLUGINS
    OUTPUT_VARIABLE _opendss_qt_plugin_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
if (NOT EXISTS "${_opendss_qt_plugin_path}/platforms/qoffscreen.dll")
    message(FATAL_ERROR
        "ShellSingleImage requires the Qt offscreen platform plugin at "
        "${_opendss_qt_plugin_path}/platforms/qoffscreen.dll"
    )
endif()

add_test(NAME ShellSingleImage
    COMMAND tst_ShellSingleImage
        -v1
        -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
        -o ShellSingleImage-results.txt,txt
)
set_tests_properties(ShellSingleImage PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tst_ShellSingleImage>"
    ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_opendss_qt_plugin_path};QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)

add_test(NAME ShellSingleImageNativeWindowClamp
    COMMAND tst_ShellSingleImage
        ShellSingleImage::test_appStartsMaximized
        -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
)
set_tests_properties(ShellSingleImageNativeWindowClamp PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tst_ShellSingleImage>"
    ENVIRONMENT
        "QT_QPA_PLATFORM=windows;QT_PLUGIN_PATH=${_opendss_qt_plugin_path};QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
