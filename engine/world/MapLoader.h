#pragma once

#include <string>
#include <vector>

class Map;

bool LoadMapFromFile(const std::string& filePath, Map& outMap, std::vector<std::string>& errors);
