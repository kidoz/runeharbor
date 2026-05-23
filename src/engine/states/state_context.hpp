// SPDX-License-Identifier: MIT
#pragma once

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <functional>
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
class BitmapFont;
} // namespace runeharbor::graphics
namespace runeharbor::media
{
class VideoPlayer;
}
namespace runeharbor::game
{
class GameWorld;
class EventEngine;
class CombatSystem;
class SpellSystem;
class Inventory;
class SaveGame;
} // namespace runeharbor::game
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
    game::EventEngine* eventEngine = nullptr;
    game::CombatSystem* combatSystem = nullptr;
    game::SpellSystem* spellSystem = nullptr;
    game::Inventory* inventory = nullptr;
    game::SaveGame* saveGame = nullptr;
    MapScene* mapScene = nullptr;
    std::string newGameStartMapName = "out01.odm";
    std::string startupMapName;
    bool startupPreferOutdoor = false;
    bool autoLoadMap = false;
    bool showFrameRate = false;
    // Non-zero => request loadingN.pcx screen during the next loading state.
    int loadingScreenIndex = 0;
    bool loadFromSave = false;
    bool quickStartReady = false;
    bool arrivalOverrideActive = false;
    float arrivalX = 0.0f;
    float arrivalY = 0.0f;
    float arrivalZ = 0.0f;
    float arrivalYaw = 0.0f;
    bool hasPendingEventRuntimeState = false;
    std::vector<uint8_t> pendingEventRuntimeState;
    // 3D world viewport within 640x480 game coordinates ([screen] INI section).
    int worldViewportX = 8;
    int worldViewportY = 8;
    int worldViewportWidth = 468;
    int worldViewportHeight = 351;
    std::string statusMessage;
    // Runtime NPC dialogue handoff from event callbacks to in-game UI.
    int pendingNpcDialogId = -1;
    bool awaitingNpcDialogText = false;
    bool openNpcDialogue = false;
    std::string npcDialogueSpeaker;
    std::string npcDialogueText;
    std::vector<int> npcDialogueChoiceIds;
    std::vector<std::string> npcDialogueChoiceTexts;
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

    // Bitmap fonts (loaded from .fnt files in ICONS.LOD)
    graphics::BitmapFont* createFont = nullptr;   // create.fnt (char creation headers)
    graphics::BitmapFont* ccharFont = nullptr;    // CCHAR.FNT (character names)
    graphics::BitmapFont* arrusFont = nullptr;    // ARRUS.FNT (stats/labels)
    graphics::BitmapFont* smallnumFont = nullptr; // SMALLNUM.FNT (small numbers)

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

    // --- Audio helpers ---
    std::function<void(const std::string&)> playUiSound;
};

} // namespace runeharbor::engine
