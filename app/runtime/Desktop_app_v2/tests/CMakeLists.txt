find_package(Qt6 REQUIRED COMPONENTS QuickTest)

qt_add_executable(tst_ShellSingleImage tst_main.cpp)
target_link_libraries(tst_ShellSingleImage PRIVATE
    Qt6::QuickTest
    Desktop_app_v2plugin
    Desktop_app_v2Contentplugin
)

add_test(NAME ShellSingleImage
    COMMAND tst_ShellSingleImage -input ${CMAKE_CURRENT_SOURCE_DIR}/qml
)
