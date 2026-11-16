#pragma once

// Reads a level's .map file into a Map: one character per cell (. # A B D T X), rows of equal
// width; empty lines and lines starting with ';' are skipped. Every bad row or tile is reported
// in `errors`.

#include <string>
#include <vector>

class Map;

bool LoadMapFromFile(const std::string& filePath, Map& outMap, std::vector<std::string>& errors);
