#include "engine/audio/AudioSystem.h"

#include <SDL.h>

#include <algorithm>

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"

AudioSystem::~AudioSystem() {
    Shutdown();
}

bool AudioSystem::Init(int sampleRate, int channels, int samples) {
    if (initialized_) {
        return true;
    }

    SDL_AudioSpec desired{};
    desired.freq = sampleRate;
    desired.format = AUDIO_S16LSB;
    desired.channels = static_cast<std::uint8_t>(channels);
    desired.samples = static_cast<std::uint16_t>(samples);
    desired.callback = nullptr;

    SDL_AudioSpec obtained{};
    deviceId_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (deviceId_ == 0) {
        LogError(std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError());
        return false;
    }

    deviceFrequency_ = obtained.freq;
    deviceFormat_ = obtained.format;
    deviceChannels_ = obtained.channels;
    SDL_PauseAudioDevice(deviceId_, 0);
    initialized_ = true;
    return true;
}

void AudioSystem::Shutdown() {
    effects_.clear();

    if (deviceId_ != 0) {
        SDL_ClearQueuedAudio(deviceId_);
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }

    deviceFrequency_ = 0;
    deviceFormat_ = 0;
    deviceChannels_ = 0;
    masterVolume_ = 1.0f;
    initialized_ = false;
}

bool AudioSystem::LoadEffectFromWav(
    const std::string& effectId,
    const std::string& filePath,
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

    bool ok = true;
    EffectData effect{};

    SDL_AudioCVT cvt{};
    if (SDL_BuildAudioCVT(
            &cvt,
            sourceSpec.format,
            sourceSpec.channels,
            sourceSpec.freq,
            deviceFormat_,
            deviceChannels_,
            deviceFrequency_) < 0) {
        errors.push_back("Failed to build audio converter for " + effectId + ": " + SDL_GetError());
        ok = false;
    } else if (cvt.needed == 0) {
        effect.pcm.assign(sourceBuffer, sourceBuffer + sourceLength);
    } else {
        cvt.len = static_cast<int>(sourceLength);
        cvt.buf = static_cast<std::uint8_t*>(SDL_malloc(static_cast<std::size_t>(cvt.len) * cvt.len_mult));
        if (cvt.buf == nullptr) {
            errors.push_back("Out of memory while converting audio for: " + effectId);
            ok = false;
        } else {
            std::copy(sourceBuffer, sourceBuffer + sourceLength, cvt.buf);
            if (SDL_ConvertAudio(&cvt) != 0) {
                errors.push_back("Failed to convert audio for " + effectId + ": " + SDL_GetError());
                ok = false;
            } else {
                effect.pcm.assign(cvt.buf, cvt.buf + cvt.len_cvt);
            }
            SDL_free(cvt.buf);
            cvt.buf = nullptr;
        }
    }

    SDL_FreeWAV(sourceBuffer);

    if (!ok || effect.pcm.empty()) {
        if (ok) {
            errors.push_back("Converted audio produced empty buffer for: " + effectId);
        }
        return false;
    }

    effects_[effectId] = std::move(effect);
    return true;
}

bool AudioSystem::HasEffect(const std::string& effectId) const {
    return effects_.find(effectId) != effects_.end();
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

    if (masterVolume_ >= 0.999f) {
        SDL_QueueAudio(deviceId_, effect.pcm.data(), static_cast<std::uint32_t>(effect.pcm.size()));
        return;
    }

    std::vector<std::uint8_t> scaled(effect.pcm);
    if (deviceFormat_ == AUDIO_S16LSB || deviceFormat_ == AUDIO_S16MSB) {
        auto* samples = reinterpret_cast<std::int16_t*>(scaled.data());
        const std::size_t sampleCount = scaled.size() / sizeof(std::int16_t);
        for (std::size_t i = 0; i < sampleCount; ++i) {
            const float value = static_cast<float>(samples[i]) * masterVolume_;
            samples[i] = static_cast<std::int16_t>(Clamp(value, -32768.0f, 32767.0f));
        }
    } else if (deviceFormat_ == AUDIO_U8) {
        for (std::uint8_t& value : scaled) {
            const int centered = static_cast<int>(value) - 128;
            const int mixed = static_cast<int>(static_cast<float>(centered) * masterVolume_) + 128;
            value = static_cast<std::uint8_t>(Clamp(mixed, 0, 255));
        }
    }

    SDL_QueueAudio(deviceId_, scaled.data(), static_cast<std::uint32_t>(scaled.size()));
}

void AudioSystem::SetMasterVolume(float volume) {
    masterVolume_ = Clamp(volume, 0.0f, 1.0f);
}

float AudioSystem::MasterVolume() const {
    return masterVolume_;
}
