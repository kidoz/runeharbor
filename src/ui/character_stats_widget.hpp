// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../game/game_world.hpp"
#include "widgets.hpp"

struct SDL_Renderer;

namespace runeharbor::ui
{

class CharacterStatsWidget : public Widget
{
  public:
    // The MM7 character sheet's two main pages. Inventory/Awards have their own
    // panels; Stats and Skills share this widget.
    enum class Tab
    {
        Stats = 0,
        Skills = 1,
    };

    CharacterStatsWidget();

    using TextureLookup = std::function<void*(const std::string&, int& w, int& h)>;
    void setTextureLookup(TextureLookup lookup) { textureLookup_ = lookup; }

    void setGameWorld(game::GameWorld* world) { gameWorld_ = world; }
    void setActiveCharacter(int index) { activeCharacterIndex_ = index; }
    void setTab(Tab tab);
    Tab tab() const { return tab_; }

    // Status-line feedback (skill raised / not enough points).
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
    struct SkillRowRect
    {
        game::SkillId skill = game::SkillId::Staff;
        int x = 0, y = 0, w = 0, h = 0;
    };

    void* getCachedTexture(const std::string& name, int& w, int& h);
    void renderStatsTab(graphics::IRenderer& renderer, const graphics::DebugText& text,
                        SDL_Renderer* sdl);
    void renderSkillsTab(graphics::IRenderer& renderer, const graphics::DebugText& text,
                         SDL_Renderer* sdl);
    bool handleTabClick(int mouseX, int mouseY);
    bool raiseSelectedSkill();
    std::vector<game::SkillId> learnedSkills() const;
    void clampSkillSelection(int count);

    game::GameWorld* gameWorld_ = nullptr;
    int activeCharacterIndex_ = 0;
    TextureLookup textureLookup_;

    Tab tab_ = Tab::Stats;
    int skillSelection_ = 0;
    int skillScroll_ = 0;
    std::vector<SkillRowRect> skillRowRects_;
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
