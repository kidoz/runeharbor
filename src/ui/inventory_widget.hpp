// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "../game/game_world.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

class InventoryWidget : public Widget
{
  public:
    InventoryWidget();

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setActiveCharacter(int index) { activeCharacterIndex_ = index; }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

  private:
    void setBackground(void* tex, int w, int h) { bgTexture_ = tex; bgW_ = w; bgH_ = h; }
    void setCharacterBody(void* tex, int w, int h) { bodyTexture_ = tex; bodyW_ = w; bodyH_ = h; }

  private:
    game::GameWorld* gameWorld_ = nullptr;
    int activeCharacterIndex_ = 0;
    
    void* bgTexture_ = nullptr;
    int bgW_ = 0, bgH_ = 0;
    
    void* bodyTexture_ = nullptr;
    int bodyW_ = 0, bodyH_ = 0;
};

} // namespace runeharbor::ui