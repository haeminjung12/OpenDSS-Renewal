#include "metadata_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

size_t findKey(const std::string& s, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    return s.find(needle);
}

bool extractArraySpan(const std::string& s, size_t keyPos, size_t& outStart, size_t& outEnd) {
    if (keyPos == std::string::npos)
        return false;
    size_t start = s.find('[', keyPos);
    if (start == std::string::npos)
        return false;
    size_t end = s.find(']', start);
    if (end == std::string::npos)
        return false;
    outStart = start + 1;
    outEnd = end;
    return true;
}

bool parseStringArray(const std::string& s, const std::string& key, std::vector<std::string>& out) {
    size_t start = 0, end = 0;
    if (!extractArraySpan(s, findKey(s, key), start, end))
        return false;
    out.clear();
    for (size_t i = start; i < end; ++i) {
        if (s[i] == '"') {
            size_t j = s.find('"', i + 1);
            if (j == std::string::npos || j > end)
                break;
            out.push_back(s.substr(i + 1, j - i - 1));
            i = j;
        }
    }
    return !out.empty();
}

bool parseNumberArray(const std::string& s, const std::string& key, std::vector<double>& out) {
    size_t start = 0, end = 0;
    if (!extractArraySpan(s, findKey(s, key), start, end))
        return false;
    out.clear();
    size_t i = start;
    while (i < end) {
        while (i < end && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ','))
            ++i;
        if (i >= end)
            break;
        char* next = nullptr;
        double v = std::strtod(&s[i], &next);
        if (&s[i] == next)
            break;
        out.push_back(v);
        i = static_cast<size_t>(next - s.data());
    }
    return !out.empty();
}

std::string toLowerAscii(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char c : input) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string trimAscii(const std::string& input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        ++start;
    }
    size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }
    return input.substr(start, end - start);
}

std::string unescapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    bool escaped = false;
    for (char c : input) {
        if (escaped) {
            out.push_back(c);
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool findObjectSpan(const std::string& s, size_t keyPos, size_t& outStart, size_t& outEnd) {
    if (keyPos == std::string::npos)
        return false;
    size_t start = s.find('{', keyPos);
    if (start == std::string::npos)
        return false;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                outStart = start + 1;
                outEnd = i;
                return true;
            }
        }
    }
    return false;
}

bool parseStringMap(const std::string& s, const std::string& key,
                    std::vector<std::pair<std::string, std::string>>& out) {
    size_t start = 0, end = 0;
    if (!findObjectSpan(s, findKey(s, key), start, end))
        return false;
    out.clear();
    size_t i = start;
    while (i < end) {
        while (i < end && s[i] != '"')
            ++i;
        if (i >= end)
            break;
        size_t keyEnd = s.find('"', i + 1);
        if (keyEnd == std::string::npos || keyEnd > end)
            break;
        std::string mapKey = unescapeJsonString(s.substr(i + 1, keyEnd - i - 1));
        i = keyEnd + 1;
        size_t colon = s.find(':', i);
        if (colon == std::string::npos || colon > end)
            break;
        i = colon + 1;
        while (i < end && std::isspace(static_cast<unsigned char>(s[i])))
            ++i;
        if (i >= end || s[i] != '"')
            break;
        size_t valueEnd = s.find('"', i + 1);
        if (valueEnd == std::string::npos || valueEnd > end)
            break;
        std::string mapValue = unescapeJsonString(s.substr(i + 1, valueEnd - i - 1));
        out.push_back({mapKey, mapValue});
        i = valueEnd + 1;
    }
    return !out.empty();
}

bool containsClassId(const Metadata& meta, const std::string& classId) {
    return std::find(meta.classes.begin(), meta.classes.end(), classId) != meta.classes.end();
}

bool isBinaryMetadata(const Metadata& meta) {
    return meta.classes.size() == 2 && containsClassId(meta, "0") && containsClassId(meta, "1");
}
} // namespace

