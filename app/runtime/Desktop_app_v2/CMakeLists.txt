
cmake_minimum_required(VERSION 3.21.1)

option(LINK_INSIGHT "Link Qt Insight Tracker library" ON)
option(BUILD_QDS_COMPONENTS "Build design studio components" ON)

project(Desktop_app_v2App LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)
set(QML_IMPORT_PATH ${QT_QML_OUTPUT_DIRECTORY}
    CACHE STRING "Import paths for Qt Creator's code model"
    FORCE
)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickTimeline ShaderTools)
qt_standard_project_setup()


qt_add_executable(${CMAKE_PROJECT_NAME} WIN32)
qt_add_resources(${CMAKE_PROJECT_NAME} "configuration"
    PREFIX "/"
    FILES
        qtquickcontrols2.conf)

if (NOT BUILD_QDS_COMPONENTS AND NOT TARGET QtQuickDesignerComponents)
    add_library(QtQuickDesignerComponents INTERFACE)
endif()

include(qds)

if (WIN32)
    get_target_property(_opendss_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_opendss_qt_bin "${_opendss_qmake}" DIRECTORY)
    set(_opendss_windeployqt "${_opendss_qt_bin}/windeployqt.exe")
    if (NOT EXISTS "${_opendss_windeployqt}")
        message(FATAL_ERROR "windeployqt.exe not found: ${_opendss_windeployqt}")
    endif()

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND "${_opendss_windeployqt}"
            --force
            --compiler-runtime
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/Desktop_app_v2Content"
            --dir "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>"
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
        COMMENT "Deploying the configured Qt runtime beside ${CMAKE_PROJECT_NAME}"
        VERBATIM
    )
endif()

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Desktop_app_v2Content/FullSizeImageViewer.qml
    PROPERTIES
        QT_RESOURCE_ALIAS FullSizeImageViewer.qml
        QT_QML_SKIP_CACHEGEN TRUE
)
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/Desktop_app_v2Content/FullSizeImageViewerForm.ui.qml
    PROPERTIES
        QT_RESOURCE_ALIAS FullSizeImageViewerForm.ui.qml
        QT_QML_SKIP_CACHEGEN TRUE
)
qt_target_qml_sources(Desktop_app_v2Content
    QML_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Desktop_app_v2Content/FullSizeImageViewer.qml
        ${CMAKE_CURRENT_SOURCE_DIR}/Desktop_app_v2Content/FullSizeImageViewerForm.ui.qml
)
# The generated content subdirectory finalizes qmldir while include(qds) runs.
_qt_internal_write_deferred_qmldir_file(Desktop_app_v2Content)

include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

if (BUILD_QDS_COMPONENTS)
    include(qmlcomponents OPTIONAL)
endif()

if (LINK_INSIGHT)
    include(insight OPTIONAL)
endif ()

include(GNUInstallDirs)
install(TARGETS ${CMAKE_PROJECT_NAME}
  BUNDLE DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
