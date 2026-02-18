// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "igame_state.hpp"
#include "state_context.hpp"

namespace runeharbor::engine
{

class CharacterCreationState : public IGameState
{
  public:
    explicit CharacterCreationState(StateContext& ctx);
    ~CharacterCreationState() override;

    // Non-owning texture references (set by Application after loading assets)
    void setBackground(void* tex, int w, int h);
    void setFallbackBackground(void* tex, int w, int h);
    void setPortraitTexture(int index, void* tex, int w, int h);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    int calculateBonusPointsRemaining() const;
    void updateCharacterForFace(Character& ch);
    void updateSkillsForClass(Character& ch);

    StateContext& ctx;

    int activeCharacterIndex = 0;
    int menuRowIndex = 0;
    bool isNaming = false;
    static constexpr int kRowCount = 10;

    // Textures (non-owning)
    void* background = nullptr;
    int backgroundWidth = 0;
    int backgroundHeight = 0;
    void* fallbackBackground = nullptr;
    int fallbackWidth = 0;
    int fallbackHeight = 0;

    static constexpr int kPortraitCount = 20;
    void* portraitTextures[kPortraitCount] = {};
    int portraitWidths[kPortraitCount] = {};
    int portraitHeights[kPortraitCount] = {};
};

} // namespace runeharbor::engine
