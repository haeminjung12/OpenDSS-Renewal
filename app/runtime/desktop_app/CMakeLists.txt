cmake_minimum_required(VERSION 3.19)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Path to vcpkg")
  if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
      set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    endif()
  endif()
  project(desktop_app LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(DCAM_SDK_DIR "" CACHE PATH "Path to Hamamatsu DCAM SDK (dcamsdk4)")
set(ONNXRUNTIME_DIR "" CACHE PATH "Path to ONNX Runtime")
set(NIDAQMX_DIR "" CACHE PATH "Path to NI-DAQmx (optional)")
option(ENABLE_NIDAQMX "Enable NI-DAQmx trigger support" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

if (ONNXRUNTIME_DIR)
  if (NOT ONNXRUNTIME_INCLUDE_DIR)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h PATHS "${ONNXRUNTIME_DIR}/include" NO_DEFAULT_PATH)
  endif()
  if (NOT ONNXRUNTIME_LIB)
    find_library(ONNXRUNTIME_LIB onnxruntime PATHS "${ONNXRUNTIME_DIR}/lib" "${ONNXRUNTIME_DIR}/lib64" NO_DEFAULT_PATH)
  endif()
else()
  if (NOT ONNXRUNTIME_INCLUDE_DIR)
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h)
  endif()
  if (NOT ONNXRUNTIME_LIB)
    find_library(ONNXRUNTIME_LIB onnxruntime)
  endif()
endif()
if (NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIB)
  message(FATAL_ERROR "ONNX Runtime not found. Set ONNXRUNTIME_DIR to a valid install path.")
endif()

set(OPENDSS_RUNTIME_SOURCES
  ../dataset_capture_session.cpp
  ../cli_runner.cpp
  ../dcam_camera.cpp
  ../detection/droplet_detector_adapters.cpp
  ../event_detector.cpp
  ../daq_trigger.cpp
  ../fast_event_detector.cpp
  ../metadata_loader.cpp
  ../onnx_classifier.cpp
)

add_library(opendss_runtime STATIC
  ${OPENDSS_RUNTIME_SOURCES}
)

