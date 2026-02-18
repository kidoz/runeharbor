// SPDX-License-Identifier: MIT
#include "title_state.hpp"

#include <SDL3/SDL.h>

#include "../../graphics/debug_text.hpp"
#include "../../graphics/irenderer.hpp"

namespace runeharbor::engine
{

const std::vector<std::string> TitleState::kMenuItems = {"NEW", "LOAD", "CREDITS", "EXIT"};

TitleState::TitleState(StateContext& ctx) : ctx(ctx) {}

TitleState::~TitleState() = default;

void TitleState::setBackground(void* tex, int w, int h)
{
    background = tex;
    backgroundWidth = w;
    backgroundHeight = h;
}

void TitleState::setButtonTextures(int index, void* tex, int w, int h)
{
    if (index >= 0 && index < kButtonCount)
    {
        buttonTextures[index] = tex;
        buttonWidths[index] = w;
        buttonHeights[index] = h;
    }
}

void TitleState::enter()
{
    selectedIndex = 0;
    statusMessage.clear();
    buttons.clear(); // Force re-layout on first frame
}

void TitleState::exit() {}

std::optional<GameStateId> TitleState::update()
{
    // Lazy button layout (needs viewport size)
    if (buttons.empty())
    {
        layoutButtons();
    }

    updateHover();

    // Keyboard navigation
    int count = static_cast<int>(kMenuItems.size());
    if (ctx.isKeyPressed(SDL_SCANCODE_UP))
    {
        selectedIndex = (selectedIndex + count - 1) % count;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_DOWN))
    {
        selectedIndex = (selectedIndex + 1) % count;
    }

    // Activation (Enter or mouse click)
    bool activated = ctx.isKeyPressed(SDL_SCANCODE_RETURN);
    if (!activated && ctx.window.wasMousePressed(platform::MouseButton::Left))
    {
        for (size_t i = 0; i < buttons.size(); i++)
        {
            if (buttons[i].isHovered)
            {
                selectedIndex = static_cast<int>(i);
                activated = true;
                break;
            }
        }
    }

    if (activated)
    {
        switch (selectedIndex)
        {
        case 0:
            return GameStateId::CharacterCreation; // NEW GAME
        case 1:
            statusMessage = "Load game not implemented yet";
            break;
        case 2:
            statusMessage = "Credits not implemented yet";
            break;
        case 3:
            return GameStateId::Quit;
        default:
            break;
        }
    }

    // Keyboard shortcuts
    if (ctx.isKeyPressed(SDL_SCANCODE_N))
    {
        return GameStateId::CharacterCreation;
    }
    if (ctx.isKeyPressed(SDL_SCANCODE_L))
    {
        statusMessage = "Load game not implemented yet";
    }
    else if (ctx.isKeyPressed(SDL_SCANCODE_C))
    {
        statusMessage = "Credits not implemented yet";
    }
    else if (ctx.isKeyPressed(SDL_SCANCODE_Q) || ctx.isKeyPressed(SDL_SCANCODE_E))
    {
        return GameStateId::Quit;
    }

    return std::nullopt;
}

void TitleState::render()
{
    if (!ctx.renderer)
    {
        return;
    }

    if (background)
    {
        ctx.renderFullscreenTexture(background, backgroundWidth, backgroundHeight);
    }

    if (!ctx.debugText || !ctx.renderer->getSDLRenderer())
    {
        return;
    }

    // Render hover textures for buttons
    for (const auto& button : buttons)
    {
        if (button.isHovered && button.hoverTexture)
        {
            ctx.renderer->renderTexture(button.hoverTexture, button.bounds.x, button.bounds.y,
                                        button.bounds.width, button.bounds.height);
        }
    }

    // Render status message
    if (!statusMessage.empty())
    {
        int scale = 2;
        int x = 40;
        int y = ctx.viewportHeight > 0 ? ctx.viewportHeight - 60 : 520;
        ctx.debugText->drawText(ctx.renderer->getSDLRenderer(), x, y, scale, 255, 220, 80,
                                statusMessage);
    }
}

void TitleState::layoutButtons()
{
    if (ctx.viewportWidth == 0 || ctx.viewportHeight == 0)
    {
        return;
    }
    buttons.clear();

    // MM7-accurate button positions (640x480 game coords)
    constexpr int kButtonGameX = 495;
    constexpr int kButtonGameY[] = {172, 227, 282, 337};

    for (int i = 0; i < kButtonCount; ++i)
    {
        int btnW = buttonWidths[i] > 0 ? buttonWidths[i] : 85;
        int btnH = buttonHeights[i] > 0 ? buttonHeights[i] : 30;

        MenuButton button;
        button.id = kMenuItems[static_cast<size_t>(i)];
        button.bounds = {ctx.scaleX(kButtonGameX), ctx.scaleY(kButtonGameY[i]), ctx.scaleW(btnW),
                         ctx.scaleH(btnH)};
        button.hoverTexture = buttonTextures[i];
        button.textureWidth = buttonWidths[i];
        button.textureHeight = buttonHeights[i];
        buttons.push_back(button);
    }
}

void TitleState::updateHover()
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        bool mouseOver = ctx.isMouseOver(button.bounds);
        if (mouseOver)
        {
            selectedIndex = static_cast<int>(i);
        }
        button.isHovered = (mouseOver || (static_cast<int>(i) == selectedIndex));
    }
}

} // namespace runeharbor::engine
