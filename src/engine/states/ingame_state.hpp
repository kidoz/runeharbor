// SPDX-License-Identifier: MIT
#pragma once

#include "../../graphics/primitives.hpp"
#include "../../graphics/visibility.hpp"
#include "../../ui/character_stats_widget.hpp"
#include "../../ui/dialogue.hpp"
#include "../../ui/hud.hpp"
#include "../../ui/inventory_widget.hpp"
#include "../../ui/map_widget.hpp"
#include "../../ui/rest_widget.hpp"
#include "../../ui/shop_window.hpp"
#include "../../ui/spellbook_widget.hpp"
#include "../map_scene.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class InGameState : public IGameState
{
  public:
    explicit InGameState(StateContext& ctx);
    ~InGameState() override;

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

    void setInventoryBackground(void* tex, int w, int h) { inventory_.setBackground(tex, w, h); }

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup)
    {
        inventory_.setTextureLookup(lookup);
        characterStats_.setTextureLookup(lookup);
        spellbook_.setTextureLookup(lookup);
        restWidget_.setTextureLookup(lookup);
        mapWidget_.setTextureLookup(lookup);
        hud_.setTextureLookup(lookup);
    }

  private:
    graphics::Rect worldViewportRect() const;
    bool mapMouseToWorldViewport(int screenX, int screenY, int& localX, int& localY) const;
    void updateCameraInput(float deltaMs);
    void renderOverlay();
    int pickMonsterUnderCursor() const;
    std::optional<graphics::PickHit> pickMapObjectUnderCursor(bool requireEventId) const;
    int pickMapEventUnderCursor() const;
    int findActivePartyMember() const;
    int findFirstDamageSpell(int characterIndex) const;
    void preserveCurrentMapStateForSave();
    bool quickSaveToSlot(int slotIndex);
    bool quickLoadFromSlot(int slotIndex);

    StateContext& ctx;

    MapRenderOptions renderOptions;
    bool showGrid = false;
    bool showAxes = true;
    bool showHelpOverlay = false;
    ui::DialogueWindow dialogue_;
    ui::HUD hud_;
    ui::InventoryWidget inventory_;
    ui::CharacterStatsWidget characterStats_;
    ui::SpellbookWidget spellbook_;
    ui::RestWidget restWidget_;
    ui::ShopWindow shopWindow_;
    ui::MapWidget mapWidget_;
    uint64_t lastUpdateTicks_ = 0;
    float fpsAccumulatorMs_ = 0.0f;
    int fpsFrameCounter_ = 0;
    float displayedFps_ = 0.0f;
    int selectedMonsterIndex_ = -1;
    int hoveredEventId_ = 0;
    std::optional<graphics::PickHit> hoveredPick_;
    std::string hoverLine_;
    std::string statusLine_;
};

} // namespace runeharbor::engine
