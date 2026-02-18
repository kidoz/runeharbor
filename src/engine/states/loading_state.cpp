// SPDX-License-Identifier: MIT
#include "loading_state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <format>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"
#include "../../util/ilogger.hpp"

namespace runeharbor::engine
{

namespace
{
std::string toUpper(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return value;
}
} // namespace

LoadingState::LoadingState(StateContext& ctx) : ctx(ctx) {}

LoadingState::~LoadingState() = default;

void LoadingState::setCallbacks(StartLoadingFn start, IsLoadingDoneFn isDone,
                                FinalizeLoadingFn finalize)
{
    onStartLoading = start;
    savedStartLoading = std::move(start);
    onIsLoadingDone = std::move(isDone);
    onFinalizeLoading = std::move(finalize);
}

void LoadingState::setProgress(std::atomic<float>* progress)
{
    loadProgress = progress;
}

void LoadingState::setBackground(void* tex, int w, int h)
{
    background = tex;
    backgroundWidth = w;
    backgroundHeight = h;
}

void LoadingState::setFallbackBackground(void* tex, int w, int h)
{
    fallbackBackground = tex;
    fallbackWidth = w;
    fallbackHeight = h;
}

void LoadingState::setAnimationFrames(std::vector<void*>* frames, std::vector<int>* widths,
                                      std::vector<int>* heights)
{
    animFrames = frames;
    animWidths = widths;
    animHeights = heights;
}

void LoadingState::enter()
{
    loadingStarted = false;
    enterTicks = SDL_GetTicks();
    onStartLoading = savedStartLoading;
    if (loadProgress)
    {
        loadProgress->store(0.0f);
    }
}

void LoadingState::exit() {}

std::optional<GameStateId> LoadingState::update()
{
    bool autoLoad = ctx.shared && ctx.shared->autoLoadMap;

    // Wait for RETURN if not auto-loading
    if (!autoLoad && !loadingStarted)
    {
        if (ctx.isKeyPressed(SDL_SCANCODE_RETURN))
        {
            ctx.logger.info("LoadingState: RETURN pressed, starting load");
            loadingStarted = true;
        }
        else
        {
            return std::nullopt;
        }
    }

    if (!loadingStarted)
    {
        ctx.logger.info(std::format("LoadingState: auto-starting load (autoLoad={})", autoLoad));
        loadingStarted = true;
    }

    // Start async loading
    if (onStartLoading)
    {
        ctx.logger.info("LoadingState: calling onStartLoading");
        onStartLoading();
        onStartLoading = nullptr; // Only call once
    }

    // Check if loading is done
    if (onIsLoadingDone && onIsLoadingDone())
    {
        ctx.logger.info("LoadingState: loading done, finalizing");
        if (onFinalizeLoading)
        {
            bool success = onFinalizeLoading();
            if (success)
            {
                ctx.logger.info("LoadingState: finalize succeeded → InGame");
                return GameStateId::InGame;
            }
            else
            {
                ctx.logger.error("LoadingState: finalize failed → TitleScreen");
                return GameStateId::TitleScreen;
            }
        }
    }

    return std::nullopt;
}

void LoadingState::render()
{
    if (!ctx.renderer)
    {
        return;
    }

    // Render animation or static background
    float progress = loadProgress ? std::clamp(loadProgress->load(), 0.0f, 1.0f) : 0.0f;

    if (animFrames && !animFrames->empty() && animWidths && animHeights)
    {
        size_t frameIndex = 0;
        if (animFrames->size() > 1 && loadProgress)
        {
            frameIndex = static_cast<size_t>(progress * static_cast<float>(animFrames->size() - 1));
        }
        else
        {
            uint64_t now = SDL_GetTicks();
            frameIndex = static_cast<size_t>((now - enterTicks) / 100) % animFrames->size();
        }
        ctx.renderFullscreenTexture((*animFrames)[frameIndex], (*animWidths)[frameIndex],
                                    (*animHeights)[frameIndex]);
    }
    else if (background)
    {
        ctx.renderFullscreenTexture(background, backgroundWidth, backgroundHeight);
    }
    else if (fallbackBackground)
    {
        ctx.renderFullscreenTexture(fallbackBackground, fallbackWidth, fallbackHeight);
    }

    if (!ctx.debugText || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);

    // Loading text
    std::string mapName = ctx.shared ? ctx.shared->startupMapName : "";
    std::string line = "LOADING...";
    if (!mapName.empty())
    {
        line = "LOADING " + toUpper(mapName);
    }
    int percent = static_cast<int>(progress * 100.0f + 0.5f);
    line += " " + std::to_string(percent) + "%";

    int scale = 2;
    int x = 40;
    int y = ctx.viewportHeight > 0 ? ctx.viewportHeight - 60 : 520;
    ctx.debugText->drawText(sdlRenderer, x, y, scale, 255, 255, 255, line);

    // Progress bar
    int barWidth = 240;
    int barHeight = 10;
    int barY = y + ctx.debugText->lineHeight(scale) + 6;
    SDL_FRect bg = {static_cast<float>(x), static_cast<float>(barY), static_cast<float>(barWidth),
                    static_cast<float>(barHeight)};
    SDL_SetRenderDrawColor(sdlRenderer, 20, 20, 20, 200);
    SDL_RenderFillRect(sdlRenderer, &bg);

    SDL_FRect fg = {static_cast<float>(x) + 1.0f, static_cast<float>(barY) + 1.0f,
                    static_cast<float>((barWidth - 2) * progress),
                    static_cast<float>(barHeight - 2)};
    SDL_SetRenderDrawColor(sdlRenderer, 230, 200, 120, 220);
    SDL_RenderFillRect(sdlRenderer, &fg);
}

} // namespace runeharbor::engine
