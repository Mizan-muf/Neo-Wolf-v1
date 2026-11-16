#pragma once

#include <SDL.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // Initialises the SDL audio subsystem and opens the default device. Returns false (the
    // game then runs silently) if there is no usable audio device.
    bool Init(int sampleRate = 44100, int channels = 2, int samples = 1024);
    void Shutdown();

    bool LoadEffectFromWav(
        const std::string& effectId,
        const std::string& filePath,
        std::vector<std::string>& errors);
    bool HasEffect(const std::string& effectId) const;
    void PlayEffect(const std::string& effectId) const;
    bool IsAvailable() const { return initialized_; }

    void SetMasterVolume(float volume);
    float MasterVolume() const;

private:
    struct EffectData {
        std::vector<std::uint8_t> pcm;
    };

    std::unordered_map<std::string, EffectData> effects_{};
    SDL_AudioDeviceID deviceId_ = 0;
    int deviceFrequency_ = 0;
    std::uint16_t deviceFormat_ = 0;
    std::uint8_t deviceChannels_ = 0;
    float masterVolume_ = 1.0f;
    bool initialized_ = false;
    bool subsystemInitialized_ = false;
};
