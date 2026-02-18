// SPDX-License-Identifier: MIT
#pragma once

namespace runeharbor::graphics
{
class IRenderer;
class DebugText;
} // namespace runeharbor::graphics

namespace runeharbor::game
{
class GameWorld;
}

namespace runeharbor::ui
{

/// In-game HUD rendering.
/// Draws party portraits, HP/SP bars, gold/food, time, minimap placeholder.
/// Operates in game coordinates (640x480); caller provides scaling.
class HUD
{
  public:
    HUD() = default;

    /// Set the game world to read party/calendar data from
    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }

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
};

} // namespace runeharbor::ui
