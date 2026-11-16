#include "engine/audio/AudioSystem.h"

#include <SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"

// The header stores these SDL types as plain integers so that it does not include SDL.
static_assert(std::is_same_v<SDL_AudioDeviceID, std::uint32_t>, "AudioSystem::deviceId_ type");
static_assert(std::is_same_v<SDL_AudioFormat, std::uint16_t>, "AudioSystem::deviceFormat_ type");

namespace {
// Volumes this close to 1 count as full volume: the effect is queued as is, without a copy.
constexpr float kFullVolume = 0.999f;
// Unsigned 8-bit samples are centred on 128 (silence).
constexpr int kU8Silence = 128;

struct WavDeleter {
    void operator()(std::uint8_t* buffer) const { SDL_FreeWAV(buffer); }
};

struct SdlFreeDeleter {
    void operator()(std::uint8_t* buffer) const { SDL_free(buffer); }
};

// Scales signed 16-bit samples (host byte order) by `volume`, clamping to the sample range.
void ScaleS16Samples(std::vector<std::uint8_t>& pcm, float volume) {
    constexpr auto kMinSample = static_cast<float>(std::numeric_limits<std::int16_t>::min());
    constexpr auto kMaxSample = static_cast<float>(std::numeric_limits<std::int16_t>::max());
    for (std::size_t offset = 0; offset + sizeof(std::int16_t) <= pcm.size(); offset += sizeof(std::int16_t)) {
        std::int16_t sample = 0;
        std::memcpy(&sample, &pcm[offset], sizeof(sample));
        sample = static_cast<std::int16_t>(Clamp(static_cast<float>(sample) * volume, kMinSample, kMaxSample));
        std::memcpy(&pcm[offset], &sample, sizeof(sample));
    }
}

// Scales unsigned 8-bit samples around their silence level by `volume`.
void ScaleU8Samples(std::vector<std::uint8_t>& pcm, float volume) {
    for (std::uint8_t& value : pcm) {
        const int centered = static_cast<int>(value) - kU8Silence;
        const int mixed = static_cast<int>(static_cast<float>(centered) * volume) + kU8Silence;
        value = static_cast<std::uint8_t>(Clamp(mixed, 0, static_cast<int>(std::numeric_limits<std::uint8_t>::max())));
    }
}
}  // namespace

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Init(int sampleRate, int channels, int samples) {
    if (initialized_) {
        return true;
    }

    // Audio is its own SDL subsystem, so a machine without a usable audio device still runs
    // the game (silently). Engine::Init logs the warning when this returns false.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        LogWarning(std::string("SDL audio initialisation failed: ") + SDL_GetError());
        return false;
    }
    subsystemInitialized_ = true;

    SDL_AudioSpec desired{};
    desired.freq = sampleRate;
    desired.format = AUDIO_S16LSB;
    desired.channels = static_cast<std::uint8_t>(channels);
    desired.samples = static_cast<std::uint16_t>(samples);
    desired.callback = nullptr;  // no callback: samples are pushed with SDL_QueueAudio

    SDL_AudioSpec obtained{};
    deviceId_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceId_ == 0) {
        LogWarning(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        subsystemInitialized_ = false;
        return false;
    }

    deviceFrequency_ = obtained.freq;
    deviceFormat_ = obtained.format;
    deviceChannels_ = obtained.channels;
    SDL_PauseAudioDevice(deviceId_, 0);  // 0 = start playing
    initialized_ = true;
    const char* driver = SDL_GetCurrentAudioDriver();
    LogInfo(std::string("Audio: driver ") + (driver != nullptr ? driver : "?") + ", " +
            std::to_string(deviceFrequency_) + " Hz, " + std::to_string(deviceChannels_) + " channel(s)");
    return true;
}

void AudioSystem::Shutdown() {
    effects_.clear();

    if (deviceId_ != 0) {
        SDL_ClearQueuedAudio(deviceId_);
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }

    if (subsystemInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        subsystemInitialized_ = false;
    }

    deviceFrequency_ = 0;
    deviceFormat_ = 0;
    deviceChannels_ = 0;
    masterVolume_ = 1.0f;
    initialized_ = false;
}

