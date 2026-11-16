#pragma once

// Shared helpers for the JSON content loaders (gameplay.json, *.entities.json).
// Include only from .cpp files: it pulls in the (large) nlohmann/json header, which reaches
// the engine through the neowolf_json CMake target as a SYSTEM include.

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Reads a whole file (UTF-8 path). Appends "Unable to open <what>: <path>" to errors on failure.
bool ReadTextFile(const std::string& filePath, const std::string& what, std::string& outText,
                  std::vector<std::string>& errors);

// Parses JSON text. Syntax errors and duplicate keys in one object are errors (a duplicate key
// would otherwise silently override the first one). Messages start with `sourceName`.
bool ParseJsonText(const std::string& text, const std::string& sourceName, nlohmann::json& outDocument,
                   std::vector<std::string>& errors);

// A short description of a JSON value for error messages: 35, 2.5, "text", true, null,
// an object, an array.
std::string DescribeJsonValue(const nlohmann::json& value);
