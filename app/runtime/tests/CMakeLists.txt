set(OPENDSS_CUDA_RUNTIME_DIR
  "$ENV{LOCALAPPDATA}/OpenDSS/training-venv-gpu/Lib/site-packages/torch/lib"
  CACHE PATH "Qualified CUDA 13 and cuDNN runtime DLL directory"
)

get_target_property(_opendss_test_qmake Qt6::qmake IMPORTED_LOCATION)
execute_process(
  COMMAND "${_opendss_test_qmake}" -query QT_INSTALL_PLUGINS
  OUTPUT_VARIABLE OPENDSS_TEST_QT_PLUGIN_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
if(NOT EXISTS "${OPENDSS_TEST_QT_PLUGIN_PATH}/platforms/qoffscreen.dll")
  message(FATAL_ERROR
    "OpenDSS GUI-capable tests require the Qt offscreen platform plugin at "
    "${OPENDSS_TEST_QT_PLUGIN_PATH}/platforms/qoffscreen.dll"
  )
endif()

if(NOT EXISTS "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll")
  message(FATAL_ERROR
    "Qualified ONNX Runtime test DLL is missing: "
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
  )
endif()
file(SHA256
  "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
  _opendss_test_onnxruntime_sha256
)
if(NOT _opendss_test_onnxruntime_sha256 STREQUAL
    "a64bdd69d14f3685142c34ff46546a98cdd9ccae6130619bbee7414b5dd83b1b")
  message(FATAL_ERROR
    "Qualified ONNX Runtime test DLL hash mismatch: "
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
  )
endif()

function(opendss_configure_offscreen_test test_name target_name)
  set_tests_properties(${test_name} PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target_name}>"
    ENVIRONMENT
      "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${OPENDSS_TEST_QT_PLUGIN_PATH};QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
  )
endfunction()

qt_add_executable(qt_offscreen_platform_smoke_test
  qt_offscreen_platform_smoke_test.cpp
)
target_link_libraries(qt_offscreen_platform_smoke_test PRIVATE Qt6::Gui)
add_test(
  NAME qt_offscreen_platform_smoke_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:qt_offscreen_platform_smoke_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:qt_offscreen_platform_smoke_test>
)
opendss_configure_offscreen_test(
  qt_offscreen_platform_smoke_test
  qt_offscreen_platform_smoke_test
)

function(opendss_deploy_qualified_cuda target_name)
  set(qualified_runtime_file_hashes
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll" "a64bdd69d14f3685142c34ff46546a98cdd9ccae6130619bbee7414b5dd83b1b"
    "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll" "a22f19b4a103ac8c1828fdfe03c18a4e408332de9fd5003a9b6ccb3a0c2e3965"
    "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_cuda.dll" "b54ad9ce0feac6eb39843bdbeb01253b8dd4c8032b839ee2a6102bf29fa73468"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cublas64_13.dll" "5d083083cdd613577496791dd96d00fe3a78c955684b53ca46a8ffa3e0b1e170"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cublasLt64_13.dll" "e1c26671801ecd435baf97c6e9cfe28196cf55005c2039356b48030b09ce5dd5"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cufft64_12.dll" "611ba7e40dfab64b9b5bd35f4ad3593e00a8e93785fbf53160d9398aacd5ac14"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn64_9.dll" "99a529a5c252fbc1efcb2f51860498aed053be878090f2094ac86638d4765021"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn_ops64_9.dll" "92c09220f486c3eb3a456106e069667253f153792b2e8e91cba49db110a74f5e"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn_graph64_9.dll" "bd641fd259e877928ae9469efae7a40666d0b4db71ea1df18a40479378b75dfe"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn_heuristic64_9.dll" "c81c3cd1d76fade865512ceb5318f793b1871097aa92afefcddf5e4dd6cbd924"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn_engines_precompiled64_9.dll" "c329d58cb49e3a1574d2bbdde12c6a53f787c12f813efa9db7efa8d16c4207fb"
    "${OPENDSS_CUDA_RUNTIME_DIR}/cudnn_engines_runtime_compiled64_9.dll" "9d0cafc368b1998811f49509b031a4d69c5972a91a3210d89ae183ba7a918719"
    "${OPENDSS_CUDA_RUNTIME_DIR}/nvrtc64_130_0.dll" "37d2195bcdc5db37a838a8eb4f286f502ea94d82319bb7de68751ad672f4ad6d"
  )
  while(qualified_runtime_file_hashes)
    list(POP_FRONT qualified_runtime_file_hashes runtime_dll expected_sha256)
    if(NOT EXISTS "${runtime_dll}")
      message(FATAL_ERROR "Qualified CUDA deployment file is missing: ${runtime_dll}")
    endif()
    file(SHA256 "${runtime_dll}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
      message(FATAL_ERROR "Qualified CUDA deployment hash mismatch: ${runtime_dll}")
    endif()
    list(APPEND qualified_runtime_files "${runtime_dll}")
  endwhile()

  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${qualified_runtime_files}
      "$<TARGET_FILE_DIR:${target_name}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_SOURCE_DIR}/../models/cuda_inference_readiness.json"
      "$<TARGET_FILE_DIR:${target_name}>"
    COMMENT "Deploying qualified ONNX Runtime CUDA 13 provider for ${target_name}"
  )
