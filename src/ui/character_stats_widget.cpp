// SPDX-License-Identifier: MIT
#include "character_stats_widget.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <format>
#include <vector>

#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

namespace
{
constexpr int kTabYOffset = 36;
constexpr int kTabWidth = 84;
constexpr int kTabHeight = 22;
constexpr int kTabGap = 6;
constexpr int kContentOffset = 70;
constexpr int kRowHeight = 16;

uint8_t affordabilityChannel(int have, int need, uint8_t affordable, uint8_t missing)
{
    return have >= need ? affordable : missing;
}
} // namespace

CharacterStatsWidget::CharacterStatsWidget() {}

void CharacterStatsWidget::setTab(Tab tab)
{
    if (tab_ == tab)
    {
        return;
    }
    tab_ = tab;
    skillSelection_ = 0;
    skillScroll_ = 0;
}

void* CharacterStatsWidget::getCachedTexture(const std::string& name, int& w, int& h)
{
    if (name.empty())
        return nullptr;

    auto it = textureCache_.find(name);
    if (it != textureCache_.end())
    {
        w = it->second.w;
        h = it->second.h;
        return it->second.tex;
    }

    if (textureLookup_)
    {
        void* tex = textureLookup_(name, w, h);
        textureCache_[name] = {tex, w, h};
        return tex;
    }

    return nullptr;
}

std::vector<game::SkillId> CharacterStatsWidget::learnedSkills() const
{
    std::vector<game::SkillId> result;
    if (!gameWorld_ || activeCharacterIndex_ < 0 || activeCharacterIndex_ >= game::kPartySize)
    {
        return result;
    }

    const auto& character = gameWorld_->party().member(activeCharacterIndex_);
    result.reserve(character.skillLevels.size());
    for (size_t i = 0; i < character.skillLevels.size(); ++i)
    {
        if (character.skillLevels[i].learned())
        {
            result.push_back(static_cast<game::SkillId>(i));
        }
    }
    return result;
}

void CharacterStatsWidget::clampSkillSelection(int count)
{
    if (count <= 0)
    {
        skillSelection_ = 0;
        skillScroll_ = 0;
        return;
    }
    skillSelection_ = std::clamp(skillSelection_, 0, count - 1);
    skillScroll_ = std::clamp(skillScroll_, 0, std::max(0, count - 1));
}

bool CharacterStatsWidget::raiseSelectedSkill()
{
    if (!gameWorld_ || activeCharacterIndex_ < 0 || activeCharacterIndex_ >= game::kPartySize)
    {
        return false;
    }

    auto& character = gameWorld_->party().member(activeCharacterIndex_);
    const std::vector<game::SkillId> skills = learnedSkills();
    if (skills.empty())
    {
        return false;
    }
    clampSkillSelection(static_cast<int>(skills.size()));

    const game::SkillId skill = skills[static_cast<size_t>(skillSelection_)];
    const auto& value = character.skillLevels[static_cast<size_t>(skill)];

    if (character.raiseSkill(skill))
    {
        if (onStatus_)
        {
            const auto& updated = character.skillLevels[static_cast<size_t>(skill)];
            onStatus_(std::format("{} raised to level {} - {} skill points left.",
                                  game::skillDisplayName(skill), updated.level,
                                  character.skillPoints));
        }
        return true;
    }

    if (onStatus_)
    {
        if (value.level >= game::Character::kMaxSkillLevel)
        {
            onStatus_(std::format("{} is already at level {}.", game::skillDisplayName(skill),
                                  game::Character::kMaxSkillLevel));
        }
        else
        {
            const int cost = character.skillUpgradeCost(skill);
            onStatus_(std::format("Not enough skill points: {} needs {}, you have {}.",
                                  game::skillDisplayName(skill), cost, character.skillPoints));
        }
    }
    return false;
}

void CharacterStatsWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    if (!visible_)
        return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    if (bgTexture_)
    {
        graphics::Rect src = {0, 0, bgW_, bgH_};
        graphics::Rect dst = {bounds_.x, bounds_.y, bounds_.width, bounds_.height};
        SDL_FRect sdlSrc = {static_cast<float>(src.x), static_cast<float>(src.y),
                            static_cast<float>(src.width), static_cast<float>(src.height)};
        SDL_FRect sdlDst = {static_cast<float>(dst.x), static_cast<float>(dst.y),
                            static_cast<float>(dst.width), static_cast<float>(dst.height)};
        SDL_RenderTexture(sdl, static_cast<SDL_Texture*>(bgTexture_), &sdlSrc, &sdlDst);
    }
    else
    {
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 40, 30, 20,
                                255);
    }

    if (!gameWorld_ || activeCharacterIndex_ < 0 || activeCharacterIndex_ >= game::kPartySize)
        return;

    auto& party = gameWorld_->party();
    auto& character = party.member(activeCharacterIndex_);

    text.drawText(sdl, bounds_.x + 16, bounds_.y + 8, 2, 255, 255, 255, character.name);

    // Tab strip: Stats | Skills. These replace the inventory art's baked-in
    // tab row when the character sheet is opened from the CHARS HUD button.
    const int tabY = bounds_.y + kTabYOffset;
    const int statsTabX = bounds_.x + 16;
    const int skillsTabX = statsTabX + kTabWidth + kTabGap;
    const auto drawTab = [&](int x, const char* label, bool active)
    {
        const uint8_t r = active ? 120 : 70;
        const uint8_t g = active ? 95 : 62;
        const uint8_t b = active ? 45 : 40;
        renderer.drawFilledRect(x, tabY, kTabWidth, kTabHeight, r, g, b, 235);
        renderer.drawRect(x, tabY, kTabWidth, kTabHeight, active ? 220 : 130, 200, 150, 255);
        text.drawText(sdl, x + 8, tabY + 5, 1, 245, 235, 200, label);
    };
    drawTab(statsTabX, "Stats", tab_ == Tab::Stats);
    drawTab(skillsTabX, "Skills", tab_ == Tab::Skills);

    if (tab_ == Tab::Skills)
    {
        renderSkillsTab(renderer, text, sdl);
    }
    else
    {
        renderStatsTab(renderer, text, sdl);
    }
}

void CharacterStatsWidget::renderStatsTab(graphics::IRenderer&, const graphics::DebugText& text,
                                          SDL_Renderer* sdl)
{
    auto& character = gameWorld_->party().member(activeCharacterIndex_);

    int textX = bounds_.x + 20;
    int textY = bounds_.y + kContentOffset;

    const auto line = [&](std::string value)
    {
        text.drawText(sdl, textX, textY, 1, 210, 210, 210, value);
        textY += 18;
    };

    line(std::format("Level: {}   XP: {} / {}", character.level, character.experience,
                     character.xpRequiredForNextLevel()));
    line(std::format("HP: {} / {}   SP: {} / {}", character.hitPoints, character.maxHitPoints,
                     character.spellPoints, character.maxSpellPoints));
    line(std::format("Armor Class: {}   Skill Points: {}", character.armorClass,
                     character.skillPoints));
    textY += 4;

    line(std::format("Might: {}   Intellect: {}   Personality: {}", character.stats.might,
                     character.stats.intellect, character.stats.personality));
    line(std::format("Endurance: {}   Accuracy: {}   Speed: {}   Luck: {}",
                     character.stats.endurance, character.stats.accuracy, character.stats.speed,
                     character.stats.luck));
    textY += 4;

    line(std::format("Resistances: Fire {}  Air {}  Water {}  Earth {}  Mind {}  Body {}",
                     character.effectiveResistance(0), character.effectiveResistance(1),
                     character.effectiveResistance(2), character.effectiveResistance(3),
                     character.effectiveResistance(4), character.effectiveResistance(5)));
    textY += 4;

    const game::ConditionIndex worst = character.worstActiveCondition();
    line(std::format("Condition: {}",
                     worst == game::ConditionIndex::Count ? "Healthy" : "Afflicted"));

    const int hintY = bounds_.y + bounds_.height - 20;
    text.drawText(sdl, bounds_.x + 16, hintY, 1, 160, 160, 170,
                  "Skills tab: spend skill points   Esc: close");
}

