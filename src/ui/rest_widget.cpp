// SPDX-License-Identifier: MIT
#include "rest_widget.hpp"

#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

RestWidget::RestWidget() {}

void* RestWidget::getCachedTexture(const std::string& name, int& w, int& h)
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

void RestWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
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
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 30, 20, 10,
                                240);
        renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 80, 50, 20, 255);
    }

    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();

    int textX = bounds_.x + 20;
    int textY = bounds_.y + 20;

    text.drawText(sdl, textX, textY, 2, 255, 255, 255, "Camp / Rest");
    textY += 40;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  "Current Food: " + std::to_string(party.food()));
    textY += 30;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200, "Rest 8 hours (consume 1 food)");
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200, "Wait 5 minutes");
    textY += 20;
    text.drawText(sdl, textX, textY, 1, 200, 200, 200, "Wait 1 hour");
    textY += 20;
}

bool RestWidget::handleEvent(const UIEvent& event)
{
    if (!visible_ || !enabled_)
        return false;

    if (event.type == UIEventType::MouseDown && bounds_.contains(event.mouseX, event.mouseY))
    {
        // Rest 8 hours + random encounter check.
        if (gameWorld_)
        {
            auto& party = gameWorld_->party();
            if (party.rest(8))
            {
                if (party.checkRandomEncounter())
                {
                    if (onStatus_)
                        onStatus_("Ambush! Monsters attack during your rest!");
                    // Spawn a random monster near the party.
                    if (combatSystem_)
                    {
                        combatSystem_->spawnMonster(1, // basic monster id
                                                    party.worldX() + 512.0f,
                                                    party.worldY() + 512.0f, party.worldZ());
                        combatSystem_->setInCombat(true);
                    }
                }
                else
                {
                    if (onStatus_)
                        onStatus_("You rest peacefully. Party fully restored.");
                }
            }
            else if (onStatus_)
            {
                onStatus_("Not enough food to rest.");
            }
        }
        setVisible(false);
        return true;
    }
    return false;
}

} // namespace runeharbor::ui
