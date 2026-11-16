#pragma once

// game/defs/audio.cfg: the sound file of each effect and the master volume
// (docs/CONTENT_DEFINITIONS.md).

#include <string>
#include <vector>

struct AudioEffectDefinitions {
    std::string doorOpen;
    std::string pickup;
    std::string enemyAlert;
    std::string weaponFire;
    float masterVolume = 0.85f;  // master_volume default (0.0 to 1.0)
};

bool LoadAudioEffectDefinitions(const std::string& filePath, AudioEffectDefinitions& outDefinitions,
                                std::vector<std::string>& errors);
