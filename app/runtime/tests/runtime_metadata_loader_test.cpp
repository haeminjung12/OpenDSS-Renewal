#include "metadata_loader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    const std::filesystem::path testDir =
        std::filesystem::temp_directory_path() / "opendss_runtime_metadata_loader_test";
    const std::filesystem::path metadataPath = testDir / "metadata.json";

    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir, ec);
    if (ec) {
        std::cerr << "FAIL: could not create test directory: " << ec.message() << '\n';
        return 1;
    }

    {
        std::ofstream out(metadataPath, std::ios::out | std::ios::trunc);
        out << "{\n"
            << "  \"classes\": [\"0\", \"1\"],\n"
            << "  \"display_labels\": {\"0\": \"Waste\", \"1\": \"Hit\"},\n"
            << "  \"input_size\": [64, 64, 1],\n"
            << "  \"mean\": [0.5],\n"
            << "  \"std\": [0.25]\n"
            << "}\n";
    }

    Metadata metadata;
    std::string err;
    if (!require(LoadMetadata(metadataPath.string(), metadata, err),
                 "LoadMetadata should succeed for valid metadata.json")) {
        return 1;
    }
    if (!require(metadata.classes.size() == 2, "metadata should contain two classes"))
        return 1;
    if (!require(metadata.displayLabels.size() == 2, "metadata should contain two display labels"))
        return 1;
    if (!require(metadata.displayLabels[1] == "Hit", "class 1 should resolve to display label Hit"))
        return 1;
    if (!require(metadata.inputH == 64 && metadata.inputW == 64 && metadata.inputC == 1,
                 "input_size should populate H/W/C"))
        return 1;
    if (!require(metadata.mean.size() == 1 && metadata.mean[0] == 0.5f,
                 "mean vector should load the configured normalization mean"))
        return 1;
    if (!require(metadata.std.size() == 1 && metadata.std[0] == 0.25f,
                 "std vector should load the configured normalization std"))
        return 1;
    if (!require(DisplayLabelForClassId(metadata, "1") == "Hit",
                 "DisplayLabelForClassId should return the mapped display label"))
        return 1;
    if (!require(FormatClassForDisplay("1", "Hit") == "Hit (1)",
                 "FormatClassForDisplay should include both label and class id"))
        return 1;

    std::string resolvedClassId;
    std::string resolvedDisplayLabel;
    if (!require(ResolveTargetClassId(metadata, "", "Hit", resolvedClassId, resolvedDisplayLabel, err),
                 "ResolveTargetClassId should resolve explicit display labels")) {
        return 1;
    }
    if (!require(resolvedClassId == "1" && resolvedDisplayLabel == "Hit",
                 "display label resolution should choose class 1"))
        return 1;

    if (!require(ResolveTargetClassId(metadata, "", "", resolvedClassId, resolvedDisplayLabel, err),
                 "binary metadata should default to class id 1 when no target is supplied")) {
        return 1;
    }
    if (!require(resolvedClassId == "1" && resolvedDisplayLabel == "Hit",
                 "default binary target should resolve to class 1 / Hit"))
        return 1;

    if (!require(!ResolveTargetClassId(metadata, "missing", "", resolvedClassId, resolvedDisplayLabel, err),
                 "missing class ids should be rejected")) {
        return 1;
    }
    if (!require(err.find("not present") != std::string::npos,
                 "missing class id error should explain the lookup failure"))
        return 1;

    std::filesystem::remove_all(testDir, ec);
    return 0;
}
