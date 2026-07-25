
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

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
    "../../v2/dataset/dataset_label_controller.cpp"
    "../../v2/dataset/dataset_label_controller.h"
    "../../v2/dataset/dataset_label_service.cpp"
    "../../v2/dataset/dataset_label_service.h"
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
    "../../v2/sequence/sequence_manifest_v2.cpp"
    "../../v2/sequence/sequence_manifest_v2.h"
    "../../v2/sequence/sequence_viewer_controller.cpp"
    "../../v2/sequence/sequence_viewer_controller.h"
    "../../v2/sequence/sequence_viewer_image_provider.cpp"
    "../../v2/sequence/sequence_viewer_image_provider.h"
    "../../v2/sequence/sequence_viewer_model.cpp"
    "../../v2/sequence/sequence_viewer_model.h"
    "../../v2/state/application_state_store.cpp"
    "../../v2/state/application_state_store.h"
    "../../v2/state/domain_state.h"
    "../../v2/training/training_controller.cpp"
    "../../v2/training/training_controller.h"
    "../../v2/training/training_service.cpp"
    "../../v2/training/training_service.h"
    "../../desktop_app/app_utils.cpp"
    "../../desktop_app/app_utils.h"
    "../../desktop_app/json_persistence.cpp"
    "../../desktop_app/json_persistence.h"
    "../../desktop_app/model_registry_service.cpp"
    "../../desktop_app/model_registry_service.h"
    "../../inference/onnx_inference_adapter.cpp"
    "../../inference/onnx_inference_adapter.h"
    "../../metadata_loader.cpp"
    "../../metadata_loader.h"
    "../../onnx_classifier.cpp"
    "../../onnx_classifier.h"
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # The v2 shell prepares the persisted Active Model but never activates or installs one.
    OPENDSS_MODEL_LOAD_NO_PIPELINE
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${ONNXRUNTIME_INCLUDE_DIR}"
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    ${ONNXRUNTIME_LIB}
    ${OpenCV_LIBS})
