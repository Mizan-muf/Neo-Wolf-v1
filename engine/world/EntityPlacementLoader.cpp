#include "engine/world/EntityPlacementLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

bool ParseType(const std::string& value, PlacementType& outType) {
    if (value == "player") {
        outType = PlacementType::Player;
        return true;
    }
    if (value == "enemy") {
        outType = PlacementType::Enemy;
        return true;
    }
    if (value == "pickup") {
        outType = PlacementType::Pickup;
        return true;
    }
    if (value == "door") {
        outType = PlacementType::Door;
        return true;
    }
    if (value == "trigger") {
        outType = PlacementType::Trigger;
        return true;
    }
    if (value == "decoration") {
        outType = PlacementType::Decoration;
        return true;
    }
    return false;
}
} // namespace

bool LoadEntityPlacementsFromFile(
    const std::string& filePath,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        errors.push_back("Unable to open entity placement file: " + filePath);
        return false;
    }

    outPlacements.clear();
    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::vector<std::string> fields;
        std::stringstream ss(trimmed);
        std::string token;
        while (std::getline(ss, token, ',')) {
            fields.push_back(Trim(token));
        }

        if (fields.size() < 3) {
            errors.push_back(filePath + ":" + std::to_string(lineNumber) + " expected at least 3 CSV fields");
            continue;
        }

        EntityPlacement placement{};
        if (!ParseType(fields[0], placement.type)) {
            errors.push_back(filePath + ":" + std::to_string(lineNumber) + " unknown type: " + fields[0]);
            continue;
        }

        try {
            placement.position.x = std::stof(fields[1]);
            placement.position.y = std::stof(fields[2]);
        } catch (...) {
            errors.push_back(filePath + ":" + std::to_string(lineNumber) + " invalid position values");
            continue;
        }

        if (fields.size() >= 5) {
            try {
                placement.mapX = std::stoi(fields[3]);
                placement.mapY = std::stoi(fields[4]);
            } catch (...) {
                errors.push_back(filePath + ":" + std::to_string(lineNumber) + " invalid map cell values");
                continue;
            }
        }

        outPlacements.push_back(placement);
    }

    return errors.empty();
}
