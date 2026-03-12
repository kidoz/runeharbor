// SPDX-License-Identifier: MIT
#pragma once

#include "../../graphics/primitives.hpp"
#include "../../graphics/visibility.hpp"
#include "../../ui/dialogue.hpp"
#include "../../ui/hud.hpp"
#include "../../ui/inventory_widget.hpp"
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

  private:
    graphics::Rect worldViewportRect() const;
    bool mapMouseToWorldViewport(int screenX, int screenY, int& localX, int& localY) const;
    void updateCameraInput();
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
    bool showHelpOverlay = true;
    ui::DialogueWindow dialogue_;
    ui::HUD hud_;
    ui::InventoryWidget inventory_;
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
