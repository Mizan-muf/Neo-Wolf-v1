#include "engine/assets/ContentConfig.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>

#include "engine/core/Paths.h"

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
    std::ifstream file(PathFromUtf8(filePath));
    if (!file.is_open()) {
        errors.push_back("Unable to open config: " + filePath);
        return false;
    }

    outValues.clear();
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // CRLF file
        }
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

std::string ExtractJsonNumber(const std::string& content, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = content.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    pos = content.find(':', pos);
    if (pos == std::string::npos) {
        return "";
    }
    ++pos;
    while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t' || content[pos] == '\r' || content[pos] == '\n')) {
        ++pos;
    }
    std::size_t end = pos;
    while (end < content.size() && (std::isdigit(static_cast<unsigned char>(content[end])) || content[end] == '.' || content[end] == '-')) {
        ++end;
    }
    return content.substr(pos, end - pos);
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

bool LoadGameplayDefinitions(
    const std::string& filePath,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors) {
    std::ifstream file(PathFromUtf8(filePath));
    if (!file.is_open()) {
        errors.push_back("Unable to open gameplay definitions: " + filePath);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    const auto parseVal = [&](const std::string& key, float defaultVal) -> float {
        const std::string str = ExtractJsonNumber(content, key);
        if (str.empty()) return defaultVal;
        try {
            return std::stof(str);
        } catch (...) {
            return defaultVal;
        }
    };

    outDefinitions.enemyHealth = static_cast<int>(parseVal("health", static_cast<float>(outDefinitions.enemyHealth)));
    outDefinitions.enemyDamage = static_cast<int>(parseVal("damage", static_cast<float>(outDefinitions.enemyDamage)));
    outDefinitions.enemyMoveSpeed = parseVal("moveSpeed", outDefinitions.enemyMoveSpeed);
    outDefinitions.enemyAlertRadius = parseVal("alertRadius", outDefinitions.enemyAlertRadius);
    outDefinitions.enemyAttackRadius = parseVal("attackRadius", outDefinitions.enemyAttackRadius);
    outDefinitions.enemyAttackRate = parseVal("attackRate", outDefinitions.enemyAttackRate);

    outDefinitions.weaponDamage = static_cast<int>(parseVal("damage", static_cast<float>(outDefinitions.weaponDamage)));
    outDefinitions.weaponFireRate = parseVal("fireRate", outDefinitions.weaponFireRate);
    outDefinitions.weaponRange = parseVal("range", outDefinitions.weaponRange);
    outDefinitions.initialAmmo = static_cast<int>(parseVal("initialAmmo", static_cast<float>(outDefinitions.initialAmmo)));

    outDefinitions.initialHealth = static_cast<int>(parseVal("initialHealth", static_cast<float>(outDefinitions.initialHealth)));
    outDefinitions.playerMoveSpeed = parseVal("moveSpeed", outDefinitions.playerMoveSpeed);
    outDefinitions.playerTurnSpeed = parseVal("turnSpeed", outDefinitions.playerTurnSpeed);
    outDefinitions.playerCollisionRadius = parseVal("collisionRadius", outDefinitions.playerCollisionRadius);

    return true;
}