endfunction()

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
opendss_configure_offscreen_test(
  opendss_detector_characterization_test
  opendss_detector_characterization_test
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
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:opendss_p0_2a_characterization_test>
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
  ../inference/onnx_inference_adapter.cpp
  ../v2/model/model_library_controller.cpp
  ../v2/model/model_load_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/operation/operation_coordinator.h
)
target_compile_definitions(model_library_controller_test PRIVATE
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
  OPENDSS_MODEL_LOAD_NO_PIPELINE
)
target_include_directories(model_library_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(model_library_controller_test PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_custom_command(TARGET model_library_controller_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    "$<TARGET_FILE_DIR:model_library_controller_test>"
  VERBATIM
)
add_test(
  NAME model_library_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=${ONNXRUNTIME_DIR}/lib\\;$<TARGET_FILE_DIR:model_library_controller_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:model_library_controller_test>
)
set_tests_properties(model_library_controller_test PROPERTIES TIMEOUT 30)

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
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
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

qt_add_executable(capture_workflow_controller_test
  capture_workflow_controller_test.cpp
  ../crops/crop_service.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/camera/camera_controller.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/dataset/dataset_capture_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/sequence/capture_workflow_controller.cpp
  ../v2/sequence/image_sequence_capture_service.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(capture_workflow_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(capture_workflow_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  ${OpenCV_LIBS}
)
add_test(
  NAME capture_workflow_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:capture_workflow_controller_test>
)
set_tests_properties(capture_workflow_controller_test PROPERTIES TIMEOUT 15)

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

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Qml Quick Test)

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
    "PATH=$<TARGET_FILE_DIR:training_service_protocol_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:training_service_protocol_test>
)
set_tests_properties(training_service_protocol_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  training_service_protocol_test
  training_service_protocol_test
)

qt_add_executable(training_real_backend_probe
  training_real_backend_probe.cpp
  ../desktop_app/json_persistence.cpp
  ../v2/training/training_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/operation/operation_coordinator.cpp
)
target_include_directories(training_real_backend_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(training_real_backend_probe PRIVATE
  Qt6::Core
)
add_test(
  NAME training_real_backend_probe
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:training_real_backend_probe>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:training_real_backend_probe>
)
set_tests_properties(training_real_backend_probe PROPERTIES TIMEOUT 150)
opendss_configure_offscreen_test(
  training_real_backend_probe
  training_real_backend_probe
)

qt_add_executable(training_controller_test
  training_controller_test.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../desktop_app/pipeline_runner.cpp
  ../inference/onnx_inference_adapter.cpp
  ../v2/training/training_controller.cpp
  ../v2/training/training_service.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/model/model_library_controller.cpp
  ../v2/model/model_load_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_compile_definitions(training_controller_test PRIVATE
  OPENDSS_TEST_REPOSITORY_ROOT="${CMAKE_SOURCE_DIR}/../.."
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_include_directories(training_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(training_controller_test PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_custom_command(TARGET training_controller_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    "$<TARGET_FILE_DIR:training_controller_test>"
  VERBATIM
)
add_test(
  NAME training_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=${ONNXRUNTIME_DIR}/lib\\;$<TARGET_FILE_DIR:training_controller_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:training_controller_test>
)
set_tests_properties(training_controller_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  training_controller_test
  training_controller_test
)

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
  ../v2/hardware/daq_output.cpp
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

qt_add_executable(daq_controller_test
  daq_controller_test.cpp
  ../daq_trigger.cpp
  ../v2/hardware/daq_controller.cpp
  ../v2/hardware/daq_output.cpp
  ../v2/hardware/daq_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(daq_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(daq_controller_test PRIVATE
  Qt6::Core
)
add_test(
  NAME daq_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:daq_controller_test>
)
set_tests_properties(daq_controller_test PROPERTIES TIMEOUT 15)

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
  Qt6::Test
)
add_test(
  NAME camera_service_contract_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:camera_service_contract_test>
)
set_tests_properties(camera_service_contract_test PROPERTIES TIMEOUT 15)

qt_add_executable(opendss_camera_pipeline_characterization_test
  camera_pipeline_characterization_test.cpp
  ../detection/droplet_detector_adapters.cpp
  ../event_detector.cpp
  ../fast_event_detector.cpp
  ../v2/camera/camera_controller.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(opendss_camera_pipeline_characterization_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(opendss_camera_pipeline_characterization_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  ${OpenCV_LIBS}
)
add_test(
  NAME opendss_camera_pipeline_characterization_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:opendss_camera_pipeline_characterization_test>
)
set_tests_properties(opendss_camera_pipeline_characterization_test PROPERTIES
  TIMEOUT 45
)

qt_add_executable(opendss_real_dcam_pipeline_headless
  real_dcam_pipeline_headless.cpp
  ../dcam_camera.cpp
  ../detection/droplet_detector_adapters.cpp
  ../event_detector.cpp
  ../fast_event_detector.cpp
  ../v2/camera/dcam_camera_device.cpp
)
set_target_properties(opendss_real_dcam_pipeline_headless PROPERTIES
  WIN32_EXECUTABLE FALSE
)
target_include_directories(opendss_real_dcam_pipeline_headless PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${DCAM_SDK_DIR}/inc
  ${OpenCV_INCLUDE_DIRS}
)
target_link_directories(opendss_real_dcam_pipeline_headless PRIVATE
  ${DCAM_SDK_DIR}/lib/win64
)
target_link_libraries(opendss_real_dcam_pipeline_headless PRIVATE
  Qt6::Core
  dcamapi
  ${OpenCV_LIBS}
)

qt_add_executable(opendss_dbg019_sequence_persistence_headless
  dbg019_sequence_persistence_headless.cpp
  ../dcam_camera.cpp
  ../desktop_app/json_persistence.cpp
  ../detection/droplet_detector_adapters.cpp
  ../event_detector.cpp
  ../fast_event_detector.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/dcam_camera_device.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/sequence/image_sequence_capture_service.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
find_package(TIFF REQUIRED)
set_target_properties(opendss_dbg019_sequence_persistence_headless PROPERTIES
  WIN32_EXECUTABLE FALSE
)
target_compile_definitions(opendss_dbg019_sequence_persistence_headless PRIVATE
  TIFF_DISABLE_DEPRECATED
)
target_include_directories(opendss_dbg019_sequence_persistence_headless PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
  ${OpenCV_INCLUDE_DIRS}
)
target_link_directories(opendss_dbg019_sequence_persistence_headless PRIVATE
  ${DCAM_SDK_DIR}/lib/win64
)
target_link_libraries(opendss_dbg019_sequence_persistence_headless PRIVATE
  Qt6::Core
  Qt6::Gui
  dcamapi
  TIFF::TIFF
  ${OpenCV_LIBS}
)

qt_add_executable(camera_controller_test
  camera_controller_test.cpp
  ../v2/camera/camera_controller.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(camera_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(camera_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::Test
)
add_test(
  NAME camera_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:camera_controller_test>
)
set_tests_properties(camera_controller_test PROPERTIES TIMEOUT 15)

qt_add_executable(camera_preview_image_provider_test
  camera_preview_image_provider_test.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/frame_conversion.cpp
)
target_include_directories(camera_preview_image_provider_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(camera_preview_image_provider_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
)
add_test(
  NAME camera_preview_image_provider_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:camera_preview_image_provider_test>
)
set_tests_properties(camera_preview_image_provider_test PROPERTIES TIMEOUT 15)

qt_add_executable(dcam_camera_device_test
  dcam_camera_device_test.cpp
  fakes/fake_dcam_camera.cpp
  ../dcam_camera.cpp
  ../v2/camera/dcam_camera_device.cpp
)
target_include_directories(dcam_camera_device_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(dcam_camera_device_test PRIVATE
  Qt6::Core
  ${OpenCV_LIBS}
)
add_test(
  NAME dcam_camera_device_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:opencv_core>\\;$ENV{PATH}"
    $<TARGET_FILE:dcam_camera_device_test>
)
set_tests_properties(dcam_camera_device_test PROPERTIES TIMEOUT 15)

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
  ../v2/camera/frame_conversion.cpp
  ../v2/camera/single_image_capture_service.cpp
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

qt_add_executable(single_image_capture_controller_test
  single_image_capture_controller_test.cpp
  ../v2/camera/camera_controller.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/camera/single_image_capture_controller.cpp
  ../v2/camera/single_image_capture_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(single_image_capture_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(single_image_capture_controller_test PRIVATE
  Qt6::Core
  Qt6::Concurrent
  Qt6::Gui
  Qt6::Quick
  Qt6::Test
)
add_test(
  NAME single_image_capture_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:single_image_capture_controller_test>
)
set_tests_properties(single_image_capture_controller_test PROPERTIES TIMEOUT 15)

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
    "PATH=$<TARGET_FILE_DIR:sequence_test_service_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:sequence_test_service_test>
)
set_tests_properties(sequence_test_service_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  sequence_test_service_test
  sequence_test_service_test
)

qt_add_executable(sequence_test_production_model_probe
  sequence_test_production_model_probe.cpp
  ../crops/crop_service.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/decision/decision_service.cpp
  ../v2/model/model_load_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/routing/observed_route_tracker.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/sequence_test/sequence_test_service.cpp
)
target_compile_definitions(sequence_test_production_model_probe PRIVATE
  OPENDSS_MODEL_LOAD_NO_PIPELINE
)
target_include_directories(sequence_test_production_model_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(sequence_test_production_model_probe PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
opendss_deploy_qualified_cuda(sequence_test_production_model_probe)

qt_add_executable(sequence_test_controller_test
  sequence_test_controller_test.cpp
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
  ../v2/sequence_test/sequence_test_controller.cpp
  ../v2/sequence_test/sequence_test_service.cpp
)
target_include_directories(sequence_test_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(sequence_test_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME sequence_test_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:sequence_test_controller_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:sequence_test_controller_test>
)
set_tests_properties(sequence_test_controller_test PROPERTIES
  TIMEOUT 30
)
opendss_configure_offscreen_test(
  sequence_test_controller_test
  sequence_test_controller_test
)

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
  ../v2/sequence/sequence_manifest_v2.cpp
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
    "PATH=$<TARGET_FILE_DIR:live_sorting_service_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:live_sorting_service_test>
)
set_tests_properties(live_sorting_service_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  live_sorting_service_test
  live_sorting_service_test
)

qt_add_executable(live_sorting_controller_test
  live_sorting_controller_test.cpp
  ../crops/crop_service.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../desktop_app/pipeline_runner.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/camera/camera_controller.cpp
  ../v2/camera/camera_preview_image_provider.cpp
  ../v2/camera/camera_service.cpp
  ../v2/camera/frame_conversion.cpp
  ../v2/decision/decision_service.cpp
  ../v2/live/live_sorting_controller.cpp
  ../v2/live/live_sorting_service.cpp
  ../v2/model/model_load_service.cpp
  ../v2/operation/operation_coordinator.cpp
  ../v2/routing/observed_route_tracker.cpp
  ../v2/run/run_manifest_v2.cpp
  ../v2/run/run_writer_v2.cpp
  ../v2/sequence/sequence_manifest_v2.cpp
  ../v2/state/application_state_store.cpp
)
target_include_directories(live_sorting_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${DCAM_SDK_DIR}/inc
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(live_sorting_controller_test PRIVATE
  opendss_runtime
  Qt6::Core
  Qt6::Gui
  Qt6::Quick
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME live_sorting_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:live_sorting_controller_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:live_sorting_controller_test>
)
set_tests_properties(live_sorting_controller_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  live_sorting_controller_test
  live_sorting_controller_test
)

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
add_test(
  NAME runs_results_controller_removal_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{PATH}"
    $<TARGET_FILE:runs_results_controller_test>
    --runs-removal
)
set_tests_properties(runs_results_controller_removal_test PROPERTIES TIMEOUT 30)

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
    "PATH=$<TARGET_FILE_DIR:model_test_summary_v2_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:model_test_summary_v2_test>
)
set_tests_properties(model_test_summary_v2_test PROPERTIES TIMEOUT 15)
opendss_configure_offscreen_test(
  model_test_summary_v2_test
  model_test_summary_v2_test
)

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
    "PATH=$<TARGET_FILE_DIR:model_test_writer_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:model_test_writer_test>
)
set_tests_properties(model_test_writer_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  model_test_writer_test
  model_test_writer_test
)

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
    "PATH=$<TARGET_FILE_DIR:model_test_service_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:model_test_service_test>
)
set_tests_properties(model_test_service_test PROPERTIES TIMEOUT 30)
opendss_configure_offscreen_test(
  model_test_service_test
  model_test_service_test
)
opendss_deploy_qualified_cuda(model_test_service_test)

if(NOT DEFINED OPENDSS_TEST_INSTALLER_PYTHON)
  set(_opendss_test_gpu_python
    "$ENV{LOCALAPPDATA}/OpenDSS/training-venv-gpu/Scripts/python.exe"
  )
  set(_opendss_test_cpu_python
    "$ENV{LOCALAPPDATA}/OpenDSS/training-venv-cpu/Scripts/python.exe"
  )
  if(EXISTS "${_opendss_test_gpu_python}")
    set(_opendss_test_default_python "${_opendss_test_gpu_python}")
  else()
    set(_opendss_test_default_python "${_opendss_test_cpu_python}")
  endif()
  set(OPENDSS_TEST_INSTALLER_PYTHON
    "${_opendss_test_default_python}"
    CACHE FILEPATH "Installer-owned OpenDSS Python used by Model Test validation"
  )
endif()
if(NOT EXISTS "${OPENDSS_TEST_INSTALLER_PYTHON}")
  message(FATAL_ERROR
    "Model Test Python validation requires the installer-owned OpenDSS runtime at "
    "${OPENDSS_TEST_INSTALLER_PYTHON}"
  )
endif()
add_test(
  NAME model_test_batch_backend_python_test
  COMMAND ${CMAKE_COMMAND} -E env
    --unset=PYTHONPATH
    --unset=PYTHONHOME
    "PYTHONNOUSERSITE=1"
    "PIP_NO_INDEX=1"
    "HF_HUB_OFFLINE=1"
    "TRANSFORMERS_OFFLINE=1"
    "PATH=$<TARGET_FILE_DIR:qt_offscreen_platform_smoke_test>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    "${OPENDSS_TEST_INSTALLER_PYTHON}" -I -m unittest discover
    -s "${CMAKE_CURRENT_SOURCE_DIR}/../../../training/python/tests"
    -p "test_model_test_batch_backend.py"
)
set_tests_properties(model_test_batch_backend_python_test PROPERTIES TIMEOUT 60)
opendss_configure_offscreen_test(
  model_test_batch_backend_python_test
  qt_offscreen_platform_smoke_test
)

add_test(
  NAME convert_pytorch_model_package_test
  COMMAND ${CMAKE_COMMAND} -E env
    --unset=PYTHONPATH
    --unset=PYTHONHOME
    "PYTHONNOUSERSITE=1"
    "PIP_NO_INDEX=1"
    "${OPENDSS_TEST_INSTALLER_PYTHON}" -I
    "${CMAKE_CURRENT_SOURCE_DIR}/test_convert_pytorch_model_package.py"
)
set_tests_properties(convert_pytorch_model_package_test PROPERTIES TIMEOUT 120)

qt_add_executable(model_test_production_model_probe
  model_test_production_model_probe.cpp
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
target_compile_definitions(model_test_production_model_probe PRIVATE
  OPENDSS_MODEL_LOAD_NO_PIPELINE
)
target_include_directories(model_test_production_model_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(model_test_production_model_probe PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
opendss_deploy_qualified_cuda(model_test_production_model_probe)
add_test(
  NAME model_test_production_model_representative_test
  COMMAND ${CMAKE_COMMAND} -E env
    --unset=PYTHONPATH
    --unset=PYTHONHOME
    "PYTHONNOUSERSITE=1"
    "PIP_NO_INDEX=1"
    "HF_HUB_OFFLINE=1"
    "TRANSFORMERS_OFFLINE=1"
    "PATH=$<TARGET_FILE_DIR:model_test_production_model_probe>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$ENV{SystemRoot}/System32\\;$ENV{SystemRoot}"
    $<TARGET_FILE:model_test_production_model_probe>
    --dataset-manifest
    "C:/Users/goals/OneDrive/Documents/OpenDropletSortingSuite/datasets/prepared/droplet_target_nontarget_3class_starter/metadata/dataset_manifest.json"
    --model-registry
    "C:/Users/goals/OneDrive/Documents/OpenDropletSortingSuite/models/model_registry.json"
    --python
    "${OPENDSS_TEST_INSTALLER_PYTHON}"
    --working-directory
    "$ENV{LOCALAPPDATA}/OpenDSS"
)
set_tests_properties(
  model_test_production_model_representative_test
  PROPERTIES
    RUN_SERIAL TRUE
    TIMEOUT 45
)
opendss_configure_offscreen_test(
  model_test_production_model_representative_test
  model_test_production_model_probe
)

qt_add_executable(model_test_controller_test
  model_test_controller_test.cpp
  ../desktop_app/app_utils.cpp
  ../desktop_app/json_persistence.cpp
  ../desktop_app/model_registry_service.cpp
  ../inference/onnx_inference_adapter.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
  ../v2/dataset/dataset_manifest_v2.cpp
  ../v2/model/model_load_service.cpp
  ../v2/model_test/model_test_controller.cpp
  ../v2/model_test/model_test_service.cpp
  ../v2/model_test/model_test_summary_v2.cpp
  ../v2/model_test/model_test_writer.cpp
  ../v2/operation/operation_coordinator.cpp
)
file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/OpenDSS" OPENDSS_TEST_WORKING_DIRECTORY)
target_compile_definitions(model_test_controller_test PRIVATE
  OPENDSS_TEST_RUNTIME_DIR="${CMAKE_CURRENT_SOURCE_DIR}/.."
  OPENDSS_TEST_INSTALLER_PYTHON="${OPENDSS_TEST_INSTALLER_PYTHON}"
  OPENDSS_TEST_WORKING_DIRECTORY="${OPENDSS_TEST_WORKING_DIRECTORY}"
)
target_include_directories(model_test_controller_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../desktop_app
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(model_test_controller_test PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)
add_test(
  NAME model_test_controller_test
  COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:desktop_app>\\;$<TARGET_FILE_DIR:Qt6::Core>\\;$<TARGET_FILE_DIR:Qt6::Gui>\\;$ENV{PATH}"
    $<TARGET_FILE:model_test_controller_test>
)
set_tests_properties(model_test_controller_test PROPERTIES TIMEOUT 30)

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
