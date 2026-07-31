// SPDX-License-Identifier: MIT
#include "intro_state.hpp"

#include <SDL3/SDL.h>

#include <format>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../../media/video_player.hpp"
#include "../../util/ilogger.hpp"

namespace runeharbor::engine
{

IntroState::IntroState(StateContext& ctx) : ctx(ctx) {}

IntroState::~IntroState() = default;

void IntroState::setPlaylist(std::vector<media::VideoClip> pl)
{
    playlist = std::move(pl);
    hasVideo = !playlist.empty();
}

void IntroState::enter()
{
    hasVideo = false;
    if (ctx.videoPlayer && !playlist.empty())
    {
        ctx.videoPlayer->setPlaylist(playlist);
        ctx.videoPlayer->start(SDL_GetTicks());
        hasVideo = true;
    }
    ctx.logger.info(std::format("IntroState::enter: playlist={} videoPlayer={} hasVideo={}",
                                playlist.size(), ctx.videoPlayer != nullptr, hasVideo));

    if (hasVideo && !ctx.videoPlayer->hasDecoder())
    {
        ctx.logger.warning(std::format("IntroState: no decoder for '{}' ({}), intro will be blank",
                                       ctx.videoPlayer->currentClipName(),
                                       ctx.videoPlayer->lastError()));
    }
}

void IntroState::exit() {}

std::optional<GameStateId> IntroState::update()
{
    // No video to play → go straight to title
    if (!hasVideo)
    {
        ctx.logger.info("IntroState: no video available, skipping to title");
        return GameStateId::TitleScreen;
    }

    if (ctx.videoPlayer)
    {
        ctx.videoPlayer->update(SDL_GetTicks());
        if (ctx.videoPlayer->isFinished())
        {
            ctx.logger.info("IntroState: playback finished, advancing to title");
            return GameStateId::TitleScreen;
        }
    }

    // ESC: skip all remaining clips → title screen
    if (ctx.isKeyPressed(SDL_SCANCODE_ESCAPE))
    {
        return GameStateId::TitleScreen;
    }

    // SPACE / RETURN / mouse click: skip current clip
    bool skipPressed = ctx.isKeyPressed(SDL_SCANCODE_RETURN) ||
                       ctx.isKeyPressed(SDL_SCANCODE_SPACE) ||
                       ctx.window.wasMousePressed(platform::MouseButton::Left);

    if (skipPressed && ctx.videoPlayer)
    {
        ctx.videoPlayer->skipToNext();
        if (ctx.videoPlayer->isFinished())
        {
            return GameStateId::TitleScreen;
        }
    }

    return std::nullopt;
}

void IntroState::render()
{
    if (!ctx.videoPlayer || !ctx.renderer)
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    if (!sdlRenderer || ctx.viewportWidth <= 0 || ctx.viewportHeight <= 0)
    {
        return;
    }

    ctx.videoPlayer->render(sdlRenderer, ctx.debugText, ctx.viewportWidth, ctx.viewportHeight);
}

} // namespace runeharbor::engine
