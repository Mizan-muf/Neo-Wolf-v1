#include "engine/assets/AudioConfig.h"

#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>

#include "engine/core/MathUtils.h"

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

bool LoadAudioEffectDefinitions(
    const std::string& filePath,
    AudioEffectDefinitions& outDefinitions,
    std::vector<std::string>& errors) {
    std::unordered_map<std::string, std::string> values;
    if (!LoadKeyValueConfig(filePath, values, errors)) {
        return false;
    }

    bool ok = true;
    ok = RequireKey(values, "door_open", outDefinitions.doorOpen, filePath, errors) && ok;
    ok = RequireKey(values, "pickup", outDefinitions.pickup, filePath, errors) && ok;
    ok = RequireKey(values, "enemy_alert", outDefinitions.enemyAlert, filePath, errors) && ok;

    const auto weaponIt = values.find("weapon_fire");
    if (weaponIt != values.end()) {
        outDefinitions.weaponFire = weaponIt->second;
    }

    const auto volumeIt = values.find("master_volume");
    if (volumeIt != values.end()) {
        try {
            outDefinitions.masterVolume = Clamp(std::stof(volumeIt->second), 0.0f, 1.0f);
        } catch (...) {
            errors.push_back(filePath + " invalid float value for master_volume: " + volumeIt->second);
            ok = false;
        }
    }

    return ok;
}
