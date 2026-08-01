// SPDX-License-Identifier: MIT
#include "character_stats_widget.hpp"

#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

CharacterStatsWidget::CharacterStatsWidget() {}

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

void CharacterStatsWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    // TODO: stub read-only dump. The real MM7 character sheet also shows
    // resistances, the skills table, equipment summary, conditions, age, and
    // armor class. handleEvent only swallows clicks (no interaction yet).
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
    }

    if (!gameWorld_ || activeCharacterIndex_ < 0)
        return;
    if (activeCharacterIndex_ >= game::kPartySize)
        return;

    auto& party = gameWorld_->party();
    auto& character = party.member(activeCharacterIndex_);

    int textX = bounds_.x + 20;
    int textY = bounds_.y + 20;

    text.drawText(sdl, textX, textY, 2, 255, 255, 255, character.name);
    textY += 30;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Level: ") + std::to_string(character.level));
    textY += 20;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("HP: ") + std::to_string(character.hitPoints) + " / " +
                      std::to_string(character.maxHitPoints));
    textY += 20;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("SP: ") + std::to_string(character.spellPoints) + " / " +
                      std::to_string(character.maxSpellPoints));
    textY += 30;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Might: ") + std::to_string(character.stats.might));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Intellect: ") + std::to_string(character.stats.intellect));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Personality: ") + std::to_string(character.stats.personality));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Endurance: ") + std::to_string(character.stats.endurance));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Accuracy: ") + std::to_string(character.stats.accuracy));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Speed: ") + std::to_string(character.stats.speed));
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Luck: ") + std::to_string(character.stats.luck));
    textY += 30;
}

bool CharacterStatsWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        return true;
    }
    return false;
}

} // namespace runeharbor::ui
