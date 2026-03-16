// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::game
{
class GameWorld;
}

namespace runeharbor::engine
{
class MapScene;
}

namespace runeharbor::ui
{

/// In-game HUD rendering.
/// Draws party portraits, HP/SP bars, gold/food, time, and a simple minimap.
/// Operates in game coordinates (640x480); caller provides scaling.
class HUD
{
  public:
    HUD() = default;

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    /// Set the game world to read party/calendar data from
    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setMapScene(engine::MapScene* mapScene) { mapScene_ = mapScene; }

    /// Render the HUD overlay.
    /// @param renderer Renderer for drawing primitives
    /// @param debugText Text renderer
    /// @param scaleX/scaleY multiplier from game coords to screen coords
    /// @param offsetX/offsetY letterbox offset in screen coords
    void render(graphics::IRenderer& renderer, const graphics::DebugText& debugText, float scale,
                float offsetX, float offsetY);

  private:
    void renderPartyBar(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                        float scale, float offsetX, float offsetY);
    void renderResourceBar(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                           float scale, float offsetX, float offsetY);
    void renderMinimap(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                       float scale, float offsetX, float offsetY);
    void renderTimeDisplay(graphics::IRenderer& renderer, const graphics::DebugText& debugText,
                           float scale, float offsetX, float offsetY);

    // Convert game coords to screen coords
    int sx(int gameX, float scale, float offsetX) const;
    int sy(int gameY, float scale, float offsetY) const;
    int sw(int gameW, float scale) const;
    int sh(int gameH, float scale) const;

    game::GameWorld* gameWorld_ = nullptr;
    engine::MapScene* mapScene_ = nullptr;
    TextureLookup textureLookup_;
};

} // namespace runeharbor::ui