void CharacterStatsWidget::renderSkillsTab(graphics::IRenderer& renderer,
                                           const graphics::DebugText& text, SDL_Renderer* sdl)
{
    auto& character = gameWorld_->party().member(activeCharacterIndex_);
    const std::vector<game::SkillId> skills = learnedSkills();
    clampSkillSelection(static_cast<int>(skills.size()));

    const int textX = bounds_.x + 20;
    const int listY = bounds_.y + kContentOffset;
    const int bottomY = bounds_.y + bounds_.height - 24;
    const int visibleRows = std::max(1, (bottomY - listY) / kRowHeight);
    const int maxScroll = std::max(0, static_cast<int>(skills.size()) - visibleRows);

    if (skillSelection_ < skillScroll_)
    {
        skillScroll_ = skillSelection_;
    }
    if (skillSelection_ >= skillScroll_ + visibleRows)
    {
        skillScroll_ = skillSelection_ - visibleRows + 1;
    }
    skillScroll_ = std::clamp(skillScroll_, 0, maxScroll);

    text.drawText(sdl, textX, listY, 1, 255, 220, 120,
                  std::format("Skill Points: {}", character.skillPoints));

    skillRowRects_.clear();
    const int rowY0 = listY + 22;

    if (skills.empty())
    {
        text.drawText(sdl, textX, rowY0, 1, 190, 190, 190, "No skills learned yet.");
        return;
    }

    for (int row = 0; row < visibleRows; ++row)
    {
        const int index = skillScroll_ + row;
        if (index >= static_cast<int>(skills.size()))
        {
            break;
        }

        const game::SkillId skill = skills[static_cast<size_t>(index)];
        const auto& value = character.skillLevels[static_cast<size_t>(skill)];
        const int cost = character.skillUpgradeCost(skill);
        const bool selected = index == skillSelection_;
        const bool maxed = value.level >= game::Character::kMaxSkillLevel;

        const int y = rowY0 + row * kRowHeight;
        if (selected)
        {
            renderer.drawFilledRect(bounds_.x + 14, y - 1, bounds_.width - 28, kRowHeight, 70, 60,
                                    30, 220);
        }

        const std::string line =
            std::format("{:<14}{:<11}Lvl {:>2}   {}", game::skillDisplayName(skill),
                        game::masteryName(value.mastery), value.level,
                        maxed ? std::string("max") : std::format("next: {} pts", cost));

        const uint8_t r = selected ? 255 : 230;
        const uint8_t g =
            selected ? 235 : affordabilityChannel(character.skillPoints, cost, 235, 170);
        const uint8_t b =
            selected ? 190 : affordabilityChannel(character.skillPoints, cost, 180, 150);
        text.drawText(sdl, textX, y, 1, r, g, b, line);

        skillRowRects_.push_back({skill, bounds_.x + 14, y - 1, bounds_.width - 28, kRowHeight});
    }

    if (maxScroll > 0)
    {
        text.drawText(
            sdl, bounds_.x + bounds_.width - 150, listY, 1, 160, 160, 170,
            std::format("{}-{} of {}", skillScroll_ + 1,
                        std::min(static_cast<int>(skills.size()), skillScroll_ + visibleRows),
                        skills.size()));
    }

    const int hintY = bounds_.y + bounds_.height - 20;
    text.drawText(sdl, bounds_.x + 16, hintY, 1, 160, 160, 170,
                  "Click a skill, or Up/Down + Enter: spend points   Tab: stats   Esc: close");
}

bool CharacterStatsWidget::handleTabClick(int mouseX, int mouseY)
{
    const int tabY = bounds_.y + kTabYOffset;
    const int statsTabX = bounds_.x + 16;
    const int skillsTabX = statsTabX + kTabWidth + kTabGap;

    if (mouseY < tabY || mouseY >= tabY + kTabHeight)
    {
        return false;
    }
    if (mouseX >= statsTabX && mouseX < statsTabX + kTabWidth)
    {
        setTab(Tab::Stats);
        return true;
    }
    if (mouseX >= skillsTabX && mouseX < skillsTabX + kTabWidth)
    {
        setTab(Tab::Skills);
        return true;
    }
    return false;
}

bool CharacterStatsWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    if (event.type == UIEventType::KeyDown)
    {
        if (event.scancode == SDL_SCANCODE_TAB)
        {
            setTab(tab_ == Tab::Stats ? Tab::Skills : Tab::Stats);
            return true;
        }
        if (tab_ != Tab::Skills)
        {
            return false;
        }

        const std::vector<game::SkillId> skills = learnedSkills();
        const int count = static_cast<int>(skills.size());
        clampSkillSelection(count);

        if (event.scancode == SDL_SCANCODE_DOWN)
        {
            if (count > 0 && skillSelection_ < count - 1)
                ++skillSelection_;
            return true;
        }
        if (event.scancode == SDL_SCANCODE_UP)
        {
            if (skillSelection_ > 0)
                --skillSelection_;
            return true;
        }
        if (event.scancode == SDL_SCANCODE_RETURN || event.scancode == SDL_SCANCODE_SPACE)
        {
            (void)raiseSelectedSkill();
            return true;
        }
        return false;
    }

    if (event.type != UIEventType::MouseDown)
    {
        return false;
    }

    if (!bounds_.contains(event.mouseX, event.mouseY))
    {
        return false;
    }

    if (handleTabClick(event.mouseX, event.mouseY))
    {
        return true;
    }

    if (tab_ == Tab::Skills)
    {
        for (size_t i = 0; i < skillRowRects_.size(); ++i)
        {
            const auto& row = skillRowRects_[i];
            if (event.mouseX >= row.x && event.mouseX < row.x + row.w && event.mouseY >= row.y &&
                event.mouseY < row.y + row.h)
            {
                // Rows are drawn from skillScroll_, so their visible ordinal
                // maps directly to the selected skill.
                skillSelection_ = skillScroll_ + static_cast<int>(i);
                (void)raiseSelectedSkill();
                return true;
            }
        }
    }

    return true; // modal: swallow clicks inside the panel
}

} // namespace runeharbor::ui
