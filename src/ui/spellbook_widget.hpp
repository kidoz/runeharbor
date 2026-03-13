// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "../game/game_world.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

class SpellbookWidget : public Widget
{
  public:
    SpellbookWidget();

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setActiveCharacter(int index) { activeCharacterIndex_ = index; }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

    void setBackground(void* tex, int w, int h)
    {
        bgTexture_ = tex;
        bgW_ = w;
        bgH_ = h;
    }

  private:
    void* getCachedTexture(const std::string& name, int& w, int& h);

    game::GameWorld* gameWorld_ = nullptr;
    int activeCharacterIndex_ = 0;
    TextureLookup textureLookup_;

    struct CachedTexture
    {
        void* tex;
        int w, h;
    };
    std::unordered_map<std::string, CachedTexture> textureCache_;

    void* bgTexture_ = nullptr;
    int bgW_ = 0, bgH_ = 0;
};

} // namespace runeharbor::ui
