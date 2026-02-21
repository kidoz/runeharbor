// SPDX-License-Identifier: MIT
#include "credits_state.hpp"

#include <SDL3/SDL.h>

#include <format>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"

namespace runeharbor::engine
{

CreditsState::CreditsState(StateContext& ctx) : ctx(ctx) {}

CreditsState::~CreditsState() = default;

void CreditsState::setCreditsSections(std::vector<formats::CreditsSection> sections)
{
    sections_ = std::move(sections);
}

void CreditsState::enter()
{
    enterTicks_ = SDL_GetTicks();
    scrollY_ = static_cast<float>(kGameHeight);
}

void CreditsState::exit() {}

std::optional<GameStateId> CreditsState::update()
{
    if (ctx.isKeyPressed(SDL_SCANCODE_ESCAPE) || ctx.isKeyPressed(SDL_SCANCODE_RETURN))
    {
        return GameStateId::TitleScreen;
    }

    // Advance scroll
    uint64_t now = SDL_GetTicks();
    float elapsed = static_cast<float>(now - enterTicks_) / 1000.0f;
    scrollY_ = static_cast<float>(kGameHeight) - elapsed * kScrollSpeed;

    // Calculate total content height
    int totalLines = 0;
    for (const auto& section : sections_)
    {
        totalLines += 2; // title + blank line
        totalLines += static_cast<int>(section.content.size());
        totalLines += 1; // gap between sections
    }
    float totalHeight = static_cast<float>(totalLines * 16);

    // Scroll complete when all content has passed off the top
    if (scrollY_ < -totalHeight)
    {
        return GameStateId::TitleScreen;
    }

    return std::nullopt;
}

void CreditsState::render()
{
    if (!ctx.renderer || !ctx.debugText || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    // Black background
    ctx.renderer->clear(0, 0, 0, 255);

    SDL_Renderer* sdlRenderer = ctx.renderer->getSDLRenderer();
    int scale = 2;
    int lineH = ctx.debugText->lineHeight(scale);
    int centerX = ctx.viewportWidth / 2;

    float currentY = scrollY_;

    for (const auto& section : sections_)
    {
        // Section title (bright gold)
        int screenY = ctx.scaleY(static_cast<int>(currentY));
        if (screenY > -lineH && screenY < ctx.viewportHeight)
        {
            int titleWidth = static_cast<int>(section.title.size()) * 8 * scale;
            int titleX = centerX - titleWidth / 2;
            ctx.debugText->drawText(sdlRenderer, titleX, screenY, scale, 255, 215, 0,
                                    section.title);
        }
        currentY += 16.0f;

        // Blank line after title
        currentY += 16.0f;

        // Content lines (lighter)
        for (const auto& line : section.content)
        {
            screenY = ctx.scaleY(static_cast<int>(currentY));
            if (screenY > -lineH && screenY < ctx.viewportHeight)
            {
                int textWidth = static_cast<int>(line.size()) * 8 * scale;
                int textX = centerX - textWidth / 2;
                ctx.debugText->drawText(sdlRenderer, textX, screenY, scale, 200, 200, 220, line);
            }
            currentY += 16.0f;
        }

        // Gap between sections
        currentY += 16.0f;
    }

    // Show "Press ESC to return" at bottom
    int hintScale = std::max(1, scale - 1);
    ctx.debugText->drawText(sdlRenderer, 20, ctx.viewportHeight - 30, hintScale, 120, 120, 120,
                            "Press ESC or ENTER to return");
}

} // namespace runeharbor::engine
