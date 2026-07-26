
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent)

set(NIDAQMX_DIR "" CACHE PATH "Path to NI-DAQmx (optional)")
option(ENABLE_NIDAQMX "Enable NI-DAQmx trigger support" ON)

if (ONNXRUNTIME_DIR)
    if (NOT ONNXRUNTIME_INCLUDE_DIR)
        find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
            PATHS "${ONNXRUNTIME_DIR}/include"
            NO_DEFAULT_PATH)
    endif()
    if (NOT ONNXRUNTIME_LIB)
        find_library(ONNXRUNTIME_LIB onnxruntime
            PATHS "${ONNXRUNTIME_DIR}/lib" "${ONNXRUNTIME_DIR}/lib64"
            NO_DEFAULT_PATH)
    endif()
endif()
if (NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIB)
    message(FATAL_ERROR "ONNX Runtime not found. Set ONNXRUNTIME_DIR to a valid install path.")
endif()

target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    "main.cpp"
    "../../crops/crop_service.cpp"
    "../../crops/crop_service.h"
    "../../dcam_camera.cpp"
    "../../dcam_camera.h"
    "../../v2/camera/camera_controller.cpp"
    "../../v2/camera/camera_controller.h"
    "../../v2/camera/camera_device.h"
    "../../v2/camera/camera_preview_image_provider.cpp"
    "../../v2/camera/camera_preview_image_provider.h"
    "../../v2/camera/camera_service.cpp"
    "../../v2/camera/camera_service.h"
    "../../v2/camera/dcam_camera_device.cpp"
    "../../v2/camera/dcam_camera_device.h"
    "../../v2/camera/frame_conversion.cpp"
    "../../v2/camera/frame_conversion.h"
    "../../v2/camera/single_image_capture_controller.cpp"
    "../../v2/camera/single_image_capture_controller.h"
    "../../v2/camera/single_image_capture_service.cpp"
    "../../v2/camera/single_image_capture_service.h"
    "../../v2/dataset/dataset_label_controller.cpp"
    "../../v2/dataset/dataset_label_controller.h"
    "../../v2/dataset/dataset_capture_service.cpp"
    "../../v2/dataset/dataset_capture_service.h"
    "../../v2/dataset/dataset_label_service.cpp"
    "../../v2/dataset/dataset_label_service.h"
    "../../v2/decision/decision_service.cpp"
    "../../v2/decision/decision_service.h"
    "../../v2/hardware/daq_controller.cpp"
    "../../v2/hardware/daq_controller.h"
    "../../v2/hardware/daq_output.cpp"
    "../../v2/hardware/daq_output.h"
    "../../v2/hardware/daq_service.cpp"
    "../../v2/hardware/daq_service.h"
    "../../v2/live/live_sorting_controller.cpp"
    "../../v2/live/live_sorting_controller.h"
    "../../v2/live/live_sorting_service.cpp"
    "../../v2/live/live_sorting_service.h"
    "../../v2/dataset/dataset_manifest_v2.cpp"
    "../../v2/dataset/dataset_manifest_v2.h"
    "../../v2/model/model_library_controller.cpp"
    "../../v2/model/model_library_controller.h"
    "../../v2/model/model_load_service.cpp"
    "../../v2/model/model_load_service.h"
    "../../v2/model_test/model_test_controller.cpp"
    "../../v2/model_test/model_test_controller.h"
    "../../v2/model_test/model_test_service.cpp"
    "../../v2/model_test/model_test_service.h"
    "../../v2/model_test/model_test_summary_v2.cpp"
    "../../v2/model_test/model_test_summary_v2.h"
    "../../v2/model_test/model_test_writer.cpp"
    "../../v2/model_test/model_test_writer.h"
    "../../v2/operation/operation_coordinator.cpp"
    "../../v2/operation/operation_coordinator.h"
    "../../v2/routing/observed_route_tracker.cpp"
    "../../v2/routing/observed_route_tracker.h"
    "../../v2/settings/settings_controller.cpp"
    "../../v2/settings/settings_controller.h"
    "../../v2/settings/settings_repository.cpp"
    "../../v2/settings/settings_repository.h"
    "../../v2/results/run_repository.cpp"
    "../../v2/results/run_repository.h"
    "../../v2/results/runs_results_controller.cpp"
    "../../v2/results/runs_results_controller.h"
    "../../v2/run/run_manifest_v2.cpp"
    "../../v2/run/run_manifest_v2.h"
    "../../v2/run/run_writer_v2.cpp"
    "../../v2/run/run_writer_v2.h"
    "../../v2/sequence/sequence_manifest_v2.cpp"
    "../../v2/sequence/sequence_manifest_v2.h"
    "../../v2/sequence/capture_workflow_controller.cpp"
    "../../v2/sequence/capture_workflow_controller.h"
    "../../v2/sequence/image_sequence_capture_service.cpp"
    "../../v2/sequence/image_sequence_capture_service.h"
    "../../v2/sequence/sequence_viewer_controller.cpp"
    "../../v2/sequence/sequence_viewer_controller.h"
    "../../v2/sequence/sequence_viewer_image_provider.cpp"
    "../../v2/sequence/sequence_viewer_image_provider.h"
    "../../v2/sequence/sequence_viewer_model.cpp"
    "../../v2/sequence/sequence_viewer_model.h"
    "../../v2/sequence_test/sequence_test_controller.cpp"
    "../../v2/sequence_test/sequence_test_controller.h"
    "../../v2/sequence_test/sequence_test_service.cpp"
    "../../v2/sequence_test/sequence_test_service.h"
    "../../v2/state/application_state_store.cpp"
    "../../v2/state/application_state_store.h"
    "../../v2/state/domain_state.h"
    "../../v2/training/training_controller.cpp"
    "../../v2/training/training_controller.h"
    "../../v2/training/training_service.cpp"
    "../../v2/training/training_service.h"
    "../../desktop_app/pipeline_runner.cpp"
    "../../desktop_app/pipeline_runner.h"
    "../../desktop_app/app_utils.cpp"
    "../../desktop_app/app_utils.h"
    "../../desktop_app/json_persistence.cpp"
    "../../desktop_app/json_persistence.h"
    "../../desktop_app/live_frame_dispatcher.h"
    "../../desktop_app/model_registry_service.cpp"
    "../../desktop_app/model_registry_service.h"
    "../../detection/droplet_detector_adapters.cpp"
    "../../detection/droplet_detector_adapters.h"
    "../../daq_trigger.cpp"
    "../../daq_trigger.h"
    "../../event_detector.cpp"
    "../../event_detector.h"
    "../../fast_event_detector.cpp"
    "../../fast_event_detector.h"
    "../../inference/onnx_inference_adapter.cpp"
    "../../inference/onnx_inference_adapter.h"
    "../../metadata_loader.cpp"
    "../../metadata_loader.h"
    "../../onnx_classifier.cpp"
    "../../onnx_classifier.h"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${DCAM_SDK_DIR}/inc"
    "${ONNXRUNTIME_INCLUDE_DIR}"
    ${OpenCV_INCLUDE_DIRS}
)

target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${DCAM_SDK_DIR}/lib/win64"
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    dcamapi
    ${ONNXRUNTIME_LIB}
    ${OpenCV_LIBS})

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
        target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_NIDAQMX=1)
        target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${NIDAQMX_INCLUDE_DIR})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${NIDAQMX_LIBRARY})
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
