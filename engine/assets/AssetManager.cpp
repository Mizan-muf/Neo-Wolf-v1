#include "engine/assets/AssetManager.h"

#include "engine/assets/TextureLoader.h"

void AssetManager::Clear() {
    textures_.clear();
}

void AssetManager::SetTexture(const std::string& id, const Texture& texture) {
    textures_[id] = texture;
}

bool AssetManager::LoadTextureFromPPM(
    const std::string& id,
    const std::string& filePath,
    std::vector<std::string>& errors) {
    Texture texture{};
    if (!TextureLoader::LoadPPM(filePath, texture)) {
        errors.push_back("Failed to load texture '" + id + "' from: " + filePath);
        return false;
    }

    textures_[id] = texture;
    return true;
}

const Texture* AssetManager::GetTexture(const std::string& id) const {
    const auto it = textures_.find(id);
    if (it == textures_.end()) {
        return nullptr;
    }
    return &it->second;
}
