// SPDX-License-Identifier: MIT
#include "audio_system.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

#include <cstring>

namespace runeharbor::audio
{

AudioSystem::AudioSystem(util::ILogger& logger) : logger_(logger) {}

AudioSystem::~AudioSystem()
{
    shutdown();
}

bool AudioSystem::initialize()
{
    if (initialized_)
    {
        return true;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        logger_.error(std::string("Failed to init SDL audio: ") + SDL_GetError());
        return false;
    }

    initialized_ = true;
    logger_.info("Audio system initialized");
    return true;
}

void AudioSystem::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    stopAll();
    unloadAll();

    // Clean up all stream instances
    for (auto& inst : instances_)
    {
        if (inst.stream)
        {
            SDL_DestroyAudioStream(inst.stream);
            inst.stream = nullptr;
        }
    }
    instances_.clear();

    initialized_ = false;
    logger_.info("Audio system shut down");
}

bool AudioSystem::loadSound(const std::string& name, const std::vector<uint8_t>& wavData)
{
    if (wavData.empty())
    {
        logger_.warning("Empty WAV data for sound: " + name);
        return false;
    }

    // Parse WAV using SDL
    SDL_AudioSpec spec;
    uint8_t* audioBuffer = nullptr;
    uint32_t audioLen = 0;

    SDL_IOStream* rw = SDL_IOFromConstMem(wavData.data(), static_cast<int>(wavData.size()));
    if (!rw)
    {
        logger_.warning("Failed to create IO stream for sound: " + name);
        return false;
    }

    if (!SDL_LoadWAV_IO(rw, true, &spec, &audioBuffer, &audioLen))
    {
        logger_.warning("Failed to load WAV: " + name + " - " + SDL_GetError());
        return false;
    }

    SoundBuffer buffer;
    buffer.pcmData.assign(audioBuffer, audioBuffer + audioLen);
    buffer.sampleRate = spec.freq;
    buffer.channels = spec.channels;
    buffer.sdlFormat = spec.format;
    buffer.bytesPerSample = SDL_AUDIO_BYTESIZE(spec.format);

    SDL_free(audioBuffer);

    sounds_[name] = std::move(buffer);
    logger_.debug("Loaded sound: " + name + " (" + std::to_string(audioLen) + " bytes, " +
                  std::to_string(spec.freq) + "Hz, " + std::to_string(spec.channels) + "ch)");
    return true;
}

void AudioSystem::unloadSound(const std::string& name)
{
    sounds_.erase(name);
}

void AudioSystem::unloadAll()
{
    sounds_.clear();
}

int AudioSystem::playSound(const std::string& name, float volume)
{
    if (!initialized_)
    {
        return -1;
    }

    auto it = sounds_.find(name);
    if (it == sounds_.end())
    {
        logger_.warning("Sound not loaded: " + name);
        return -1;
    }

    const auto& buffer = it->second;

    // Create audio spec for this sound
    SDL_AudioSpec srcSpec;
    srcSpec.format = static_cast<SDL_AudioFormat>(buffer.sdlFormat);
    srcSpec.channels = buffer.channels;
    srcSpec.freq = buffer.sampleRate;

    // Create a stream bound to the default playback device
    SDL_AudioStream* stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &srcSpec, nullptr, nullptr);
    if (!stream)
    {
        logger_.warning("Failed to create audio stream: " + std::string(SDL_GetError()));
        return -1;
    }

    // Put audio data into stream
    if (!SDL_PutAudioStreamData(stream, buffer.pcmData.data(),
                                static_cast<int>(buffer.pcmData.size())))
    {
        logger_.warning("Failed to put audio data: " + std::string(SDL_GetError()));
        SDL_DestroyAudioStream(stream);
        return -1;
    }

    // Signal that no more data will be added
    SDL_FlushAudioStream(stream);

    // Set volume
    float effectiveVolume = volume * masterVolume_;
    SDL_SetAudioStreamGain(stream, effectiveVolume);

    // Resume playback
    SDL_ResumeAudioStreamDevice(stream);

    // Find or create an instance slot
    cleanupFinished();
    int instanceId = -1;
    for (size_t i = 0; i < instances_.size(); i++)
    {
        if (!instances_[i].active)
        {
            instanceId = static_cast<int>(i);
            break;
        }
    }
    if (instanceId < 0)
    {
        instanceId = static_cast<int>(instances_.size());
        instances_.push_back({});
    }

    instances_[static_cast<size_t>(instanceId)] = {stream, true, volume};
    return instanceId;
}

void AudioSystem::stopSound(int instanceId)
{
    if (instanceId < 0 || instanceId >= static_cast<int>(instances_.size()))
    {
        return;
    }

    auto& inst = instances_[static_cast<size_t>(instanceId)];
    if (inst.stream)
    {
        SDL_DestroyAudioStream(inst.stream);
        inst.stream = nullptr;
    }
    inst.active = false;
}

void AudioSystem::stopAll()
{
    for (int i = 0; i < static_cast<int>(instances_.size()); i++)
    {
        stopSound(i);
    }
}

void AudioSystem::setMasterVolume(float vol)
{
    masterVolume_ = std::clamp(vol, 0.0f, 1.0f);
    // Update all active instances
    for (auto& inst : instances_)
    {
        if (inst.active && inst.stream)
        {
            SDL_SetAudioStreamGain(inst.stream, inst.volume * masterVolume_);
        }
    }
}

bool AudioSystem::hasSound(const std::string& name) const
{
    return sounds_.contains(name);
}

int AudioSystem::activeSoundCount() const
{
    int count = 0;
    for (const auto& inst : instances_)
    {
        if (inst.active)
        {
            count++;
        }
    }
    return count;
}

void AudioSystem::cleanupFinished()
{
    for (auto& inst : instances_)
    {
        if (inst.active && inst.stream)
        {
            // Check if stream has finished playing
            int available = SDL_GetAudioStreamAvailable(inst.stream);
            if (available == 0)
            {
                SDL_DestroyAudioStream(inst.stream);
                inst.stream = nullptr;
                inst.active = false;
            }
        }
    }
}

} // namespace runeharbor::audio
