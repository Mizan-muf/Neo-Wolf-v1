#include "engine/assets/AudioConfig.h"

#include <string>
#include <unordered_map>

#include "engine/assets/ContentConfig.h"
#include "engine/core/MathUtils.h"

bool LoadAudioEffectDefinitions(const std::string& filePath, AudioEffectDefinitions& outDefinitions,
                                std::vector<std::string>& errors) {
    std::unordered_map<std::string, std::string> values;
    if (!LoadKeyValueConfig(filePath, values, errors)) {
        return false;
    }

    bool ok = true;
    ok = RequireConfigKey(values, "door_open", outDefinitions.doorOpen, filePath, errors) && ok;
    ok = RequireConfigKey(values, "pickup", outDefinitions.pickup, filePath, errors) && ok;
    ok = RequireConfigKey(values, "enemy_alert", outDefinitions.enemyAlert, filePath, errors) && ok;

    const auto weaponIt = values.find("weapon_fire");
    if (weaponIt != values.end()) {
        outDefinitions.weaponFire = weaponIt->second;
    }

    const auto volumeIt = values.find("master_volume");
    if (volumeIt != values.end()) {
        try {
            outDefinitions.masterVolume = Clamp(std::stof(volumeIt->second), 0.0f, 1.0f);
        } catch (...) {
            errors.push_back(filePath + " invalid float value for master_volume: " + volumeIt->second);
            ok = false;
        }
    }

    return ok;
}
