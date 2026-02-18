// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../game/character.hpp"
#include "../../graphics/primitives.hpp"
#include "../../platform/iwindow.hpp"

// Forward declarations
struct SDL_Renderer;
namespace runeharbor::util
{
class ILogger;
}
namespace runeharbor::graphics
{
class IRenderer;
class SDLRenderer;
class LineRenderer;
class DebugText;
class WorldRenderer;
class Camera;
} // namespace runeharbor::graphics
namespace runeharbor::media
{
class VideoPlayer;
}
namespace runeharbor::game
{
class GameWorld;
}
namespace runeharbor::engine
{
class VirtualFileSystem;
class MapScene;
} // namespace runeharbor::engine

namespace runeharbor::engine
{

// Character is game::Character, aliased into engine:: by application.hpp
using Character = game::Character;

// MM7 base resolution
inline constexpr int kGameWidth = 640;
inline constexpr int kGameHeight = 480;

/// Data that flows between game states (owned by Application)
struct SharedGameData
{
    std::vector<Character>* party = nullptr;
    game::GameWorld* gameWorld = nullptr;
    MapScene* mapScene = nullptr;
    std::string startupMapName;
    bool startupPreferOutdoor = false;
    bool autoLoadMap = false;
    bool quickStartReady = false;
};

/// Shared resources and helpers available to all game states.
/// Application owns the resources; states get a non-owning reference.
struct StateContext
{
    util::ILogger& logger;
    platform::IWindow& window;
    graphics::IRenderer* renderer = nullptr;
    graphics::SDLRenderer* sdlRenderer = nullptr;
    graphics::LineRenderer* lineRenderer = nullptr;
    graphics::WorldRenderer* worldRenderer = nullptr;
    graphics::DebugText* debugText = nullptr;
    media::VideoPlayer* videoPlayer = nullptr;
    VirtualFileSystem* vfs = nullptr;
    graphics::Camera* camera = nullptr;

    SharedGameData* shared = nullptr;

    int viewportWidth = 0;
    int viewportHeight = 0;

    // Input state (managed by Application's main loop)
    const bool* keyState = nullptr;
    const std::vector<uint8_t>* previousKeyState = nullptr;
    int keyCount = 0;

    // --- Input helpers ---

    bool isKeyDown(SDL_Scancode code) const
    {
        if (!keyState || code >= keyCount)
        {
            return false;
        }
        return keyState[code];
    }

    bool isKeyPressed(SDL_Scancode code) const
    {
        if (!keyState || code >= keyCount)
        {
            return false;
        }
        bool now = keyState[code];
        bool before = (previousKeyState && !previousKeyState->empty())
                          ? (*previousKeyState)[static_cast<size_t>(code)] != 0
                          : false;
        return now && !before;
    }

    bool isMouseOver(const graphics::Rect& rect) const
    {
        auto ms = window.getMouseState();
        return rect.contains(ms.x, ms.y);
    }

    bool wasMouseClickedIn(const graphics::Rect& rect) const
    {
        if (!window.wasMousePressed(platform::MouseButton::Left))
        {
            return false;
        }
        return isMouseOver(rect);
    }

    // --- Coordinate scaling (640x480 -> viewport) ---

    int scaleX(int gameX) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        float offsetX = (viewportWidth - kGameWidth * s) / 2.0f;
        return static_cast<int>(offsetX + gameX * s);
    }

    int scaleY(int gameY) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        float offsetY = (viewportHeight - kGameHeight * s) / 2.0f;
        return static_cast<int>(offsetY + gameY * s);
    }

    int scaleW(int gameW) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        return static_cast<int>(gameW * s);
    }

    int scaleH(int gameH) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        return static_cast<int>(gameH * s);
    }

    int unscaleX(int screenX) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        if (s <= 0.0f)
            return 0;
        float offsetX = (viewportWidth - kGameWidth * s) / 2.0f;
        return static_cast<int>((screenX - offsetX) / s);
    }

    int unscaleY(int screenY) const
    {
        float s = std::min(static_cast<float>(viewportWidth) / static_cast<float>(kGameWidth),
                           static_cast<float>(viewportHeight) / static_cast<float>(kGameHeight));
        if (s <= 0.0f)
            return 0;
        float offsetY = (viewportHeight - kGameHeight * s) / 2.0f;
        return static_cast<int>((screenY - offsetY) / s);
    }

    // --- Rendering helpers ---

    /// Render a texture scaled to fill the viewport (letterboxed)
    void renderFullscreenTexture(void* textureHandle, int texWidth, int texHeight) const;

    /// Render a simple text menu
    void renderMenu(const std::vector<std::string>& items, int selectedIndex, int x, int y,
                    int scale) const;
};

} // namespace runeharbor::engine
