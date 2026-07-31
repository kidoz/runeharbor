// SPDX-License-Identifier: MIT
#include "video_player.hpp"

#include <SDL3/SDL.h>

#include "../graphics/debug_text.hpp"
#include "bink_decoder.hpp"
#include "smacker_decoder.hpp"
#include "vid_archive.hpp"

// Include audio frame types
using runeharbor::media::BinkAudioFrame;
using runeharbor::media::SmackerAudioFrame;

namespace runeharbor::media
{

VideoPlayer::VideoPlayer()
{
    dummyBinkFrame_ = std::make_unique<BinkFrame>();
}

VideoPlayer::~VideoPlayer()
{
    closeAudioStream();
    destroyTexture();
}

bool VideoPlayer::loadArchive(const std::filesystem::path& path)
{
    auto archive = std::make_unique<VidArchive>();
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
    lastAudioFrameQueued_ = UINT32_MAX;
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
    binkDecoder_.reset();
    closeAudioStream();
    destroyTexture();
}

void VideoPlayer::pause()
{
    if (active_ && !paused_)
    {
        paused_ = true;
        pauseStartTicks_ = SDL_GetTicks();
        if (audioStream_)
        {
            SDL_PauseAudioStreamDevice(audioStream_);
        }
    }
}

void VideoPlayer::resume()
{
    if (active_ && paused_)
    {
        totalPausedTime_ += SDL_GetTicks() - pauseStartTicks_;
        paused_ = false;
        if (audioStream_)
        {
            SDL_ResumeAudioStreamDevice(audioStream_);
        }
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

    uint32_t oldFrame = currentFrame_;
    updateFrame(nowTicks);

    if (active_ && !paused_ && currentFrame_ != oldFrame)
    {
        queueAudioForCurrentFrame();
    }
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
        closeAudioStream();
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
    closeAudioStream();
    lastError_.clear();

    if (currentIndex_ >= playlist_.size())
    {
        lastError_ = "playlist index out of range";
        return false;
    }

    const std::string& name = playlist_[currentIndex_].name;

    // Find archive containing this clip
    VidArchive* archive = findArchiveWithClip(name);
    if (!archive)
    {
        // Clip not found - treat as placeholder
        lastError_ = "no loaded archive contains '" + name + "'";
        return false;
    }

    auto entryIdx = archive->findEntry(name);
    if (!entryIdx)
    {
        lastError_ = "archive has no entry for '" + name + "'";
        return false;
    }

    const VidEntry* entry = archive->getEntry(*entryIdx);
    if (!entry)
    {
        lastError_ = "entry lookup failed for '" + name + "'";
        return false;
    }

    if (entry->format == VideoFormat::Smacker)
    {
        // Load and decode Smacker video
        std::vector<uint8_t> videoData = archive->readVideoData(*entryIdx);
        if (videoData.empty())
        {
            lastError_ = "empty Smacker data for '" + name + "'";
            return false;
        }

        smackerDecoder_ = std::make_unique<SmackerDecoder>();
        if (!smackerDecoder_->load(videoData))
        {
            smackerDecoder_.reset();
            lastError_ = "Smacker decoder rejected '" + name + "'";
            return false;
        }

        // Find first track with audio
        audioTrack_ = 0;
        for (int i = 0; i < 7; i++)
        {
            if (smackerDecoder_->hasAudio(i))
            {
                audioTrack_ = i;
                break;
            }
        }

        setupAudioStream();
        return true;
    }
    else if (entry->format == VideoFormat::Bink)
    {
        // Load and decode Bink video
        std::vector<uint8_t> videoData = archive->readVideoData(*entryIdx);
        if (videoData.empty())
        {
            lastError_ = "empty Bink data for '" + name + "'";
            return false;
        }

        binkDecoder_ = std::make_unique<BinkDecoder>();
        if (!binkDecoder_->load(videoData))
        {
            binkDecoder_.reset();
            lastError_ = "Bink decoder rejected '" + name + "'";
            return false;
        }

        // Find first track with audio
        audioTrack_ = 0;
        for (uint32_t i = 0; i < binkDecoder_->audioTrackCount(); i++)
        {
            if (binkDecoder_->hasAudio(i))
            {
                audioTrack_ = static_cast<int>(i);
                break;
            }
        }

        setupAudioStream();
        return true;
    }

    return false;
}

VidArchive* VideoPlayer::findArchiveWithClip(const std::string& name)
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
    uint32_t pixelFormat = SDL_PIXELFORMAT_RGBA32;

    if (smackerDecoder_)
    {
        vidWidth = smackerDecoder_->width();
        vidHeight = smackerDecoder_->height();
        pixelFormat = SDL_PIXELFORMAT_RGBA32;
    }
    else if (binkDecoder_)
    {
        vidWidth = binkDecoder_->width();
        vidHeight = binkDecoder_->height();
        pixelFormat = SDL_PIXELFORMAT_IYUV;
    }
    else
    {
        return;
    }

    // Create or recreate texture if needed
    if (!frameTexture_ || lastRenderer_ != renderer || textureWidth_ != vidWidth ||
        textureHeight_ != vidHeight || textureFormat_ != pixelFormat)
    {
        destroyTexture();

        frameTexture_ = SDL_CreateTexture(renderer, static_cast<SDL_PixelFormat>(pixelFormat),
                                          SDL_TEXTUREACCESS_STREAMING, static_cast<int>(vidWidth),
                                          static_cast<int>(vidHeight));

        if (!frameTexture_)
        {
            return;
        }

        lastRenderer_ = renderer;
        textureWidth_ = vidWidth;
        textureHeight_ = vidHeight;
        textureFormat_ = pixelFormat;
        lastRenderedFrame_ = UINT32_MAX;
    }

    // Update texture only if frame changed
    if (currentFrame_ != lastRenderedFrame_)
    {
        if (smackerDecoder_)
        {
            const auto& rgba = smackerDecoder_->getFrameRGBA(currentFrame_);
            if (!rgba.empty())
            {
                void* pixels;
                int pitch;
                if (SDL_LockTexture(frameTexture_, nullptr, &pixels, &pitch))
                {
                    if (pitch == static_cast<int>(vidWidth * 4))
                    {
                        std::memcpy(pixels, rgba.data(), rgba.size());
                    }
                    else
                    {
                        const uint8_t* src = rgba.data();
                        uint8_t* dst = static_cast<uint8_t*>(pixels);
                        for (uint32_t y = 0; y < vidHeight; y++)
                        {
                            std::memcpy(dst, src, vidWidth * 4);
                            src += vidWidth * 4;
                            dst += pitch;
                        }
                    }
                    SDL_UnlockTexture(frameTexture_);
                    lastRenderedFrame_ = currentFrame_;
                }
            }
        }
        else if (binkDecoder_)
        {
            // Decodes frame if needed
            if (dummyBinkFrame_ && binkDecoder_->decodeFrame(currentFrame_, *dummyBinkFrame_))
            {
                auto planes = binkDecoder_->getYUVPlanes();
                if (planes)
                {
                    SDL_UpdateYUVTexture(frameTexture_, nullptr, planes->y,
                                         static_cast<int>(planes->yStride), planes->u,
                                         static_cast<int>(planes->uvStride), planes->v,
                                         static_cast<int>(planes->uvStride));
                    lastRenderedFrame_ = currentFrame_;
                }
            }
        }
    }

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
        for (int i = 0; i < 7; i++)
        {
            if (smackerDecoder_->hasAudio(i))
                return true;
        }
    }
    if (binkDecoder_)
    {
        for (uint32_t i = 0; i < binkDecoder_->audioTrackCount(); i++)
        {
            if (binkDecoder_->hasAudio(i))
                return true;
        }
    }
    return false;
}

