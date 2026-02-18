// SPDX-License-Identifier: MIT
#include "state_context.hpp"

#include <SDL3/SDL.h>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"

namespace runeharbor::engine
{

void StateContext::renderFullscreenTexture(void* textureHandle, int texWidth, int texHeight) const
{
    if (!renderer || !textureHandle || texWidth <= 0 || texHeight <= 0 || viewportWidth <= 0 ||
        viewportHeight <= 0)
    {
        return;
    }

    float sx = static_cast<float>(viewportWidth) / static_cast<float>(texWidth);
    float sy = static_cast<float>(viewportHeight) / static_cast<float>(texHeight);
    float scale = std::min(sx, sy);

    int drawWidth = static_cast<int>(texWidth * scale);
    int drawHeight = static_cast<int>(texHeight * scale);
    int drawX = (viewportWidth - drawWidth) / 2;
    int drawY = (viewportHeight - drawHeight) / 2;

    renderer->renderTexture(textureHandle, drawX, drawY, drawWidth, drawHeight);
}

void StateContext::renderMenu(const std::vector<std::string>& items, int selectedIndex, int x,
                              int y, int scale) const
{
    if (!debugText || !renderer || !renderer->getSDLRenderer())
    {
        return;
    }

    SDL_Renderer* sdlRenderer = renderer->getSDLRenderer();
    int cursorY = y;
    int panelPadding = 6;
    int panelWidth = 0;
    int panelHeight = 0;

    for (const auto& item : items)
    {
        panelWidth =
            std::max(panelWidth, debugText->charWidth(scale) * static_cast<int>(item.size()));
    }
    panelHeight = debugText->lineHeight(scale) * static_cast<int>(items.size());

    SDL_FRect panel = {static_cast<float>(x - panelPadding), static_cast<float>(y - panelPadding),
                       static_cast<float>(panelWidth + panelPadding * 2),
                       static_cast<float>(panelHeight + panelPadding * 2)};
    SDL_SetRenderDrawColor(sdlRenderer, 10, 10, 10, 180);
    SDL_RenderFillRect(sdlRenderer, &panel);

    for (size_t i = 0; i < items.size(); i++)
    {
        bool selected = static_cast<int>(i) == selectedIndex;
        uint8_t r = selected ? 255 : 210;
        uint8_t g = selected ? 220 : 210;
        uint8_t b = selected ? 130 : 210;
        debugText->drawText(sdlRenderer, x, cursorY, scale, r, g, b, items[i]);
        cursorY += debugText->lineHeight(scale);
    }
}

} // namespace runeharbor::engine
