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

add_executable(opendss_detector_characterization_test
  detector_characterization_test.cpp
)

target_include_directories(opendss_detector_characterization_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(opendss_detector_characterization_test PRIVATE
  opendss_runtime
  ${OpenCV_LIBS}
)

add_test(
  NAME opendss_detector_characterization_test
  COMMAND opendss_detector_characterization_test
)

add_executable(opendss_detector_contract_parity_test
  detector_contract_parity_test.cpp
)

target_include_directories(opendss_detector_contract_parity_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(opendss_detector_contract_parity_test PRIVATE
  opendss_runtime
  ${OpenCV_LIBS}
)

add_test(
  NAME opendss_detector_contract_parity_test
  COMMAND opendss_detector_contract_parity_test
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

add_executable(opendss_p0_2a_characterization_test
  p0_2a_characterization_test.cpp
  ../daq_trigger.cpp
  ../dataset_capture_session.cpp
  ../desktop_app/background_task_registry.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/live_data_collection_writer.cpp
  ../desktop_app/live_log_writer.cpp
  ../desktop_app/sequence_summary_writer.cpp
)

target_compile_definitions(opendss_p0_2a_characterization_test PRIVATE HAVE_NIDAQMX)

target_include_directories(opendss_p0_2a_characterization_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(opendss_p0_2a_characterization_test PRIVATE
  Qt6::Core
  Qt6::Gui
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)

add_test(
  NAME opendss_p0_2a_characterization_test
  COMMAND opendss_p0_2a_characterization_test
)

set_tests_properties(opendss_p0_2a_characterization_test PROPERTIES TIMEOUT 15)
