// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../game/save_game.hpp"
#include "widgets.hpp"

namespace runeharbor::ui
{

/// Modal save/load slot picker. Mirrors the original save screen's page of ten
/// slots: Up/Down move by one, PageUp/PageDown by ten, number keys jump within
/// the page, Enter confirms, Esc cancels. In Load mode empty slots cannot be
/// selected for loading.
class SaveLoadWidget : public Widget
{
  public:
    enum class Mode : uint8_t
    {
        Save = 0,
        Load = 1,
    };

    enum class ActionType : uint8_t
    {
        None = 0,
        Save,
        Load,
    };

    struct Action
    {
        ActionType type = ActionType::None;
        int slotIndex = -1;
    };

    void setSaveGame(game::SaveGame* saveGame) { saveGame_ = saveGame; }
    void open(Mode mode);
    Mode mode() const { return mode_; }
    void refresh();

    /// Returns the pending action (if any) and clears it. The caller owns the
    /// actual save/load work so map-state capture stays in InGameState.
    Action takeAction();

    void render(graphics::IRenderer& renderer, const graphics::DebugText& text) override;
    bool handleEvent(const UIEvent& event) override;

    // Introspection for tests.
    int selectedSlot() const { return selected_; }
    static constexpr int kVisibleRows = 10;
    bool slotExists(int index) const;

  private:
    void moveSelection(int delta);
    void confirmSelection();
    int entryCount() const { return static_cast<int>(slots_.size()); }
    int pageStart() const;
    std::string statusMessage_;

    game::SaveGame* saveGame_ = nullptr;
    Mode mode_ = Mode::Save;
    std::vector<game::SaveSlotInfo> slots_;
    int selected_ = 0;
    Action pending_;
    // Save mode requires a second Enter before overwriting an occupied slot.
    bool overwriteArmed_ = false;

    std::vector<graphics::Rect> rowRects_;
    graphics::Rect confirmRect_;
    graphics::Rect cancelRect_;
};

} // namespace runeharbor::ui
