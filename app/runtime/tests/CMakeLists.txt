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

add_executable(model_load_activation_test
  model_load_activation_test.cpp
  ../desktop_app/model_registry_service.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/pipeline_runner.cpp
  ../inference/onnx_inference_adapter.cpp
  ../v2/model/model_load_service.cpp
)
target_compile_definitions(model_load_activation_test PRIVATE
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(model_load_activation_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(model_load_activation_test PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME model_load_activation_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:desktop_app>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:model_load_activation_test>
)

qt_add_executable(dataset_manifest_v2_test
  dataset_manifest_v2_test.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
)
target_include_directories(dataset_manifest_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(dataset_manifest_v2_test PRIVATE
  Qt6::Core
)
add_test(
  NAME dataset_manifest_v2_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:dataset_manifest_v2_test>
)
set_tests_properties(dataset_manifest_v2_test PROPERTIES TIMEOUT 15)

qt_add_executable(training_service_protocol_test
  training_service_protocol_test.cpp
  ../v2/training/training_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
)
target_compile_definitions(training_service_protocol_test PRIVATE
  OPENDSS_TEST_REPOSITORY_ROOT="${CMAKE_SOURCE_DIR}/../.."
)
target_include_directories(training_service_protocol_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(training_service_protocol_test PRIVATE
  Qt6::Core
)
add_test(
  NAME training_service_protocol_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:training_service_protocol_test>
)
set_tests_properties(training_service_protocol_test PROPERTIES TIMEOUT 30)
