// SPDX-License-Identifier: MIT
#include "video_player.hpp"

#include "bink_decoder.hpp"
#include "smacker_decoder.hpp"
#include "vid_archive.hpp"

#include "../graphics/debug_text.hpp"

#include <SDL3/SDL.h>

// Include audio frame types
using runeharbor::media::BinkAudioFrame;
using runeharbor::media::SmackerAudioFrame;

namespace runeharbor::media
{

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer()
{
    destroyTexture();
}

bool VideoPlayer::loadArchive(const std::filesystem::path& path)
{
    auto archive = std::make_unique<VIDArchive>();
    if (!archive->open(path))
    {
        return false;
    }

    archives_.push_back(std::move(archive));
    return true;
}

void VideoPlayer::unloadArchives()
{
    stop();
    archives_.clear();
}

void VideoPlayer::setPlaylist(const std::vector<VideoClip>& clips)
{
    playlist_ = clips;
    currentIndex_ = 0;
    currentName_ = playlist_.empty() ? "" : playlist_.front().name;
    active_ = false;
    finished_ = playlist_.empty();
}

void VideoPlayer::clearPlaylist()
{
    stop();
    playlist_.clear();
    currentName_.clear();
    finished_ = true;
}

void VideoPlayer::start(uint64_t startTicks)
{
    if (playlist_.empty())
    {
        finished_ = true;
        active_ = false;
        return;
    }

    currentIndex_ = 0;
    currentName_ = playlist_.front().name;
    playStartTicks_ = startTicks;
    clipStartTicks_ = startTicks;
    totalPausedTime_ = 0;
    currentFrame_ = 0;
    active_ = true;
    paused_ = false;
    finished_ = false;

    loadCurrentClip();
}

void VideoPlayer::stop()
{
    active_ = false;
    paused_ = false;
    finished_ = true;
    currentFrame_ = 0;
    smackerDecoder_.reset();
    destroyTexture();
}

void VideoPlayer::pause()
{
    if (active_ && !paused_)
    {
        paused_ = true;
        pauseStartTicks_ = SDL_GetTicks();
    }
}

void VideoPlayer::resume()
{
    if (active_ && paused_)
    {
        totalPausedTime_ += SDL_GetTicks() - pauseStartTicks_;
        paused_ = false;
    }
}

void VideoPlayer::skipToNext()
{
    if (!active_ || playlist_.empty())
    {
        return;
    }

    advanceToNextClip(SDL_GetTicks());
}

void VideoPlayer::update(uint64_t nowTicks)
{
    if (!active_ || paused_ || playlist_.empty())
    {
        return;
    }

    updateFrame(nowTicks);
}

void VideoPlayer::updateFrame(uint64_t nowTicks)
{
    uint64_t effectiveTicks = nowTicks - totalPausedTime_;

    if (smackerDecoder_)
    {
        // Calculate current frame based on time
        double fps = smackerDecoder_->frameRate();
        if (fps <= 0)
        {
            fps = 15.0;
        }

        uint64_t elapsedMs = effectiveTicks - clipStartTicks_;
        uint32_t targetFrame = static_cast<uint32_t>((elapsedMs * fps) / 1000.0);

        if (targetFrame >= smackerDecoder_->frameCount())
        {
            advanceToNextClip(nowTicks);
            return;
        }

        currentFrame_ = targetFrame;
    }
    else if (binkDecoder_)
    {
        // Calculate current frame based on time
        double fps = binkDecoder_->frameRate();
        if (fps <= 0)
        {
            fps = 15.0;
        }

        uint64_t elapsedMs = effectiveTicks - clipStartTicks_;
        uint32_t targetFrame = static_cast<uint32_t>((elapsedMs * fps) / 1000.0);

        if (targetFrame >= binkDecoder_->frameCount())
        {
            advanceToNextClip(nowTicks);
            return;
        }

        currentFrame_ = targetFrame;
    }
    else
    {
        // Unknown format - use duration
        const auto& clip = playlist_[currentIndex_];
        uint64_t elapsedMs = effectiveTicks - clipStartTicks_;

        if (elapsedMs >= clip.durationMs)
        {
            advanceToNextClip(nowTicks);
        }
    }
}

void VideoPlayer::advanceToNextClip(uint64_t nowTicks)
{
    currentIndex_++;

    if (currentIndex_ >= playlist_.size())
    {
        active_ = false;
        finished_ = true;
        smackerDecoder_.reset();
        binkDecoder_.reset();
        return;
    }

    currentName_ = playlist_[currentIndex_].name;
    clipStartTicks_ = nowTicks - totalPausedTime_;
    currentFrame_ = 0;

    loadCurrentClip();
}

bool VideoPlayer::loadCurrentClip()
{
    smackerDecoder_.reset();
    binkDecoder_.reset();

    if (currentIndex_ >= playlist_.size())
    {
        return false;
    }

    const std::string& name = playlist_[currentIndex_].name;

    // Find archive containing this clip
    VIDArchive* archive = findArchiveWithClip(name);
    if (!archive)
    {
        // Clip not found - treat as placeholder
        return false;
    }

    auto entryIdx = archive->findEntry(name);
    if (!entryIdx)
    {
        return false;
    }

    const VIDEntry* entry = archive->getEntry(*entryIdx);
    if (!entry)
    {
        return false;
    }

    if (entry->format == VideoFormat::Smacker)
    {
        // Load and decode Smacker video
        std::vector<uint8_t> videoData = archive->readVideoData(*entryIdx);
        if (videoData.empty())
        {
            return false;
        }

        smackerDecoder_ = std::make_unique<SmackerDecoder>();
        if (!smackerDecoder_->load(videoData))
        {
            smackerDecoder_.reset();
            return false;
        }

        return true;
    }
    else if (entry->format == VideoFormat::Bink)
    {
        // Load and decode Bink video
        std::vector<uint8_t> videoData = archive->readVideoData(*entryIdx);
        if (videoData.empty())
        {
            return false;
        }

        binkDecoder_ = std::make_unique<BinkDecoder>();
        if (!binkDecoder_->load(videoData))
        {
            binkDecoder_.reset();
            return false;
        }

        return true;
    }

    return false;
}

VIDArchive* VideoPlayer::findArchiveWithClip(const std::string& name)
{
    for (auto& archive : archives_)
    {
        if (archive->findEntry(name))
        {
            return archive.get();
        }
    }
    return nullptr;
}

void VideoPlayer::render(SDL_Renderer* renderer, graphics::DebugText* debugText, int width,
                         int height)
{
    if (!renderer || !active_)
    {
        return;
    }

    if (smackerDecoder_ || binkDecoder_)
    {
        renderFrame(renderer, width, height);
    }
    else
    {
        renderPlaceholder(renderer, debugText, width, height);
    }
}

void VideoPlayer::renderFrame(SDL_Renderer* renderer, int width, int height)
{
    uint32_t vidWidth = 0;
    uint32_t vidHeight = 0;
    std::vector<uint8_t> rgba;

    if (smackerDecoder_)
    {
        vidWidth = smackerDecoder_->width();
        vidHeight = smackerDecoder_->height();
        rgba = smackerDecoder_->getFrameRGBA(currentFrame_);
    }
    else if (binkDecoder_)
    {
        vidWidth = binkDecoder_->width();
        vidHeight = binkDecoder_->height();
        rgba = binkDecoder_->getFrameRGBA(currentFrame_);
    }
    else
    {
        return;
    }

    if (rgba.empty())
    {
        return;
    }

    // Create or recreate texture if needed
    if (!frameTexture_ || lastRenderer_ != renderer || textureWidth_ != vidWidth ||
        textureHeight_ != vidHeight)
    {
        destroyTexture();

        frameTexture_ =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                              static_cast<int>(vidWidth), static_cast<int>(vidHeight));

        if (!frameTexture_)
        {
            return;
        }

        lastRenderer_ = renderer;
        textureWidth_ = vidWidth;
        textureHeight_ = vidHeight;
    }

