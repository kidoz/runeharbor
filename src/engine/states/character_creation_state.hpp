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
    void setSkyHeader(void* tex, int w, int h);
    void setTitleHeader(void* tex, int w, int h);

    // Overlay texture setters
    void setFaceMask(void* tex, int w, int h);
    void setOkButton(void* tex, int w, int h);
    void setClearButton(void* tex, int w, int h);
    void setMinusButton(void* tex, int w, int h);
    void setPlusButton(void* tex, int w, int h);
    void setLeftArrow(void* tex, int w, int h);
    void setRightArrow(void* tex, int w, int h);
    void setClassIcon(int index, void* tex, int w, int h);

    void enter() override;
    void exit() override;
    std::optional<GameStateId> update() override;
    void render() override;

  private:
    int calculateBonusPointsRemaining() const;
    void updateCharacterForFace(Character& ch);
    void updateSkillsForClass(Character& ch);
    void rebuildAvailableSkills();

    StateContext& ctx;

    int activeCharacterIndex = 0;
    int menuRowIndex = 0;
    bool isNaming = false;
    static constexpr int kRowCount = 10;

    // Additional skill selection (2 per character)
    static constexpr int kMaxExtraSkills = 2;
    struct AvailableSkill
    {
        game::SkillId id;
        bool selected;
    };
    std::vector<AvailableSkill> availableSkills; // for active character's class

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

    // Overlay textures (non-owning, owned by Application)
    struct TexRef
    {
        void* tex = nullptr;
        int w = 0, h = 0;
    };
    TexRef faceMask;
    TexRef skyHeader;
    TexRef titleHeader;
    TexRef okButton;
    TexRef clearButton;
    TexRef minusButton;
    TexRef plusButton;
    TexRef leftArrow;
    TexRef rightArrow;

    static constexpr int kClassIconCount = 9;
    TexRef classIcons[kClassIconCount] = {};
};

} // namespace runeharbor::engine
