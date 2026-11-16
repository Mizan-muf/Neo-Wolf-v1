#pragma once

#include <string>
#include <vector>

#include "engine/world/GameplayDefinitions.h"

// Loads game/defs/gameplay.json (schema: docs/CONTENT_DEFINITIONS.md).
//
// Every key is optional; a missing key keeps its default from GameplayDefinitions.h. A file
// that cannot be read, is not valid JSON, repeats a key, has a value of the wrong type or out
// of range, or breaks a cross-field rule (initialHealth <= maxHealth, initialAmmo <= maxAmmo)
// is an error: the function returns false and leaves outDefinitions unchanged. Unknown keys
// are warnings and do not fail the load. Messages start with the file name and name the key,
// e.g. "gameplay.json: weapon.damage must be an integer, got 35.5".
bool LoadGameplayDefinitions(
    const std::string& filePath,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings);

// The same, from JSON text; `sourceName` starts every message.
bool ParseGameplayDefinitions(
    const std::string& jsonText,
    const std::string& sourceName,
    GameplayDefinitions& outDefinitions,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings);

// One-line summary of every value, for the log.
std::string DescribeGameplayDefinitions(const GameplayDefinitions& definitions);
