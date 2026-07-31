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
get_filename_component(_opendss_qt_bin "${_opendss_qmake}" DIRECTORY)
foreach(_opendss_qt_dll
        Qt6Core.dll
        Qt6Gui.dll
        Qt6Qml.dll
        Qt6Quick.dll
        Qt6QuickTest.dll)
    if (NOT EXISTS "${_opendss_qt_bin}/${_opendss_qt_dll}")
        message(FATAL_ERROR
            "ShellSingleImage requires ${_opendss_qt_dll} from the selected "
            "Qt installation at ${_opendss_qt_bin}"
        )
    endif()
endforeach()

add_test(NAME ShellSingleImage
    COMMAND "${CMAKE_COMMAND}" -E env
        "PATH=$<TARGET_FILE_DIR:tst_ShellSingleImage>;${_opendss_qt_bin};$ENV{SystemRoot}/System32;$ENV{SystemRoot}"
        "QT_QPA_PLATFORM=offscreen"
        "QT_PLUGIN_PATH=${_opendss_qt_plugin_path}"
        "QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
        "$<TARGET_FILE:tst_ShellSingleImage>"
        -v1
        -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
        -o ShellSingleImage-results.txt,txt
)
set_tests_properties(ShellSingleImage PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:tst_ShellSingleImage>"
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
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_opendss_qt_bin};PATH=path_list_prepend:$<TARGET_FILE_DIR:tst_ShellSingleImage>"
)
