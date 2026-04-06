#pragma once

#include <string>
#include <vector>

struct AudioEffectDefinitions {
    std::string doorOpen;
    std::string pickup;
    std::string enemyAlert;
    std::string weaponFire;
    float masterVolume = 0.85f;
};

bool LoadAudioEffectDefinitions(
    const std::string& filePath,
    AudioEffectDefinitions& outDefinitions,
    std::vector<std::string>& errors);