    // Update texture
    SDL_UpdateTexture(frameTexture_, nullptr, rgba.data(), static_cast<int>(vidWidth * 4));

    // Calculate destination rect (fit to screen, maintaining aspect ratio)
    float videoAspect = static_cast<float>(vidWidth) / static_cast<float>(vidHeight);
    float screenAspect = static_cast<float>(width) / static_cast<float>(height);

    SDL_FRect destRect;
    if (videoAspect > screenAspect)
    {
        // Video is wider - fit to width
        destRect.w = static_cast<float>(width);
        destRect.h = static_cast<float>(width) / videoAspect;
        destRect.x = 0;
        destRect.y = (static_cast<float>(height) - destRect.h) / 2.0f;
    }
    else
    {
        // Video is taller - fit to height
        destRect.h = static_cast<float>(height);
        destRect.w = static_cast<float>(height) * videoAspect;
        destRect.x = (static_cast<float>(width) - destRect.w) / 2.0f;
        destRect.y = 0;
    }

    // Clear with black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render video frame
    SDL_RenderTexture(renderer, frameTexture_, nullptr, &destRect);
}

void VideoPlayer::renderPlaceholder(SDL_Renderer* renderer, graphics::DebugText* debugText,
                                    int width, int height)
{
    // Fill background
    SDL_FRect background = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    SDL_SetRenderDrawColor(renderer, 10, 10, 20, 255);
    SDL_RenderFillRect(renderer, &background);

    if (!debugText)
    {
        return;
    }

    const int scale = 3;

    std::string statusText = "VIDEO PLAYBACK (UNKNOWN FORMAT)";
    int textWidth = debugText->measureTextWidth(statusText, scale);
    int startX = (width - textWidth) / 2;
    int startY = height / 2 - debugText->lineHeight(scale);

    debugText->drawText(renderer, startX, startY, scale, 230, 230, 230, statusText);

    std::string nameLine = "CLIP: " + currentName_;
    int nameWidth = debugText->measureTextWidth(nameLine, scale);
    int nameX = (width - nameWidth) / 2;
    debugText->drawText(renderer, nameX, startY + debugText->lineHeight(scale), scale, 200, 200,
                        120, nameLine);
}

