// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../game/game_world.hpp"
#include "../game/spells.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

// A request to cast a spell, emitted when the player selects a spell in the
// spellbook. The InGameState uses the target type to drive the targeting flow
// (single-ally -> portrait click, single-enemy -> monster click, etc.).
struct SpellCastRequest
{
    int spellId = 0;
    game::SpellTarget targetType = game::SpellTarget::SingleEnemy;
};

class SpellbookWidget : public Widget
{
  public:
    SpellbookWidget();

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setSpellSystem(game::SpellSystem* spells) { spellSystem_ = spells; }
    void setActiveCharacter(int index) { activeCharacterIndex_ = index; }

    // Fired when the player chooses a spell to cast (Enter/click on a spell).
    void setSpellCastRequestCallback(std::function<void(const SpellCastRequest&)> cb)
    {
        onCastRequest_ = std::move(cb);
    }
    // Status feedback (e.g. "not enough mana").
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

  private:
    void* getCachedTexture(const std::string& name, int& w, int& h);
    void requestCast(int spellId);

    game::GameWorld* gameWorld_ = nullptr;
    game::SpellSystem* spellSystem_ = nullptr;
    int activeCharacterIndex_ = 0;
    TextureLookup textureLookup_;

    // Cached spell list + selection (rebuilt each render).
    struct SpellRow
    {
        int id = 0;
        std::string name;
        int manaCost = 0;
        game::SpellTarget target = game::SpellTarget::SingleEnemy;
    };
    std::vector<SpellRow> rows_;
    std::vector<int> rowY_; // screen Y of each row for hit-testing
    int rowHeight_ = 18;
    int selected_ = 0;

    std::function<void(const SpellCastRequest&)> onCastRequest_;
    std::function<void(const std::string&)> onStatus_;

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