bool AudioSystem::LoadEffectFromWav(const std::string& effectId, const std::string& filePath,
                                    std::vector<std::string>& errors) {
    if (!initialized_ || deviceId_ == 0) {
        errors.push_back("Audio device not initialized while loading: " + effectId);
        return false;
    }

    SDL_AudioSpec sourceSpec{};
    std::uint8_t* sourceBuffer = nullptr;
    std::uint32_t sourceLength = 0;
    if (SDL_LoadWAV(filePath.c_str(), &sourceSpec, &sourceBuffer, &sourceLength) == nullptr) {
        errors.push_back("Failed to load WAV (" + effectId + "): " + filePath + " (" + SDL_GetError() + ")");
        return false;
    }
    const std::unique_ptr<std::uint8_t, WavDeleter> source(sourceBuffer);

    EffectData effect{};
    if (!ConvertToDeviceFormat(effectId, sourceSpec, source.get(), sourceLength, effect.pcm, errors)) {
        return false;
    }
    if (effect.pcm.empty()) {
        errors.push_back("Converted audio produced empty buffer for: " + effectId);
        return false;
    }

    effects_[effectId] = std::move(effect);
    return true;
}

bool AudioSystem::ConvertToDeviceFormat(const std::string& effectId, const SDL_AudioSpec& sourceSpec,
                                        const std::uint8_t* source, std::uint32_t length,
                                        std::vector<std::uint8_t>& pcm, std::vector<std::string>& errors) const {
    SDL_AudioCVT cvt{};
    if (SDL_BuildAudioCVT(&cvt, sourceSpec.format, sourceSpec.channels, sourceSpec.freq, deviceFormat_, deviceChannels_,
                          deviceFrequency_) < 0) {
        errors.push_back("Failed to build audio converter for " + effectId + ": " + SDL_GetError());
        return false;
    }
    if (cvt.needed == 0) {
        pcm.assign(source, source + length);
        return true;
    }

    // SDL converts in place, in a buffer len_mult times the source size.
    cvt.len = static_cast<int>(length);
    const std::unique_ptr<std::uint8_t, SdlFreeDeleter> buffer(
        static_cast<std::uint8_t*>(SDL_malloc(static_cast<std::size_t>(cvt.len) * cvt.len_mult)));
    if (buffer == nullptr) {
        errors.push_back("Out of memory while converting audio for: " + effectId);
        return false;
    }
    std::copy(source, source + length, buffer.get());
    cvt.buf = buffer.get();
    if (SDL_ConvertAudio(&cvt) != 0) {
        errors.push_back("Failed to convert audio for " + effectId + ": " + SDL_GetError());
        return false;
    }
    pcm.assign(cvt.buf, cvt.buf + cvt.len_cvt);
    return true;
}

void AudioSystem::PlayEffect(const std::string& effectId) const {
    if (!initialized_ || deviceId_ == 0) {
        return;
    }

    const auto it = effects_.find(effectId);
    if (it == effects_.end()) {
        return;
    }

    const EffectData& effect = it->second;
    if (effect.pcm.empty()) {
        return;
    }

    if (masterVolume_ >= kFullVolume) {
        SDL_QueueAudio(deviceId_, effect.pcm.data(), static_cast<std::uint32_t>(effect.pcm.size()));
        return;
    }

    // Only the 16-bit and 8-bit integer formats are scaled; other device formats play at full
    // volume.
    std::vector<std::uint8_t> scaled(effect.pcm);
    if (deviceFormat_ == AUDIO_S16LSB || deviceFormat_ == AUDIO_S16MSB) {
        ScaleS16Samples(scaled, masterVolume_);
    } else if (deviceFormat_ == AUDIO_U8) {
        ScaleU8Samples(scaled, masterVolume_);
    }

    SDL_QueueAudio(deviceId_, scaled.data(), static_cast<std::uint32_t>(scaled.size()));
}

void AudioSystem::SetMasterVolume(float volume) {
    masterVolume_ = Clamp(volume, 0.0f, 1.0f);
}