target_include_directories(opendss_runtime
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
  PRIVATE
    ${DCAM_SDK_DIR}/inc
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

add_executable(desktop_app
  main.cpp
  app_options.cpp
  app_options.h
  app_paths.cpp
  app_paths.h
  app_context.cpp
  app_context.h
  main_window.cpp
  main_window.h
  background_task_registry.cpp
  background_task_registry.h
  app_utils.cpp
  app_utils.h
  collection_postprocessor.cpp
  collection_postprocessor.h
  live_data_collection_writer.cpp
  live_data_collection_writer.h
  live_log_writer.cpp
  live_log_writer.h
  json_persistence.cpp
  json_persistence.h
  model_registry_service.cpp
  model_registry_service.h
  sequence_summary_writer.cpp
  sequence_summary_writer.h
  crash_handler.cpp
  crash_handler.h
  camera_workspace_controller.cpp
  camera_workspace_controller.h
  dataset_workspace_controller.cpp
  dataset_workspace_controller.h
  reports_workspace_controller.cpp
  reports_workspace_controller.h
  settings_workspace_controller.cpp
  settings_workspace_controller.h
  validator_workspace_controller.cpp
  validator_workspace_controller.h
  camera_worker.cpp
  camera_worker.h
  dcam_controller.cpp
  frame_grabber.cpp
  icons.cpp
  icons.h
  object_names.h
  pipeline_runner.cpp
  ../inference/onnx_inference_adapter.cpp
  ../v2/model/model_load_service.cpp
  theme.cpp
  theme.h
  widget_helpers.cpp
  widget_helpers.h
  workspace_camera.cpp
  workspace_camera.h
  workspace_model.cpp
  workspace_model.h
  workspace_dataset.cpp
  workspace_dataset.h
  workspace_validator.cpp
  workspace_validator.h
  workspace_reports.cpp
  workspace_reports.h
  workspace_settings.cpp
  workspace_settings.h
  stats_figure_window.cpp
  stats_figure_window.h
  dataset_labeler_dialog.cpp
  dataset_labeler_dialog.h
  image_validation_dialog.cpp
  image_validation_dialog.h
  viewer_window.cpp
  viewer_window.h
  zoom_image_view.cpp
  zoom_image_view.h
  resources/desktop_app_resources.qrc
  styles/default_theme.qss
  styles/opendss-dark.qss
  widgets/collapsible_section.cpp
  widgets/collapsible_section.h
  widgets/metric_tile.cpp
  widgets/metric_tile.h
  widgets/panel_frame.cpp
  widgets/panel_frame.h
  widgets/status_chip.cpp
  widgets/status_chip.h
)

if (WIN32)
  set_target_properties(desktop_app PROPERTIES WIN32_EXECUTABLE ON)
  if (TARGET Qt6::EntryPoint)
    target_link_libraries(desktop_app PRIVATE Qt6::EntryPoint)
  elseif (TARGET Qt6::WinMain)
    target_link_libraries(desktop_app PRIVATE Qt6::WinMain)
  endif()
endif()

set_target_properties(desktop_app PROPERTIES
  OUTPUT_NAME OpenDSS
)

target_include_directories(desktop_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${DCAM_SDK_DIR}/inc
  ${ONNXRUNTIME_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(desktop_app PRIVATE
  ${DCAM_SDK_DIR}/lib/win64
)

target_link_libraries(desktop_app PRIVATE
  Qt6::Widgets
  opendss_runtime
  dcamapi
  ${ONNXRUNTIME_LIB}
  ${OpenCV_LIBS}
)

if (WIN32)
  set(ONNXRUNTIME_DLL_CANDIDATES "")
  if (ONNXRUNTIME_DIR)
    list(APPEND ONNXRUNTIME_DLL_CANDIDATES
      "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
      "${ONNXRUNTIME_DIR}/lib64/onnxruntime.dll"
      "${ONNXRUNTIME_DIR}/bin/onnxruntime.dll"
    )
  endif()
  get_filename_component(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_LIB}" DIRECTORY)
  list(APPEND ONNXRUNTIME_DLL_CANDIDATES
    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll"
    "${ONNXRUNTIME_LIB_DIR}/../bin/onnxruntime.dll"
  )

  set(ONNXRUNTIME_DLL "")
  foreach(candidate IN LISTS ONNXRUNTIME_DLL_CANDIDATES)
    if (EXISTS "${candidate}")
      set(ONNXRUNTIME_DLL "${candidate}")
      break()
    endif()
  endforeach()
  if (NOT ONNXRUNTIME_DLL)
    message(FATAL_ERROR "onnxruntime.dll not found beside ONNX Runtime import library. Set ONNXRUNTIME_DIR to the runtime that should be deployed.")
  endif()

add_custom_command(TARGET desktop_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../models/cuda_inference_readiness.json"
            "$<TARGET_FILE_DIR:desktop_app>/cuda_inference_readiness.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ONNXRUNTIME_DLL}" $<TARGET_FILE_DIR:desktop_app>
    COMMENT "Deploying ONNX Runtime DLL from ${ONNXRUNTIME_DLL}"
  )

  if (ONNXRUNTIME_DIR)
    set(ONNX_PROVIDER_DLLS
      "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
      "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_cuda.dll"
      "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_tensorrt.dll"
    )
    foreach(onnx_dll IN LISTS ONNX_PROVIDER_DLLS)
      if (EXISTS "${onnx_dll}")
        add_custom_command(TARGET desktop_app POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${onnx_dll}" $<TARGET_FILE_DIR:desktop_app>
        )
      endif()
    endforeach()
  endif()
endif()

set(RUNTIME_MODELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../models")
if (EXISTS "${RUNTIME_MODELS_DIR}")
  add_custom_command(TARGET desktop_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:desktop_app>/models"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:desktop_app>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RUNTIME_MODELS_DIR}/model_registry.json"
      "$<TARGET_FILE_DIR:desktop_app>/models/model_registry.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RUNTIME_MODELS_DIR}/trainer_hyperparameters.schema.json"
      "$<TARGET_FILE_DIR:desktop_app>/models/trainer_hyperparameters.schema.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${RUNTIME_MODELS_DIR}/templates"
      "$<TARGET_FILE_DIR:desktop_app>/models/templates"
    COMMENT "Deploying active MobileNetV3-Small and EfficientNet-B0 model bundles"
  )
endif()

set(TRAINING_HELPERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../training/python")
if (EXISTS "${TRAINING_HELPERS_DIR}/droplet_trainer/__main__.py")
  add_custom_command(TARGET desktop_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${TRAINING_HELPERS_DIR}"
      "$<TARGET_FILE_DIR:desktop_app>/training/python"
  )
endif()

if (BUILD_TESTING)
  add_subdirectory(../tests ${CMAKE_CURRENT_BINARY_DIR}/tests)
endif()

if (ENABLE_NIDAQMX)
  find_path(NIDAQMX_INCLUDE_DIR NIDAQmx.h
    PATHS
      "${NIDAQMX_DIR}/include"
      "${NIDAQMX_DIR}/Include"
      "${NIDAQMX_DIR}/ExternalCompilerSupport/C/include"
      "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/include"
      "C:/Program Files/National Instruments/NI-DAQ/DAQmx ANSI C Dev/include"
      "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev/include"
  )
  find_library(NIDAQMX_LIBRARY NAMES NIDAQmx
    PATHS
      "${NIDAQMX_DIR}/lib/x64/msvc"
      "${NIDAQMX_DIR}/lib64/msvc"
      "${NIDAQMX_DIR}/lib/msvc"
      "${NIDAQMX_DIR}/ExternalCompilerSupport/C/lib64/msvc"
      "${NIDAQMX_DIR}/ExternalCompilerSupport/C/lib/msvc"
      "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C/lib64/msvc"
      "C:/Program Files/National Instruments/NI-DAQ/DAQmx ANSI C Dev/lib/msvc"
      "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev/lib/x64/msvc"
      "C:/Program Files (x86)/National Instruments/NI-DAQ/DAQmx ANSI C Dev/lib/msvc"
  )
  if (NIDAQMX_INCLUDE_DIR AND NIDAQMX_LIBRARY)
    message(STATUS "NI-DAQmx enabled: include='${NIDAQMX_INCLUDE_DIR}', library='${NIDAQMX_LIBRARY}'")
    target_compile_definitions(opendss_runtime PRIVATE HAVE_NIDAQMX=1)
    target_include_directories(opendss_runtime PRIVATE ${NIDAQMX_INCLUDE_DIR})
    target_link_libraries(opendss_runtime PUBLIC ${NIDAQMX_LIBRARY})
    target_compile_definitions(desktop_app PRIVATE HAVE_NIDAQMX=1)
    target_include_directories(desktop_app PRIVATE ${NIDAQMX_INCLUDE_DIR})
    target_link_libraries(desktop_app PRIVATE ${NIDAQMX_LIBRARY})
  else()
    set(NIDAQMX_MISSING_REASON "")
    if (NOT NIDAQMX_INCLUDE_DIR)
      string(APPEND NIDAQMX_MISSING_REASON " missing NIDAQmx.h")
    endif()
    if (NOT NIDAQMX_LIBRARY)
      string(APPEND NIDAQMX_MISSING_REASON " missing NIDAQmx.lib")
    endif()
    message(WARNING "NI-DAQmx requested but not enabled:${NIDAQMX_MISSING_REASON}. Set NIDAQMX_DIR to the NI-DAQmx C API root or install NI-DAQmx ANSI C support; building with stub trigger.")
  endif()
else()
  message(STATUS "NI-DAQmx disabled by ENABLE_NIDAQMX=OFF; building with stub trigger.")
endif()