void VideoPlayer::destroyTexture()
{
    if (frameTexture_)
    {
        SDL_DestroyTexture(frameTexture_);
        frameTexture_ = nullptr;
    }
    lastRenderer_ = nullptr;
    textureWidth_ = 0;
    textureHeight_ = 0;
}

uint32_t VideoPlayer::totalFrames() const
{
    if (smackerDecoder_)
    {
        return smackerDecoder_->frameCount();
    }
    if (binkDecoder_)
    {
        return binkDecoder_->frameCount();
    }
    return 0;
}

bool VideoPlayer::hasAudio() const
{
    if (smackerDecoder_)
    {
        return smackerDecoder_->hasAudio(0);
    }
    if (binkDecoder_)
    {
        return binkDecoder_->hasAudio(0);
    }
    return false;
}

uint32_t VideoPlayer::audioSampleRate() const
{
    if (smackerDecoder_)
    {
        auto info = smackerDecoder_->getAudioInfo(0);
        return info.sampleRate;
    }
    if (binkDecoder_)
    {
        auto info = binkDecoder_->getAudioInfo(0);
        return info.sampleRate;
    }
    return 0;
}

uint8_t VideoPlayer::audioChannels() const
{
    if (smackerDecoder_)
    {
        auto info = smackerDecoder_->getAudioInfo(0);
        return info.isStereo ? 2 : 1;
    }
    if (binkDecoder_)
    {
        auto info = binkDecoder_->getAudioInfo(0);
        return static_cast<uint8_t>(info.channels);
    }
    return 0;
}

std::vector<int16_t> VideoPlayer::getAudioSamples()
{
    if (smackerDecoder_)
    {
        SmackerAudioFrame audioFrame;
        if (smackerDecoder_->decodeAudio(currentFrame_, 0, audioFrame))
        {
            return audioFrame.samples;
        }
    }
    if (binkDecoder_)
    {
        BinkAudioFrame audioFrame;
        if (binkDecoder_->decodeAudio(currentFrame_, 0, audioFrame))
        {
            return audioFrame.samples;
        }
    }
    return {};
}

} // namespace runeharbor::media
