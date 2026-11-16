#pragma once

// Reading plain-text PPM (P3) textures, and the checkerboard used when one is missing.

#include <string>

#include "engine/assets/Texture.h"

namespace TextureLoader {
bool LoadPPM(const std::string& filePath, Texture& outTexture);
void BuildFallbackChecker(Texture& outTexture, int width, int height);
}  // namespace TextureLoader
