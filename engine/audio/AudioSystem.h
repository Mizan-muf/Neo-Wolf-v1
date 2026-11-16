#pragma once

// The SDL audio device: loads WAV sound effects, converts them to the device format and plays
// them by queueing their samples on the device (no mixer: effects play one after another,
// KNOWN_ISSUES C23). Optional: the game runs silently without a device. Only AudioSystem.cpp
// includes SDL, so files that include this header (via Engine.h) do not see SDL.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_AudioSpec;

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    // Initialises the SDL audio subsystem and opens the default device. Returns false (the
    // game then runs silently) if there is no usable audio device.
    bool Init(int sampleRate = 44100, int channels = 2, int samples = 1024);
    void Shutdown();

    bool LoadEffectFromWav(const std::string& effectId, const std::string& filePath, std::vector<std::string>& errors);
    void PlayEffect(const std::string& effectId) const;
    bool IsAvailable() const { return initialized_; }

    void SetMasterVolume(float volume);

private:
    struct EffectData {
        std::vector<std::uint8_t> pcm;
    };

    // Converts `length` bytes of WAV samples in `sourceSpec`'s format to the device format.
    bool ConvertToDeviceFormat(const std::string& effectId, const SDL_AudioSpec& sourceSpec, const std::uint8_t* source,
                               std::uint32_t length, std::vector<std::uint8_t>& pcm,
                               std::vector<std::string>& errors) const;

    std::unordered_map<std::string, EffectData> effects_;
    std::uint32_t deviceId_ = 0;  // an SDL_AudioDeviceID; 0 = no device open
    int deviceFrequency_ = 0;
    std::uint16_t deviceFormat_ = 0;  // an SDL_AudioFormat
    std::uint8_t deviceChannels_ = 0;
    float masterVolume_ = 1.0f;
    bool initialized_ = false;
    bool subsystemInitialized_ = false;
};
