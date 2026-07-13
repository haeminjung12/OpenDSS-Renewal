#include "app_options.h"

#include "pipeline_runner.h"
#include "app_types.h"

#include <QString>

#include <string>

AppOptions parseAppOptions(int argc, char* argv[]) {
    AppOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg.find("--verify-camera-workspace") != std::string::npos) {
            options.verifyCameraWorkspace = true;
        }
        if (arg.find("--verify-daq-settings") != std::string::npos) {
            options.verifyDaqSettings = true;
        }
        if (arg.find("--verify-direct-daq-manual-trigger") != std::string::npos) {
            options.verifyDirectDaqManualTrigger = true;
        }
        if (arg.find("--verify-live-view-manual-trigger") != std::string::npos) {
            options.verifyLiveViewManualTrigger = true;
        }
        if (arg.find("--verify-validation-workspace") != std::string::npos) {
            options.verifyValidationWorkspace = true;
        }
        if (arg.find("--no-startup-prompts") != std::string::npos) {
            options.noStartupPrompts = true;
        }
        const std::string workspacePrefix = "--workspace=";
        if (arg == "--workspace" && i + 1 < argc) {
            options.initialWorkspace = QString::fromLocal8Bit(argv[++i]).trimmed().toLower();
        } else if (arg.rfind(workspacePrefix, 0) == 0) {
            options.initialWorkspace =
                QString::fromLocal8Bit(arg.substr(workspacePrefix.size()).c_str()).trimmed().toLower();
        }
        const std::string reviewPrefix = "--dataset-builder-review-manifest=";
        if (arg == "--dataset-builder-review-manifest" && i + 1 < argc) {
            options.datasetBuilderReviewPath = QString::fromLocal8Bit(argv[++i]);
        } else if (arg.rfind(reviewPrefix, 0) == 0) {
            options.datasetBuilderReviewPath = QString::fromLocal8Bit(arg.substr(reviewPrefix.size()).c_str());
        }
    }
    return options;
}
