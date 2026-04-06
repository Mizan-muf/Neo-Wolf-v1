#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/assets/Texture.h"

class AssetManager {
public:
    void Clear();

    void SetTexture(const std::string& id, const Texture& texture);
    bool LoadTextureFromPPM(
        const std::string& id,
        const std::string& filePath,
        std::vector<std::string>& errors);

    const Texture* GetTexture(const std::string& id) const;

private:
    std::unordered_map<std::string, Texture> textures_{};
};
