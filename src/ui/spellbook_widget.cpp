// SPDX-License-Identifier: MIT
#include "spellbook_widget.hpp"

#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <format>

#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

SpellbookWidget::SpellbookWidget() {}

void* SpellbookWidget::getCachedTexture(const std::string& name, int& w, int& h)
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

void SpellbookWidget::requestCast(int spellId)
{
    if (spellId <= 0 || spellSystem_ == nullptr)
    {
        return;
    }

    // Gate: the active character must be able to cast (skill + mana).
    if (!spellSystem_->canCast(activeCharacterIndex_, spellId))
    {
        if (onStatus_)
            onStatus_("Cannot cast — insufficient skill or mana.");
        return;
    }

    game::SpellTarget target = game::SpellTarget::SingleEnemy;
    if (const auto* info = spellSystem_->getSpell(spellId))
    {
        target = info->target;
    }

    if (onCastRequest_)
    {
        onCastRequest_({spellId, target});
    }
}

void SpellbookWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
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
                                240);
        renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 130, 108, 60, 255);
    }

    if (!gameWorld_ || activeCharacterIndex_ < 0 || activeCharacterIndex_ >= game::kPartySize)
        return;

    auto& party = gameWorld_->party();
    auto& character = party.member(activeCharacterIndex_);

    int textX = bounds_.x + 20;
    int textY = bounds_.y + 16;

    text.drawText(sdl, textX, textY, 2, 255, 220, 120, character.name + "'s Spellbook");
    textY += 36;

    // Build the spell list from the character's available spells.
    rows_.clear();
    rowY_.clear();
    if (spellSystem_ != nullptr)
    {
        const auto spellIds = spellSystem_->getAvailableSpells(activeCharacterIndex_);
        for (int id : spellIds)
        {
            const auto* info = spellSystem_->getSpell(id);
            if (info == nullptr)
                continue;
            SpellRow row;
            row.id = id;
            row.name = info->name.empty() ? info->shortName : info->name;
            row.manaCost = spellSystem_->getManaCost(activeCharacterIndex_, id);
            row.target = info->target;
            rows_.push_back(std::move(row));
        }
    }

    if (rows_.empty())
    {
        text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                      "No spells known. Read a spell book to learn one.");
        return;
    }

    // Header.
    text.drawText(sdl, textX, textY, 1, 200, 200, 160, "Spell                              Mana");
    textY += rowHeight_ + 2;
    renderer.drawFilledRect(bounds_.x + 16, textY - 2, bounds_.width - 32, 1, 110, 90, 50, 220);
    textY += 4;

    if (selected_ >= static_cast<int>(rows_.size()))
        selected_ = 0;

    for (int i = 0; i < static_cast<int>(rows_.size()); i++)
    {
        if (textY + rowHeight_ > bounds_.y + bounds_.height - 24)
            break; // out of space
        const auto& row = rows_[i];
        if (i == selected_)
        {
            renderer.drawFilledRect(bounds_.x + 16, textY - 1, bounds_.width - 32, rowHeight_, 80,
                                    70, 30, 220);
        }
        const std::string cost = std::format("{}", row.manaCost);
        text.drawText(sdl, textX, textY, 1, 230, 230, 230, row.name);
        // Right-align the mana cost, measured (not a fixed 8px glyph advance).
        const int costW = text.measureTextWidth(cost, 1);
        text.drawText(sdl, bounds_.x + bounds_.width - 24 - costW, textY, 1, 120, 180, 255, cost);
        rowY_.push_back(textY);
        textY += rowHeight_;
    }

    // Footer hint.
    const int hintY = bounds_.y + bounds_.height - 18;
    text.drawText(sdl, textX, hintY, 1, 160, 160, 170, "Up/Dn: select   Enter: cast   Esc: close");
}

bool SpellbookWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;
    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        // Click a row to select + cast.
        for (size_t i = 0; i < rowY_.size(); i++)
        {
            if (event.mouseY >= rowY_[i] && event.mouseY < rowY_[i] + rowHeight_)
            {
                selected_ = static_cast<int>(i);
                if (i < rows_.size())
                    requestCast(rows_[i].id);
                return true;
            }
        }
        return true; // consume clicks inside the panel
    }
    if (event.type == UIEventType::KeyDown)
    {
        if (event.scancode == SDL_SCANCODE_UP)
        {
            if (selected_ > 0)
                selected_--;
            return true;
        }
        if (event.scancode == SDL_SCANCODE_DOWN)
        {
            if (selected_ < static_cast<int>(rows_.size()) - 1)
                selected_++;
            return true;
        }
        if ((event.scancode == SDL_SCANCODE_RETURN || event.scancode == SDL_SCANCODE_SPACE) &&
            selected_ < static_cast<int>(rows_.size()))
        {
            requestCast(rows_[selected_].id);
            return true;
        }
        if (event.scancode == SDL_SCANCODE_ESCAPE)
        {
            setVisible(false);
            return true;
        }
        // Assign selected spell to quickbar slot 1 or 2 (RE: per-character
        // quickbar bytes +0x1A4E/+0x1A4F).
        if ((event.scancode == SDL_SCANCODE_1 || event.scancode == SDL_SCANCODE_2) &&
            selected_ >= 0 && selected_ < static_cast<int>(rows_.size()) && gameWorld_)
        {
            const int slot = (event.scancode == SDL_SCANCODE_1) ? 0 : 1;
            gameWorld_->party()
                .member(activeCharacterIndex_)
                .setQuickbarSpell(slot, rows_[selected_].id);
            if (onStatus_)
            {
                onStatus_(std::format("Assigned {} to quickbar slot {}.", rows_[selected_].name,
                                      slot + 1));
            }
            return true;
        }
    }
    return false;
}

} // namespace runeharbor::ui
