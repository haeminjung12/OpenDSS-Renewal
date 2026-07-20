add_executable(opendss_runtime_metadata_loader_test
  runtime_metadata_loader_test.cpp
)

target_link_libraries(opendss_runtime_metadata_loader_test PRIVATE
  opendss_runtime
)

add_test(
  NAME opendss_runtime_metadata_loader_test
  COMMAND opendss_runtime_metadata_loader_test
)

add_executable(opendss_event_direction_test
  event_direction_test.cpp
)

target_include_directories(opendss_event_direction_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${DCAM_SDK_DIR}/inc
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(opendss_event_direction_test PRIVATE
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)

add_test(
  NAME opendss_event_direction_test
  COMMAND opendss_event_direction_test
)

add_executable(opendss_trainer_plot_math_test
  trainer_plot_math_test.cpp
)

target_include_directories(opendss_trainer_plot_math_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
)

add_test(
  NAME opendss_trainer_plot_math_test
  COMMAND opendss_trainer_plot_math_test
)