uint32_t VideoPlayer::audioSampleRate() const
{
    if (smackerDecoder_)
    {
        auto info = smackerDecoder_->getAudioInfo(audioTrack_);
        return info.sampleRate;
    }
    if (binkDecoder_)
    {
        auto info = binkDecoder_->getAudioInfo(static_cast<uint32_t>(audioTrack_));
        return info.sampleRate;
    }
    return 0;
}

uint8_t VideoPlayer::audioChannels() const
{
    if (smackerDecoder_)
    {
        auto info = smackerDecoder_->getAudioInfo(audioTrack_);
        return info.isStereo ? 2 : 1;
    }
    if (binkDecoder_)
    {
        auto info = binkDecoder_->getAudioInfo(static_cast<uint32_t>(audioTrack_));
        return static_cast<uint8_t>(info.channels);
    }
    return 0;
}

std::vector<int16_t> VideoPlayer::getAudioSamples()
{
    std::vector<int16_t> samples;
    if (decodeAudioFrame(currentFrame_, samples))
    {
        return samples;
    }
    return {};
}

void VideoPlayer::setAudioEnabled(bool enabled)
{
    audioEnabled_ = enabled;
    if (!audioEnabled_)
    {
        closeAudioStream();
        return;
    }

    if (active_)
    {
        setupAudioStream();
    }
}

