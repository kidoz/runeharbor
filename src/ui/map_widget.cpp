// SPDX-License-Identifier: MIT
#include "map_widget.hpp"

#include "../engine/map_scene.hpp"
#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

MapWidget::MapWidget() {}

void* MapWidget::getCachedTexture(const std::string& name, int& w, int& h)
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

void MapWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
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
        renderer.drawFilledRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 10, 20, 30,
                                240);
        renderer.drawRect(bounds_.x, bounds_.y, bounds_.width, bounds_.height, 40, 80, 100, 255);
    }

    if (!gameWorld_)
        return;

    auto& party = gameWorld_->party();

    int textX = bounds_.x + 20;
    int textY = bounds_.y + 20;

    text.drawText(sdl, textX, textY, 2, 255, 255, 255, "Automap");
    textY += 40;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Current Map: ") + gameWorld_->currentMap());
    textY += 30;

    text.drawText(sdl, textX, textY, 1, 200, 200, 200,
                  std::string("Party Position: ") +
                      std::to_string(static_cast<int>(party.worldX())) + ", " +
                      std::to_string(static_cast<int>(party.worldY())) + ", " +
                      std::to_string(static_cast<int>(party.worldZ())));
    textY += 20;

    if (mapScene_)
    {
        // For now just draw a simple blip for the party in the middle, and bounding box.
        // Full map rendering will come in a polish pass.
        int mapCenterX = bounds_.x + bounds_.width / 2;
        int mapCenterY = bounds_.y + bounds_.height / 2;

        renderer.drawFilledRect(mapCenterX - 4, mapCenterY - 4, 8, 8, 255, 0, 0, 255);
        text.drawText(sdl, mapCenterX + 10, mapCenterY - 10, 1, 255, 0, 0, "Party");
    }
}

bool MapWidget::handleEvent(const UIEvent& event)
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
