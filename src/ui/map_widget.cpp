// SPDX-License-Identifier: MIT
#include "map_widget.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

#include <cmath>

#include "../engine/map_scene.hpp"
#include "../formats/blv_map.hpp"
#include "../formats/odm_map.hpp"
#include "../game/game_world.hpp"
#include "../game/party.hpp"
#include "../graphics/debug_text.hpp"
#include "../graphics/irenderer.hpp"
#include "../graphics/math3d.hpp"
#include "../graphics/primitives.hpp"

namespace runeharbor::ui
{

MapWidget::MapWidget() {}

namespace
{

// World bounds for the current map's 2D (X,Y) extent — duplicates the HUD's
// MinimapBounds logic (hud.cpp) so the automap and minimap agree on framing.
// Kept local rather than factored to a shared header per scope.
struct MapBounds2D
{
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
    bool valid = false;
};

MapBounds2D buildMapBounds(const engine::MapScene* mapScene)
{
    MapBounds2D b;
    if (!mapScene)
    {
        return b;
    }

    auto extend = [&](float x, float y)
    {
        if (!b.valid)
        {
            b.minX = b.maxX = x;
            b.minY = b.maxY = y;
            b.valid = true;
            return;
        }
        b.minX = std::min(b.minX, x);
        b.maxX = std::max(b.maxX, x);
        b.minY = std::min(b.minY, y);
        b.maxY = std::max(b.maxY, y);
    };

    const auto& blv = mapScene->getBLVData();
    for (const auto& v : blv.vertices)
    {
        extend(static_cast<float>(v.x), static_cast<float>(v.y));
    }

    const auto& odm = mapScene->getODMData();
    if (!odm.heightmap.empty())
    {
        constexpr float kCell = 512.0f;
        const float half = static_cast<float>(formats::ODMMapData::TERRAIN_SIZE) * 0.5f;
        extend((-half) * kCell, (-half) * kCell);
        extend((half - 1.0f) * kCell, (half - 1.0f) * kCell);
    }
    for (const auto& building : odm.buildings)
    {
        extend(static_cast<float>(building.worldX + building.minX),
               static_cast<float>(building.worldY + building.minY));
        extend(static_cast<float>(building.worldX + building.maxX),
               static_cast<float>(building.worldY + building.maxY));
    }

    if (b.valid)
    {
        if (std::abs(b.maxX - b.minX) < 1.0f)
            b.maxX = b.minX + 1.0f;
        if (std::abs(b.maxY - b.minY) < 1.0f)
            b.maxY = b.minY + 1.0f;
    }
    return b;
}

float normalizeToUnit(float value, float min, float max)
{
    if (max <= min)
    {
        return 0.5f;
    }
    return std::clamp((value - min) / (max - min), 0.0f, 1.0f);
}

} // namespace

void MapWidget::syncExploredFromWorld()
{
    exploredSectors_.clear();
    if (!gameWorld_ || !mapScene_)
    {
        return;
    }
    const std::string& mapName = mapScene_->getName();
    exploredMapName_ = mapName;
    const auto* saved = gameWorld_->getSavedMapState(mapName);
    if (saved)
    {
        for (uint16_t s : saved->exploredSectors)
        {
            exploredSectors_.insert(s);
        }
    }
}

void MapWidget::syncExploredToWorld()
{
    if (!gameWorld_ || exploredMapName_.empty() || exploredSectors_.empty())
    {
        return;
    }
    // Merge the live set into the SavedMapState (non-destructive: preserves any
    // previously-stored sectors, e.g. from an earlier visit stored by the host).
    auto* saved = gameWorld_->getSavedMapState(exploredMapName_);
    game::SavedMapState state = saved ? *saved : game::SavedMapState{};
    for (uint16_t s : exploredSectors_)
    {
        if (std::find(state.exploredSectors.begin(), state.exploredSectors.end(), s) ==
            state.exploredSectors.end())
        {
            state.exploredSectors.push_back(s);
        }
    }
    gameWorld_->setSavedMapState(exploredMapName_, std::move(state));
}

void MapWidget::markCurrentSectorExplored()
{
    if (!gameWorld_ || !mapScene_)
    {
        return;
    }
    // On a map change, reload the explored set from the world's saved state for
    // the new map (carries over previously-explored sectors from earlier visits
    // and save loads). Done here so render() doesn't need explicit wiring per
    // frame — only reloads when the map actually changes.
    const std::string& mapName = mapScene_->getName();
    if (mapName != exploredMapName_)
    {
        syncExploredFromWorld();
    }
    // Indoor only: find the party's sector and add it to the explored set.
    if (mapScene_->getBLVData().vertices.empty())
    {
        return;
    }
    const auto& party = gameWorld_->party();
    const graphics::Vec3 pos(party.worldX(), party.worldY(), party.worldZ());
    const int sector = visibility_.findCameraSector(mapScene_->getBLVData(), pos);
    if (sector >= 0)
    {
        exploredSectors_.insert(static_cast<uint16_t>(sector));
    }
}

void MapWidget::render(graphics::IRenderer& renderer, const graphics::DebugText& text)
{
    if (!visible_)
        return;

    SDL_Renderer* sdl = renderer.getSDLRenderer();
    if (!sdl)
        return;

    // Background panel.
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

    if (!gameWorld_ || !mapScene_)
        return;

    // Title.
    constexpr int kTitleH = 24;
    const int contentTop = bounds_.y + kTitleH;
    text.drawText(sdl, bounds_.x + 16, bounds_.y + 6, 2, 220, 200, 120, "Automap");
    text.drawText(sdl, bounds_.x + 160, bounds_.y + 10, 1, 180, 180, 180,
                  std::string(gameWorld_->currentMap()));

    // Map viewport (inset). All geometry is drawn in screen pixels directly —
    // the letterbox transform is already baked into bounds_ by InGameState.
    constexpr int kMargin = 24;
    const int mapX = bounds_.x + kMargin;
    const int mapY = contentTop + kMargin / 2;
    const int mapW = bounds_.width - kMargin * 2;
    const int mapH = bounds_.height - kMargin - kTitleH - kMargin / 2;
    if (mapW <= 4 || mapH <= 4)
        return;

    // Clip drawing to the map viewport so walls/blips don't spill into the panel.
    const SDL_Rect clipRect{mapX, mapY, mapW, mapH};
    SDL_SetRenderClipRect(sdl, &clipRect);

    renderer.drawFilledRect(mapX, mapY, mapW, mapH, 8, 12, 8, 255);
    renderer.drawRect(mapX, mapY, mapW, mapH, 50, 70, 50, 255);

    const MapBounds2D wb = buildMapBounds(mapScene_);
    if (!wb.valid)
    {
        SDL_SetRenderClipRect(sdl, nullptr);
        return;
    }

    // Project a world (x,y) to viewport pixels. World Y grows up, screen Y down.
    auto projX = [&](float wx)
    {
        return static_cast<float>(mapX +
                                  static_cast<int>(normalizeToUnit(wx, wb.minX, wb.maxX) * mapW));
    };
    auto projY = [&](float wy)
    {
        return static_cast<float>(
            mapY + static_cast<int>((1.0f - normalizeToUnit(wy, wb.minY, wb.maxY)) * mapH));
    };

    const auto& party = gameWorld_->party();

    // Track exploration as the map renders (indoor).
    markCurrentSectorExplored();

    const bool isIndoor = !mapScene_->getBLVData().vertices.empty();

    if (isIndoor)
    {
        const auto& blv = mapScene_->getBLVData();
        // Draw wall edges. Skip floors/ceilings/invisible. Fog-of-war: a face is
        // revealed if either of its sectors has been explored (portals connect
        // two sectors — reveal if either side is known).
        SDL_SetRenderDrawColor(sdl, 110, 130, 110, 255);
        for (const auto& face : blv.faces)
        {
            if (face.isInvisible() || face.isFloor() || face.isCeiling())
                continue;
            const bool revealed = exploredSectors_.empty() || // nothing explored yet → show all
                                  exploredSectors_.count(face.sectorId) ||
                                  exploredSectors_.count(face.otherSectorId);
            if (!revealed)
                continue;
            const auto& verts = face.vertexIndices;
            for (size_t i = 0; i < verts.size(); ++i)
            {
                const uint16_t a = verts[i];
                const uint16_t b = verts[(i + 1) % verts.size()];
                if (a >= blv.vertices.size() || b >= blv.vertices.size())
                    continue;
                const auto& va = blv.vertices[a];
                const auto& vb = blv.vertices[b];
                SDL_RenderLine(sdl, projX(static_cast<float>(va.x)),
                               projY(static_cast<float>(va.y)), projX(static_cast<float>(vb.x)),
                               projY(static_cast<float>(vb.y)));
            }
        }
    }
    else
    {
        // Outdoor: draw buildings as outlined rects.
        const auto& odm = mapScene_->getODMData();
        for (const auto& building : odm.buildings)
        {
            const float x1 = projX(static_cast<float>(building.worldX + building.minX));
            const float y1 = projY(static_cast<float>(building.worldY + building.minY));
            const float x2 = projX(static_cast<float>(building.worldX + building.maxX));
            const float y2 = projY(static_cast<float>(building.worldY + building.maxY));
            const SDL_FRect r = {std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1),
                                 std::abs(y2 - y1)};
            SDL_SetRenderDrawColor(sdl, 90, 70, 50, 200);
            SDL_RenderFillRect(sdl, &r);
            SDL_SetRenderDrawColor(sdl, 150, 120, 80, 255);
            SDL_RenderRect(sdl, &r);
        }
    }

