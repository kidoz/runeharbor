// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "../game/game_world.hpp"
#include "../game/inventory.hpp"
#include "widgets.hpp"

namespace runeharbor::game
{
class SpellSystem; // forward declaration (spells.hpp pulls in many headers)
}

namespace runeharbor::ui
{

class InventoryWidget : public Widget
{
  public:
    InventoryWidget();

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setInventory(game::Inventory* inventory) { inventory_ = inventory; }
    void setSpellSystem(game::SpellSystem* spells) { spellSystem_ = spells; }
    void setActiveCharacter(int index) { activeCharacterIndex_ = index; }

    // Status-line feedback for equip/unequip outcomes (mirrors the shop window).
    void setStatusCallback(std::function<void(const std::string&)> cb)
    {
        onStatus_ = std::move(cb);
    }

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

    void setBackground(void* tex, int w, int h)
    {
        bgTexture_ = tex;
        bgW_ = w;
        bgH_ = h;
    }
    void setCharacterBody(void* tex, int w, int h)
    {
        bodyTexture_ = tex;
        bodyW_ = w;
        bodyH_ = h;
    }

  private:
    void* getCachedTexture(const std::string& name, int& w, int& h);
    // Hit-test the backpack grid; returns the backpack slot index or -1.
    int backpackSlotAt(int mouseX, int mouseY) const;
    // Hit-test the paper-doll; returns the equipped slot clicked or Count.
    game::EquipSlot equippedSlotAt(int mouseX, int mouseY) const;
    void handleClick(int mouseX, int mouseY);

    game::GameWorld* gameWorld_ = nullptr;
    game::Inventory* inventory_ = nullptr;
    game::SpellSystem* spellSystem_ = nullptr;
    int activeCharacterIndex_ = 0;
    TextureLookup textureLookup_;
    std::function<void(const std::string&)> onStatus_;

    struct CachedTexture
    {
        void* tex;
        int w, h;
    };
    std::unordered_map<std::string, CachedTexture> textureCache_;

    void* bgTexture_ = nullptr;
    int bgW_ = 0, bgH_ = 0;

    void* bodyTexture_ = nullptr;
    int bodyW_ = 0, bodyH_ = 0;
};

} // namespace runeharbor::ui