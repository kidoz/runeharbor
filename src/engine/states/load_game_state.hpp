// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../../game/save_game.hpp"
#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class LoadGameState : public IGameState
{
  public:
    explicit LoadGameState(StateContext& ctx);
    ~LoadGameState() override;

    void setBackground(void* tex, int w, int h);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    void refreshSlots();
    bool loadSelectedSlot();
    static std::string slotLabel(const game::SaveSlotInfo& slot);
    std::string entryLabel(int entryIndex) const;
    int totalEntries() const;

    StateContext& ctx;

    void* background_ = nullptr;
    int backgroundWidth_ = 0;
    int backgroundHeight_ = 0;

    std::vector<game::SaveSlotInfo> slots_;
    int selectedSlot_ = 0;
    bool autosaveExists_ = false;
    std::optional<game::SaveHeader> autosaveHeader_;
    std::string statusMessage_;
};

} // namespace runeharbor::engine