    // Party blip + facing arrow.
    const float px = projX(party.worldX());
    const float py = projY(party.worldY());
    {
        const SDL_FRect dot = {px - 3.0f, py - 3.0f, 6.0f, 6.0f};
        SDL_SetRenderDrawColor(sdl, 255, 255, 100, 255);
        SDL_RenderFillRect(sdl, &dot);

        const float yawRad = party.yaw() * (3.14159265f / 2048.0f); // MM7 turn units 0..2047
        const float arrowLen = 12.0f;
        const float ax = px + std::cos(yawRad) * arrowLen;
        const float ay = py + std::sin(yawRad) * arrowLen;
        SDL_SetRenderDrawColor(sdl, 255, 255, 100, 255);
        SDL_RenderLine(sdl, px, py, ax, ay);
    }

    // Outdoor fog-of-war: dim everything outside a radius around the party.
    // Indoor fog is handled per-face above (explored sectors).
    if (!isIndoor)
    {
        const float radius = std::min(static_cast<float>(mapW), static_cast<float>(mapH)) * 0.18f;
        // Draw four dim rects around the reveal circle (approximate — a true
        // circular mask needs a texture; this gives a diamond-ish reveal).
        SDL_SetRenderDrawColor(sdl, 0, 0, 0, 170);
        // Top
        SDL_FRect t = {static_cast<float>(mapX), static_cast<float>(mapY), static_cast<float>(mapW),
                       std::max(0.0f, py - radius - static_cast<float>(mapY))};
        SDL_RenderFillRect(sdl, &t);
        // Bottom
        SDL_FRect bm = {static_cast<float>(mapX), py + radius, static_cast<float>(mapW),
                        std::max(0.0f, static_cast<float>(mapY + mapH) - (py + radius))};
        SDL_RenderFillRect(sdl, &bm);
        // Left
        SDL_FRect l = {static_cast<float>(mapX), static_cast<float>(mapY),
                       std::max(0.0f, px - radius - static_cast<float>(mapX)),
                       static_cast<float>(mapH)};
        SDL_RenderFillRect(sdl, &l);
        // Right
        SDL_FRect rR = {px + radius, static_cast<float>(mapY),
                        std::max(0.0f, static_cast<float>(mapX + mapW) - (px + radius)),
                        static_cast<float>(mapH)};
        SDL_RenderFillRect(sdl, &rR);
    }

    SDL_SetRenderClipRect(sdl, nullptr);

    // Footer hint.
    const int hintY = bounds_.y + bounds_.height - 16;
    text.drawText(sdl, bounds_.x + 16, hintY, 1, 140, 140, 160,
                  isIndoor ? "Indoor map — explored sectors revealed"
                           : "Outdoor map — area around party revealed");
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
