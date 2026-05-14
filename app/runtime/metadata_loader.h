#pragma once

#include <string>
#include <vector>

struct Metadata {
    std::vector<std::string> classes;
    std::vector<std::string> displayLabels;
    int inputH = 0;
    int inputW = 0;
    int inputC = 0;
    std::vector<float> mean;
    std::vector<float> std;
};

bool LoadMetadata(const std::string& path, Metadata& out, std::string& err);
bool ResolveTargetClassId(const Metadata& meta,
                          const std::string& targetClassId,
                          const std::string& targetLabel,
                          std::string& resolvedClassId,
                          std::string& resolvedDisplayLabel,
                          std::string& err);
std::string DisplayLabelForClassId(const Metadata& meta, const std::string& classId);
std::string FormatClassForDisplay(const std::string& classId, const std::string& displayLabel);
