// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../util/ilogger.hpp"

struct SDL_AudioStream;

namespace runeharbor::audio
{

// Loaded sound effect (PCM data ready for playback)
struct SoundBuffer
{
    std::vector<uint8_t> pcmData;
    int sampleRate = 0;
    int channels = 0;
    int bytesPerSample = 0; // 1 for u8, 2 for s16, 4 for float32
    uint32_t sdlFormat = 0; // SDL_AudioFormat
};

// Active sound instance
struct SoundInstance
{
    SDL_AudioStream* stream = nullptr;
    bool active = false;
    float volume = 1.0f;
};

class AudioSystem
{
  public:
    explicit AudioSystem(util::ILogger& logger);
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // Initialize SDL audio subsystem
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // Load a WAV sound from raw file data
    bool loadSound(const std::string& name, const std::vector<uint8_t>& wavData);

    // Unload a single sound
    void unloadSound(const std::string& name);

    // Unload all sounds
    void unloadAll();

    // Play a loaded sound effect (returns instance ID, -1 on failure)
    int playSound(const std::string& name, float volume = 1.0f);

    // Stop a specific sound instance
    void stopSound(int instanceId);

    // Stop all currently playing sounds
    void stopAll();

    // Master volume (0.0 - 1.0)
    float masterVolume() const { return masterVolume_; }
    void setMasterVolume(float vol);
    void setMaxChannels(int channels);
    int maxChannels() const { return maxChannels_; }

    // Query
    bool hasSound(const std::string& name) const;
    int activeSoundCount() const;

  private:
    void cleanupFinished();

    util::ILogger& logger_;
    bool initialized_ = false;
    float masterVolume_ = 1.0f;
    int maxChannels_ = 16;

    std::unordered_map<std::string, SoundBuffer> sounds_;
    std::vector<SoundInstance> instances_;
};

} // namespace runeharbor::audio