bool LoadMetadata(const std::string& path, Metadata& out, std::string& err) {
    std::string content = readFile(path);
    if (content.empty()) {
        err = "failed to read metadata";
        return false;
    }

    std::vector<std::string> classes;
    std::vector<double> inputSize;
    std::vector<double> mean;
    std::vector<double> stddev;

    if (!parseStringArray(content, "classes", classes)) {
        err = "metadata missing classes";
        return false;
    }
    if (!parseNumberArray(content, "input_size", inputSize)) {
        err = "metadata missing input_size";
        return false;
    }
    if (!parseNumberArray(content, "mean", mean)) {
        err = "metadata missing normalization mean";
        return false;
    }
    if (!parseNumberArray(content, "std", stddev)) {
        err = "metadata missing normalization std";
        return false;
    }

    out.classes = classes;
    out.displayLabels.assign(classes.size(), "");
    std::vector<std::pair<std::string, std::string>> labels;
    if (parseStringMap(content, "display_labels", labels)) {
        for (const auto& kv : labels) {
            for (size_t i = 0; i < classes.size(); ++i) {
                if (classes[i] == kv.first) {
                    out.displayLabels[i] = kv.second;
                    break;
                }
            }
        }
    }
    if (inputSize.size() >= 2) {
        out.inputH = static_cast<int>(inputSize[0]);
        out.inputW = static_cast<int>(inputSize[1]);
        out.inputC = inputSize.size() > 2 ? static_cast<int>(inputSize[2]) : 1;
    }
    out.mean.clear();
    out.mean.reserve(mean.size());
    for (double v : mean) {
        out.mean.push_back(static_cast<float>(v));
    }
    out.std.clear();
    out.std.reserve(stddev.size());
    for (double v : stddev) {
        out.std.push_back(static_cast<float>(v));
    }
    return true;
}

std::string DisplayLabelForClassId(const Metadata& meta, const std::string& classId) {
    for (size_t i = 0; i < meta.classes.size(); ++i) {
        if (meta.classes[i] == classId) {
            if (i < meta.displayLabels.size() && !meta.displayLabels[i].empty()) {
                return meta.displayLabels[i];
            }
            return classId;
        }
    }
    return classId;
}

std::string FormatClassForDisplay(const std::string& classId, const std::string& displayLabel) {
    if (displayLabel.empty() || displayLabel == classId) {
        return classId;
    }
    return displayLabel + " (" + classId + ")";
}

bool ResolveTargetClassId(const Metadata& meta, const std::string& targetClassId, const std::string& targetLabel,
                          std::string& resolvedClassId, std::string& resolvedDisplayLabel, std::string& err) {
    resolvedClassId.clear();
    resolvedDisplayLabel.clear();
    err.clear();

    const std::string classId = trimAscii(targetClassId);
    if (!classId.empty()) {
        if (!containsClassId(meta, classId)) {
            err = "target class id '" + classId + "' is not present in metadata classes";
            return false;
        }
        resolvedClassId = classId;
        resolvedDisplayLabel = DisplayLabelForClassId(meta, resolvedClassId);
        return true;
    }

    const std::string label = trimAscii(targetLabel);
    if (!label.empty()) {
        std::vector<std::string> matches;
        const std::string labelLower = toLowerAscii(label);
        for (size_t i = 0; i < meta.classes.size(); ++i) {
            const std::string& candidateId = meta.classes[i];
            std::string candidateDisplay = DisplayLabelForClassId(meta, candidateId);
            if (toLowerAscii(candidateId) == labelLower || toLowerAscii(candidateDisplay) == labelLower ||
                toLowerAscii(FormatClassForDisplay(candidateId, candidateDisplay)) == labelLower) {
                matches.push_back(candidateId);
            }
        }
        std::sort(matches.begin(), matches.end());
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
        if (matches.size() == 1) {
            resolvedClassId = matches.front();
            resolvedDisplayLabel = DisplayLabelForClassId(meta, resolvedClassId);
            return true;
        }
        if (matches.empty()) {
            err = "target label '" + label + "' did not resolve to a metadata class id";
        } else {
            err = "target label '" + label + "' is ambiguous across metadata classes";
        }
        return false;
    }

    if (isBinaryMetadata(meta)) {
        resolvedClassId = "1";
        resolvedDisplayLabel = DisplayLabelForClassId(meta, resolvedClassId);
        return true;
    }

    if (containsClassId(meta, "Single")) {
        resolvedClassId = "Single";
        resolvedDisplayLabel = DisplayLabelForClassId(meta, resolvedClassId);
        return true;
    }

    err = "no target class id supplied and metadata has no binary class id '1' or legacy 'Single' class";
    return false;
}
