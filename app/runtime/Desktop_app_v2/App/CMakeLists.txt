
target_sources(${CMAKE_PROJECT_NAME} PUBLIC
    "main.cpp"
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
    "../../desktop_app/json_persistence.cpp"
    "../../desktop_app/json_persistence.h"
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml)
