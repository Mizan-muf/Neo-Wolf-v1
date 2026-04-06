#pragma once

#include <string>

#include "engine/assets/Texture.h"

namespace TextureLoader {
bool LoadPPM(const std::string& filePath, Texture& outTexture);
void BuildFallbackChecker(Texture& outTexture, int width, int height);
} // namespace TextureLoader