void VideoPlayer::setupAudioStream()
{
    if (!audioEnabled_)
    {
        closeAudioStream();
        return;
    }

    if (!hasAudio())
    {
        closeAudioStream();
        return;
    }

    uint32_t sampleRate = audioSampleRate();
    uint8_t channels = audioChannels();
    if (sampleRate == 0 || channels == 0)
    {
        closeAudioStream();
        return;
    }

    if (audioStream_ && sampleRate == audioStreamRate_ && channels == audioStreamChannels_)
    {
        SDL_ClearAudioStream(audioStream_);
        lastAudioFrameQueued_ = UINT32_MAX;
        SDL_ResumeAudioStreamDevice(audioStream_);
        return;
    }

    closeAudioStream();

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = static_cast<int>(channels);
    spec.freq = static_cast<int>(sampleRate);

    audioStream_ =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!audioStream_)
    {
        return;
    }

    audioStreamRate_ = sampleRate;
    audioStreamChannels_ = channels;
    lastAudioFrameQueued_ = UINT32_MAX;
    SDL_ResumeAudioStreamDevice(audioStream_);
}

void VideoPlayer::closeAudioStream()
{
    if (audioStream_)
    {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }
    audioStreamRate_ = 0;
    audioStreamChannels_ = 0;
    lastAudioFrameQueued_ = UINT32_MAX;
}

void VideoPlayer::queueAudioForCurrentFrame()
{
    if (!audioEnabled_ || !audioStream_ || !active_ || paused_ || !hasAudio())
    {
        return;
    }

    if (lastAudioFrameQueued_ != UINT32_MAX && currentFrame_ < lastAudioFrameQueued_)
    {
        SDL_ClearAudioStream(audioStream_);
        lastAudioFrameQueued_ = UINT32_MAX;
    }

    uint32_t startFrame =
        (lastAudioFrameQueued_ == UINT32_MAX) ? currentFrame_ : lastAudioFrameQueued_ + 1;

    // Safety: don't queue more than 10 frames at once to avoid stalls
    if (currentFrame_ > startFrame + 10)
    {
        startFrame = currentFrame_ - 10;
    }

    if (startFrame > currentFrame_)
    {
        return;
    }

    for (uint32_t frame = startFrame; frame <= currentFrame_; frame++)
    {
        std::vector<int16_t> samples;
        if (decodeAudioFrame(frame, samples) && !samples.empty())
        {
            SDL_PutAudioStreamData(audioStream_, samples.data(),
                                   static_cast<int>(samples.size() * sizeof(int16_t)));
        }
    }

    lastAudioFrameQueued_ = currentFrame_;
}

bool VideoPlayer::decodeAudioFrame(uint32_t frameIndex, std::vector<int16_t>& outSamples)
{
    if (smackerDecoder_)
    {
        SmackerAudioFrame audioFrame;
        if (smackerDecoder_->decodeAudio(frameIndex, audioTrack_, audioFrame))
        {
            outSamples = audioFrame.samples;
            return true;
        }
    }
    if (binkDecoder_)
    {
        BinkAudioFrame audioFrame;
        if (binkDecoder_->decodeAudio(frameIndex, static_cast<uint32_t>(audioTrack_), audioFrame))
        {
            outSamples = audioFrame.samples;
            return true;
        }
    }
    return false;
}

} // namespace runeharbor::media
