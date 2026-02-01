// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace runeharbor::graphics
{
class DebugText;
}

namespace runeharbor::media
{

class VIDArchive;
class SmackerDecoder;
class BinkDecoder;

struct VideoClip
{
    std::string name;
    uint32_t durationMs = 3000; // Fallback duration if auto-detection fails
};

/**
 * Video Player - Plays Smacker and Bink videos from VID archives
 *
 * Supports:
 * - Smacker (.smk) - Full software decoding
 * - Bink (.bik) - Full software decoding
 */
class VideoPlayer
{
  public:
    VideoPlayer();
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer&) = delete;
    VideoPlayer& operator=(const VideoPlayer&) = delete;

    // Load VID archive(s)
    bool loadArchive(const std::filesystem::path& path);
    void unloadArchives();

    // Playlist management
    void setPlaylist(const std::vector<VideoClip>& clips);
    void clearPlaylist();

    // Playback control
    void start(uint64_t startTicks);
    void stop();
    void pause();
    void resume();
    void skipToNext();

    // Update and render
    void update(uint64_t nowTicks);
    void render(SDL_Renderer* renderer, graphics::DebugText* debugText, int width, int height);

    // State queries
    bool isActive() const { return active_; }
    bool isPaused() const { return paused_; }
    bool isFinished() const { return finished_; }
    const std::string& currentClipName() const { return currentName_; }
    uint32_t currentFrame() const { return currentFrame_; }
    uint32_t totalFrames() const;

    // Audio support
    bool hasAudio() const;
    uint32_t audioSampleRate() const;
    uint8_t audioChannels() const;

    // Get audio samples for current frame (returns empty if no audio)
    std::vector<int16_t> getAudioSamples();

  private:
    // Archives
    std::vector<std::unique_ptr<VIDArchive>> archives_;

    // Current decoder
    std::unique_ptr<SmackerDecoder> smackerDecoder_;
    std::unique_ptr<BinkDecoder> binkDecoder_;

    // Playlist
    std::vector<VideoClip> playlist_;
    size_t currentIndex_ = 0;
    std::string currentName_;

    // Playback state
    uint64_t playStartTicks_ = 0;
    uint64_t clipStartTicks_ = 0;
    uint64_t pauseStartTicks_ = 0;
    uint64_t totalPausedTime_ = 0;
    uint32_t currentFrame_ = 0;
    bool active_ = false;
    bool paused_ = false;
    bool finished_ = false;

    // Rendering
    SDL_Texture* frameTexture_ = nullptr;
    SDL_Renderer* lastRenderer_ = nullptr;
    uint32_t textureWidth_ = 0;
    uint32_t textureHeight_ = 0;

    // Internal methods
    bool loadCurrentClip();
    void advanceToNextClip(uint64_t nowTicks);
    void updateFrame(uint64_t nowTicks);
    void renderFrame(SDL_Renderer* renderer, int width, int height);
    void renderPlaceholder(SDL_Renderer* renderer, graphics::DebugText* debugText, int width,
                           int height);
    void destroyTexture();
    VIDArchive* findArchiveWithClip(const std::string& name);
};

} // namespace runeharbor::media
