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

qt_add_executable(model_library_controller_test
  model_library_controller_test.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../v2/model/model_library_controller.cpp
)
target_compile_definitions(model_library_controller_test PRIVATE
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(model_library_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
)
target_link_libraries(model_library_controller_test PRIVATE
  Qt6::Core
  Qt6::Widgets
)
add_test(
  NAME model_library_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:model_library_controller_test>
)
set_tests_properties(model_library_controller_test PROPERTIES TIMEOUT 15)

qt_add_executable(dataset_manifest_v2_test
  dataset_manifest_v2_test.cpp
  ../desktop_app/json_persistence.cpp
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

qt_add_executable(sequence_manifest_v2_test
  sequence_manifest_v2_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
)
target_include_directories(sequence_manifest_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(sequence_manifest_v2_test PRIVATE
  Qt6::Core
)
add_test(
  NAME sequence_manifest_v2_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:sequence_manifest_v2_test>
)
set_tests_properties(sequence_manifest_v2_test PROPERTIES TIMEOUT 15)

qt_add_executable(image_sequence_capture_service_test
  image_sequence_capture_service_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/sequence/image_sequence_capture_service.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(image_sequence_capture_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
)
target_link_libraries(image_sequence_capture_service_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME image_sequence_capture_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:image_sequence_capture_service_test>
)
set_tests_properties(image_sequence_capture_service_test PROPERTIES TIMEOUT 15)

qt_add_executable(dataset_label_service_test
  dataset_label_service_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/dataset/dataset_label_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
)
target_include_directories(dataset_label_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(dataset_label_service_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME dataset_label_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:dataset_label_service_test>
)
set_tests_properties(dataset_label_service_test PROPERTIES TIMEOUT 15)

qt_add_executable(dataset_label_controller_test
  dataset_label_controller_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/dataset/dataset_label_controller.cpp
  ../v2/dataset/dataset_label_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(dataset_label_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(dataset_label_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME dataset_label_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:dataset_label_controller_test>
)
set_tests_properties(dataset_label_controller_test PROPERTIES TIMEOUT 15)

qt_add_executable(crop_golden_test
  crop_golden_test.cpp
  ../crops/crop_service.cpp
)
target_include_directories(crop_golden_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(crop_golden_test PRIVATE
  Qt6::Core
  ${OpenCV_LIBS}
)
add_test(
  NAME crop_golden_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:crop_golden_test>
)
set_tests_properties(crop_golden_test PROPERTIES TIMEOUT 15)

qt_add_executable(dataset_capture_recovery_test
  dataset_capture_recovery_test.cpp
  ../crops/crop_service.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/dataset/dataset_capture_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(dataset_capture_recovery_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
)
target_link_libraries(dataset_capture_recovery_test PRIVATE
  Qt6::Core
  Qt6::Gui
  ${OpenCV_LIBS}
)
add_test(
  NAME dataset_capture_recovery_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:dataset_capture_recovery_test>
)
set_tests_properties(dataset_capture_recovery_test PROPERTIES TIMEOUT 30)

qt_add_executable(sequence_viewer_model_test
  sequence_viewer_model_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/sequence/sequence_viewer_model.cpp
)
target_include_directories(sequence_viewer_model_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(sequence_viewer_model_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME sequence_viewer_model_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:sequence_viewer_model_test>
)
set_tests_properties(sequence_viewer_model_test PROPERTIES TIMEOUT 15)

find_package(Qt6 6.8 REQUIRED COMPONENTS Qml Quick)

qt_add_executable(sequence_viewer_controller_test
  sequence_viewer_controller_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/sequence/sequence_viewer_model.cpp
  ../v2/sequence/sequence_viewer_controller.cpp
  ../v2/sequence/sequence_viewer_image_provider.cpp
)
target_include_directories(sequence_viewer_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(sequence_viewer_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
)
add_test(
  NAME sequence_viewer_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:sequence_viewer_controller_test>
)
set_tests_properties(sequence_viewer_controller_test PROPERTIES TIMEOUT 15)

qt_add_executable(training_service_protocol_test
  training_service_protocol_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/training/training_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
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

qt_add_executable(application_state_store_test
  application_state_store_test.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(application_state_store_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(application_state_store_test PRIVATE
  Qt6::Core
)
add_test(
  NAME application_state_store_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:application_state_store_test>
)
set_tests_properties(application_state_store_test PROPERTIES TIMEOUT 15)

qt_add_executable(operation_coordinator_test
  operation_coordinator_test.cpp
  ../v2/operation/operation_coordinator.cpp
)
target_include_directories(operation_coordinator_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(operation_coordinator_test PRIVATE
  Qt6::Core
)
add_test(
  NAME operation_coordinator_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:operation_coordinator_test>
)
set_tests_properties(operation_coordinator_test PROPERTIES TIMEOUT 15)

qt_add_executable(daq_service_test
  daq_service_test.cpp
  ../daq_trigger.cpp
  ../v2/hardware/daq_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_compile_definitions(daq_service_test PRIVATE
  HAVE_NIDAQMX=1
)
target_include_directories(daq_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(daq_service_test PRIVATE
  Qt6::Core
)
add_test(
  NAME daq_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:daq_service_test>
)
set_tests_properties(daq_service_test PROPERTIES TIMEOUT 15)

qt_add_executable(run_manifest_v2_test
  run_manifest_v2_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
)
target_include_directories(run_manifest_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(run_manifest_v2_test PRIVATE
  Qt6::Core
)
add_test(
  NAME run_manifest_v2_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:run_manifest_v2_test>
)
set_tests_properties(run_manifest_v2_test PROPERTIES TIMEOUT 15)

qt_add_executable(run_writer_v2_test
  run_writer_v2_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
)
target_include_directories(run_writer_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(run_writer_v2_test PRIVATE
  Qt6::Core
)
add_test(
  NAME run_writer_v2_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:run_writer_v2_test>
)
set_tests_properties(run_writer_v2_test PROPERTIES TIMEOUT 15)

qt_add_executable(camera_service_contract_test
  camera_service_contract_test.cpp
  ../v2/camera/camera_service.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(camera_service_contract_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(camera_service_contract_test PRIVATE
  Qt6::Core
)
add_test(
  NAME camera_service_contract_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:camera_service_contract_test>
)
set_tests_properties(camera_service_contract_test PROPERTIES TIMEOUT 15)

qt_add_executable(frame_conversion_golden_test
  frame_conversion_golden_test.cpp
  ../v2/camera/frame_conversion.cpp
)
target_include_directories(frame_conversion_golden_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(frame_conversion_golden_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME frame_conversion_golden_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:frame_conversion_golden_test>
)
set_tests_properties(frame_conversion_golden_test PROPERTIES TIMEOUT 15)

qt_add_executable(single_image_capture_test
  single_image_capture_test.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/camera/single_image_capture_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(single_image_capture_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(single_image_capture_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME single_image_capture_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:single_image_capture_test>
)
set_tests_properties(single_image_capture_test PROPERTIES TIMEOUT 15)

qt_add_executable(sequence_test_decision_route_test
  sequence_test_decision_route_test.cpp
  ../v2/decision/decision_service.cpp
  ../v2/routing/observed_route_tracker.cpp
)
target_include_directories(sequence_test_decision_route_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(sequence_test_decision_route_test PRIVATE
  Qt6::Core
)
add_test(
  NAME sequence_test_decision_route_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:sequence_test_decision_route_test>
)
set_tests_properties(sequence_test_decision_route_test PROPERTIES TIMEOUT 15)

qt_add_executable(sequence_test_service_test
  sequence_test_service_test.cpp
  ../crops/crop_service.cpp
  ../desktop_app/json_persistence.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/decision/decision_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/routing/observed_route_tracker.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/sequence_test/sequence_test_service.cpp
)
target_include_directories(sequence_test_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(sequence_test_service_test PRIVATE
  Qt6::Core
  Qt6::Gui
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME sequence_test_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:desktop_app>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:sequence_test_service_test>
)
set_tests_properties(sequence_test_service_test PROPERTIES TIMEOUT 30)

qt_add_executable(live_sorting_service_test
  live_sorting_service_test.cpp
  ../crops/crop_service.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../desktop_app/pipeline_runner.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/decision/decision_service.cpp
  ../v2/live/live_sorting_service.cpp
  ../v2/model/model_load_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/routing/observed_route_tracker.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
)
target_include_directories(live_sorting_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(live_sorting_service_test PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME live_sorting_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:desktop_app>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:live_sorting_service_test>
)
set_tests_properties(live_sorting_service_test PROPERTIES TIMEOUT 30)

qt_add_executable(run_repository_test
  run_repository_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/results/run_repository.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(run_repository_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(run_repository_test PRIVATE
  Qt6::Core
)
add_test(
  NAME run_repository_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:run_repository_test>
)
set_tests_properties(run_repository_test PROPERTIES TIMEOUT 30)

qt_add_executable(runs_results_controller_test
  runs_results_controller_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/results/run_repository.cpp
  ../v2/results/runs_results_controller.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(runs_results_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(runs_results_controller_test PRIVATE
  OPENDSS_TEST_DESKTOP_V2_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../Desktop_app_v2/Desktop_app_v2Content"
)
target_link_libraries(runs_results_controller_test PRIVATE
  Qt6::Core
)
add_test(
  NAME runs_results_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:runs_results_controller_test>
)
set_tests_properties(runs_results_controller_test PROPERTIES TIMEOUT 30)

qt_add_executable(model_test_summary_v2_test
  model_test_summary_v2_test.cpp
  ../v2/model_test/model_test_summary_v2.cpp
)
target_include_directories(model_test_summary_v2_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(model_test_summary_v2_test PRIVATE
  Qt6::Core
)
add_test(
  NAME model_test_summary_v2_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:model_test_summary_v2_test>
)
set_tests_properties(model_test_summary_v2_test PROPERTIES TIMEOUT 15)

qt_add_executable(model_test_writer_test
  model_test_writer_test.cpp
  ../v2/model_test/model_test_summary_v2.cpp
  ../v2/model_test/model_test_writer.cpp
)
target_include_directories(model_test_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(model_test_writer_test PRIVATE
  Qt6::Core
)
add_test(
  NAME model_test_writer_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:model_test_writer_test>
)
set_tests_properties(model_test_writer_test PROPERTIES TIMEOUT 30)

qt_add_executable(model_test_service_test
  model_test_service_test.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/model/model_load_service.cpp
  ../v2/model_test/model_test_service.cpp
  ../v2/model_test/model_test_summary_v2.cpp
  ../v2/model_test/model_test_writer.cpp
  ../v2/operation/operation_coordinator.cpp
)
target_compile_definitions(model_test_service_test PRIVATE
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(model_test_service_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(model_test_service_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME model_test_service_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:desktop_app>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:model_test_service_test>
)
set_tests_properties(model_test_service_test PROPERTIES TIMEOUT 30)

qt_add_executable(settings_repository_test
  settings_repository_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/settings/settings_repository.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(settings_repository_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(settings_repository_test PRIVATE
  Qt6::Core
)
add_test(
  NAME settings_repository_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:settings_repository_test>
)
set_tests_properties(settings_repository_test PROPERTIES TIMEOUT 15)

qt_add_executable(settings_controller_test
  settings_controller_test.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/settings/settings_controller.cpp
  ../v2/settings/settings_repository.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(settings_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(settings_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
)
add_test(
  NAME settings_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:settings_controller_test>
)
set_tests_properties(settings_controller_test PROPERTIES TIMEOUT 15)
