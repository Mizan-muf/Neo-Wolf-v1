#include "engine/assets/ContentConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
std::string Trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

bool LoadKeyValueConfig(
    const std::string& filePath,
    std::unordered_map<std::string, std::string>& outValues,
    std::vector<std::string>& errors) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        errors.push_back("Unable to open config: " + filePath);
        return false;
    }

    outValues.clear();
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        const std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos) {
            errors.push_back(filePath + ":" + std::to_string(lineNumber) + " missing '='");
            continue;
        }

        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = Trim(trimmed.substr(eq + 1));
        if (key.empty() || value.empty()) {
            errors.push_back(filePath + ":" + std::to_string(lineNumber) + " invalid key/value");
            continue;
        }

        outValues[key] = value;
    }

    return errors.empty();
}

bool RequireKey(
    const std::unordered_map<std::string, std::string>& values,
    const std::string& key,
    std::string& outValue,
    const std::string& filePath,
    std::vector<std::string>& errors) {
    const auto it = values.find(key);
    if (it == values.end()) {
        errors.push_back(filePath + " missing required key: " + key);
        return false;
    }
    outValue = it->second;
    return true;
}
} // namespace

bool LoadWallTextureAssignments(
    const std::string& filePath,
    WallTextureAssignments& outAssignments,
    std::vector<std::string>& errors) {
    std::unordered_map<std::string, std::string> values;
    if (!LoadKeyValueConfig(filePath, values, errors)) {
        return false;
    }

    bool ok = true;
    ok = RequireKey(values, "wall", outAssignments.wall, filePath, errors) && ok;
    ok = RequireKey(values, "wall_alt_a", outAssignments.wallAltA, filePath, errors) && ok;
    ok = RequireKey(values, "wall_alt_b", outAssignments.wallAltB, filePath, errors) && ok;
    ok = RequireKey(values, "door", outAssignments.door, filePath, errors) && ok;
    ok = RequireKey(values, "floor", outAssignments.floor, filePath, errors) && ok;
    ok = RequireKey(values, "ceiling", outAssignments.ceiling, filePath, errors) && ok;
    return ok;
}

bool LoadSpriteTextureDefinitions(
    const std::string& filePath,
    SpriteTextureDefinitions& outDefinitions,
    std::vector<std::string>& errors) {
    std::unordered_map<std::string, std::string> values;
    if (!LoadKeyValueConfig(filePath, values, errors)) {
        return false;
    }

    bool ok = true;
    ok = RequireKey(values, "enemy", outDefinitions.enemy, filePath, errors) && ok;
    ok = RequireKey(values, "pickup", outDefinitions.pickup, filePath, errors) && ok;
    ok = RequireKey(values, "decoration", outDefinitions.decoration, filePath, errors) && ok;
    return ok;
}
